#ifndef __mp4_hh__c6051ba6_1c2a_4b5d_b3ec_a4d3401a9df3
#define __mp4_hh__c6051ba6_1c2a_4b5d_b3ec_a4d3401a9df3

#include "utility/logger.hh"
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <vector>
#include <iostream>
#include <stack>

extern ::utility::logger::category lMp4;

namespace mp4 {

    class Context;

    class Parser : public ::boost::enable_shared_from_this<Parser> {
        friend class Context;
    protected:
        Context *_ctx;
        uint32_t _total;
        uint32_t _wants;
        uint32_t _to_skip;
        ::boost::shared_ptr<Parser> _up;
    public:
        Parser() : _ctx(0), _total(0), _wants(0), _to_skip(0) {}
        virtual ~Parser();
        virtual void parse(const char *data) = 0;
        size_t wants() const { return _wants; }
        void skip(uint32_t to_skip) { _to_skip = to_skip; }
    };


    struct Track {
        uint64_t _duration;
        uint32_t _timescale;
        uint32_t _pwidth, _pheight;
        std::vector<uint32_t> _sample_size;
        std::vector<std::pair<uint32_t, uint32_t> > _compos_deltas;
        std::vector<uint64_t> _chunk_offsets;
        std::vector<uint64_t> _stss;
        std::vector<std::pair<uint64_t, uint64_t> > _stts;
        std::vector<char> _extradata;

        struct SampleToChunk {
            uint32_t _first_chunk;
            uint32_t _samples_per_chunk;
            uint32_t _desc_index;
            SampleToChunk() {}
            SampleToChunk(uint32_t f, uint32_t s, uint32_t d) : _first_chunk(f), _samples_per_chunk(s), _desc_index(d) {}
        };
        std::vector<SampleToChunk> _samples_to_chunk;

        Track() : _duration(0), _pwidth(0), _pheight(0) {}
    };

    struct SampleInfo {
        uint64_t _timestamp;
        uint64_t _number;
        uint64_t _offset;
        uint32_t _composition_offset;
        uint32_t _timescale;
        uint32_t _sample_size;
        bool _video;
        bool _keyframe;

        double timestamp() const { return double(_timestamp) / _timescale; }
    };

    class MetaInfo {
        friend class Context;
        std::vector<SampleInfo> _samples;
        uint32_t _width;
        uint32_t _height;
        uint32_t _bitrate;
        double _duration;
        bool _has_video, _has_audio;
    };

    class Context {
    public:
        std::vector<SampleInfo> _samples;
    private:
        ::boost::shared_ptr<Parser> _parser;
        std::vector<char> _buffer;

    public:
        unsigned wants() const { return _parser->_wants; }
        unsigned total() const { return _parser->_total; }
        unsigned to_skip() const { return _parser->_to_skip; }

#if STACK        
        struct State {
            ::boost::function<void (const char *, Context *, unsigned&)> handler;
            unsigned total;
            unsigned wants;
            unsigned to_skip;
            template<class F> State(F f, unsigned t, unsigned w) : handler(f), total(t), wants(w), to_skip(0) {}
        };
        ::std::stack<State> _parsing_state;
#endif

    public:
        Context();
        virtual ~Context();
       
#if STACK
        void skip(unsigned count) {
            _parsing_state.top().to_skip = count;
            assert ( _parsing_state.top.to_skip <= _parsing_state.top().total );
        }
        template<class F> void push_state(F handler, unsigned total, unsigned wants) {
            assert ( wants <= total );
            assert ( total <= _parsing_state.top().total );
            _parsing_state.push(State(handler, total, wants));
        }
        void pop_state() {
            _parsing_state.pop();
        }
#else
        void skip(unsigned count) {
            _parser->_to_skip = count;
            assert ( _parser->_to_skip <= _parser->_total );
        }
        void push_state(const ::boost::shared_ptr<Parser>& p, unsigned total, unsigned wants);
        void pop_state();
#endif
        size_t feed(const char *data, size_t sz);
        ::boost::shared_ptr<Track> _current_parsed;
        // valid only after full parsing:
        ::boost::shared_ptr<Track> _video;
        ::boost::shared_ptr<Track> _audio;
        uint32_t _width;
        uint32_t _height;
        uint32_t _pwidth, _pheight;
        std::string _videoextra, _audioextra;
        uint32_t _bitrate;
        double _duration;
        bool _has_video, _has_audio;

        void _unfold(const ::boost::shared_ptr<Track>& track, bool is_video);
        void finalize();

        unsigned find_sample(double timestamp);
        unsigned nsamples() const { return _samples.size(); }
        SampleInfo *get_sample(unsigned si) { return &_samples[si]; }

        bool has_video() const { return _has_video; }
        bool has_audio() const { return _has_audio; }
        uint32_t width() const { return _width; }
        uint32_t height() const { return _height; }
        uint32_t pwidth() const { return _pwidth; }
        uint32_t pheight() const { return _pheight; }
        uint32_t bitrate() const { return _bitrate; }
        double duration() const { return _duration; }
        const std::string& videoextra() const { return _videoextra; }
        const std::string& audioextra() const { return _audioextra; }

    };
}

::std::ostream& operator<<(::std::ostream&, const mp4::Track&);
::std::ostream& operator<<(::std::ostream&, const mp4::SampleInfo&);


#endif
