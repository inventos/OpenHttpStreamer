#include "pool.hh"
#include "mp4file.hh"
#include "storage.hh"
#include "utility/os_except.hh"
#include "utility/logger.hh"
#include <boost/shared_ptr.hpp>
#include <boost/lexical_cast.hpp>
#include <unordered_map>

namespace { utility::logger::category lMp4File("mp4file"); }

using namespace utility::logger;

namespace mp4 {
    namespace {

        struct CacheElement;

        struct ParseTask : public PoolMixin<ParseTask> {
            boost::shared_ptr<Context> _ctx;
            boost::shared_ptr<DataSource> _src;
            boost::shared_ptr<FileReaderCallback> _reader;
            std::string _taskname;
            std::string _errmsg;
            bool _done;
            ::utility::utils::List2<ParseCallback> _callbacks;
            off_t _offset;
            ParseTask() : _done(false), _offset(0) {}
        };

        struct CacheElement : public ::utility::utils::Link2<CacheElement>, public PoolMixin<CacheElement> {
            ::boost::shared_ptr<ParseTask> _task;
        };

        ::utility::utils::List2<CacheElement> cache_queue;
        ::std::unordered_map< ::std::string, CacheElement * > cache_map;

#define CACHE_LIMIT 200

        unsigned cache_size = 0;


        void failure(::boost::shared_ptr<ParseTask> task, const ::std::string& errmsg) {
            task->_reader.reset();
            task->_ctx.reset();
            while ( !task->_callbacks.empty() ) {
                LOG(lMp4File, 0, Param("failure"), task->_src, Param("msg"), errmsg);
                task->_callbacks.pop_first()->_failure(errmsg);
            }
            task->_src.reset();
            task->_errmsg = errmsg;
            task->_done = true;
        }

        void finalize(::boost::shared_ptr<ParseTask> task) { 
            task->_reader.reset();
#if 0
            if ( pfile->_ctx->_video ) {
                COUT << "video:\n" << *pfile->_ctx->_video << "\n";
            }
            if ( pfile->_ctx->_audio ) {
                COUT << "audio:\n" << *pfile->_ctx->_audio << "\n";
            }
#endif
            task->_ctx->finalize();
#if 0
            for ( unsigned iii = 0; iii < pfile->_samples.size() && iii < 20; ++iii ) {
                CERR << pfile->_samples[iii] << "\n";
            }
#endif
            // std::cerr << "nsamples=" << task->_ctx->_samples.size() << "; sample#123828/offset=" << task->_ctx->_samples[123828]._offset << std::endl;

            // std::cerr << "; lastsample/offset=" << task->_ctx->_samples[task->_ctx->_samples.size() - 1]._offset << std::endl;
            while ( !task->_callbacks.empty() ) {
                LOG(lMp4File, 5, Param("success"), task->_src);
                task->_callbacks.pop_first()->_success(task->_ctx);
            }
            task->_src.reset();
            task->_done = true;
        }

        void read_for_parsing(boost::shared_ptr<ParseTask> task);
        void parse_continuation(::boost::shared_ptr<ParseTask> task, const ::boost::shared_ptr<FileDataChunk>& fdc) {
            assert ( fdc );
            if ( fdc->is_eof() ) {
                finalize(task);
            }
            else {
                task->_ctx->feed(fdc->data(), fdc->sz());
                task->_offset += fdc->sz();
                read_for_parsing(task);
            }
        }

        void read_for_parsing(boost::shared_ptr<ParseTask> task) {
            if ( task->_ctx->wants() ) {
                // CERR << "parsing: wants=" << pfile->_ctx->wants() << "\n";
                if ( unsigned s = task->_ctx->to_skip() ) {
                    // CERR << "parsing: to_skip=" << pfile->_ctx->to_skip() << "\n";
                    task->_offset += s;
                    // CERR << "parsing: offset=" << pfile->_offset << "\n";
                    task->_ctx->skip(0);
                }
                task->_reader.reset(new FileReaderCallback(::boost::bind(parse_continuation, task, _1),
                                                         ::boost::bind(finalize, task),
                                                         ::boost::bind(failure, task, _1)));
                task->_src->aread(task->_offset, 4096, task->_reader);
            }
            else {
                task->_reader.reset();
                finalize(task);
            }
        }
    }

    void a_parse_mp4(const boost::shared_ptr<DataSource>& src, ParseCallback *cb) {
        std::string key = boost::lexical_cast<std::string>(*src);
        auto iter = cache_map.find(key);
        if ( iter != cache_map.end() ) {
            CacheElement *ce = iter->second;
            if ( ce->_task->_done ) {
                if ( ce->_task->_errmsg.size() != 0 ) {
                    LOG(lMp4File, 4, Param("filename"), key, Param("instant_failure"), ce->_task->_errmsg);
                    cb->_failure(ce->_task->_errmsg);
                }
                else {
                    assert ( ce->_task->_ctx );
                    LOG(lMp4File, 4, Param("filename"), key, " calling right away");
                    cache_queue.append(ce);
                    cb->_success(ce->_task->_ctx);
                }
            }
            else {
                LOG(lMp4File, 4, Param("filename"), key, " appending callback");
                cache_queue.append(ce);
                ce->_task->_callbacks.append(cb);
            }
        }
        else {
            boost::shared_ptr<ParseTask> task(new ParseTask);
            task->_ctx.reset(new Context);
            task->_src = src;
            task->_callbacks.append(cb);
            task->_offset = 0;
            task->_taskname = key;
            CacheElement *ce;
            if ( cache_size == CACHE_LIMIT ) {
                ce = cache_queue.first();
                cache_map.erase(ce->_task->_taskname);
            }
            else {
                ce = new CacheElement;
                ++cache_size;
            }
            ce->_task = task;
            cache_map[key] = ce;
            cache_queue.append(ce);
            LOG(lMp4File, 4, Param("filename"), key, " scheduling parsing");
            read_for_parsing(task);
        }
    }

}    
