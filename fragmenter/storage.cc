#include "pool.hh"
#include "storage.hh"

#include "utility/os_except.hh"
#include "utility/utils/list2.hh"
#include "utility/async.hh"
#include "utility/logger.hh"
#include "utility/reactor.hh"

#include <boost/bind.hpp>
#include <boost/thread.hpp>
#include <boost/pool/singleton_pool.hpp>

#include <map>
#include <unordered_set>
#include <unordered_map>
#include <queue>

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>


namespace {
    ::utility::logger::category lStorage("storage");
}
using ::utility::logger::Param;


namespace {
    boost::pool<> filechunk_pool(sizeof(FileDataChunk));
}

void *FileDataChunk::operator new(size_t) { return filechunk_pool.malloc(); }
void FileDataChunk::operator delete(void *p) { if(p) filechunk_pool.free(p); }


void FileReaderCallback::failure(const ::std::string& w) {
    LOG(lStorage, 0, ::utility::logger::Param("failure"), w);
    if ( _failure ) {
        try {
            _failure(w);
        }
        catch ( ::std::exception& e ) {
            LOG(lStorage, 0, ::utility::logger::Param("double_failure"), e.what());
        }
        catch ( ... ) {
            LOG(lStorage, 0, ::utility::logger::Param("double_failure"), "!@#$!@#$");
        }
    }
}

void FileReaderCallback::success(const ::boost::shared_ptr<FileDataChunk>& p) {
    if ( _success ) {
        try {
            _success(p);
        }
        catch ( ::std::exception& e ) {
            failure(e.what());
        }
    }
}

void FileReaderCallback::eof() {
    if ( _eof ) {
        try {
            _eof();
        }
        catch ( ::std::exception& e ) {
            failure(e.what());
        }
    }
}

// XXX

namespace {

    struct FileDataSource : public DataSource, public PoolMixin<FileDataSource> {
        std::string _filename;
        int _fd;
        ::boost::mutex _mx;
        FileDataSource(const std::string& filename) : _filename(filename) {
            _fd = open(filename.c_str(), O_RDONLY);
            if ( _fd == -1 ) {
                throw ::utility::OSError(errno, ("opening " + filename).c_str());
            }
        }
        ~FileDataSource() {
            close(_fd);
        }
        virtual std::ostream& repr(std::ostream& o) const {
            return o << _filename;
        }
        virtual void aread(off_t offset, size_t sz, const boost::shared_ptr<FileReaderCallback>& cb);
        size_t read_bytes(char *buffer, size_t size, off_t offset) {
            ::boost::mutex::scoped_lock l(_mx);
            off_t told = lseek(_fd, offset, SEEK_SET);
            if ( told != offset ) {
                int e = errno;
                std::stringstream out; out << "seek error reading file " << _filename;
                LOG(lStorage, 0, Param("filename"), _filename, Param("syscall"), "seek", Param("offset"), offset, Param("tell"), told, Param("errno"), e, Param("message"), strerror(e));
                throw ::utility::OSError(e, out.str().c_str());
            }
            unsigned int total = 0;
            int retries = 5;
            while ( total < size ) {
                int bytes = read(_fd, buffer + total, size - total);
                if ( bytes == -1 ) {
                    int e = errno;
                    if ( ( e != EAGAIN && e != EINTR ) || retries-- != 0 ) {
                        int e = errno;
                        std::stringstream out; out << "reading file " << _filename;
                        LOG(lStorage, 0, Param("filename"), _filename, Param("syscall"), "read", Param("errno"), e, Param("message"), strerror(e));
                        throw ::utility::OSError(e, out.str().c_str());
                    }
                    else {
                        continue;
                    }
                }
                if ( bytes == 0 ) {
                    break;
                }
                total += bytes;
            }
            return total;
        }

    };

    struct FileDataAsyncTask : public ::utility::Async, public PoolMixin<FileDataAsyncTask> {
        boost::shared_ptr<FileDataSource> _source;
        off_t _offset;
        boost::weak_ptr<FileReaderCallback> _cb;
        boost::shared_ptr<FileDataChunk> _chunk;
        size_t _sz;
        FileDataAsyncTask(const boost::shared_ptr<FileDataSource>& src, off_t o, size_t sz, const boost::shared_ptr<FileReaderCallback>& cb) :
            _source(src), _offset(o), _cb(cb), _chunk(new FileDataChunk(sz)), _sz(sz) {
            LOG(lStorage, 5, Param("filename"), *_source, Param("size"), _sz, Param("offset"), _offset);
        }
        virtual void task() {
            size_t bytes = _source->read_bytes(_chunk->data(), _sz, _offset);
            if ( bytes != _sz ) {
                _chunk->resize(bytes);
            }
        }
        virtual void callback() {
            if ( boost::shared_ptr<FileReaderCallback> cb = _cb.lock() ) {
                if ( _chunk->sz() ) {
                    cb->success(_chunk);
                }
                else {
                    LOG(lStorage, 0, "unexpected eof", Param("filename"), *_source, Param("size"), _sz, Param("offset"), _offset);
                    cb->eof();
                }
            }
        }
        virtual void errback(const std::string& str) {
            if ( boost::shared_ptr<FileReaderCallback> cb = _cb.lock() ) {
                cb->failure(str);
            }
        }
    };
};

boost::shared_ptr<DataSource> get_source(const std::string& sourcename) {
    return boost::shared_ptr<FileDataSource>(new FileDataSource(sourcename));
}

void FileDataSource::aread(off_t offset, size_t sz, const boost::shared_ptr<FileReaderCallback>& cb) {
    if ( sz == 0 ) {
        LOG(lStorage, 0, Param("source"), *shared_from_this(), Param("size"), sz, Param("offset"), offset);
    }
    LOG(lStorage, 5, Param("source"), *shared_from_this(), Param("size"), sz, Param("offset"), offset);
    ::utility::Async::schedule(new FileDataAsyncTask(boost::static_pointer_cast<FileDataSource>(shared_from_this()), offset, sz, cb));
}
