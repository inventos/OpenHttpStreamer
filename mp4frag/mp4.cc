/*
 * 
 * copyright (c) 2010 ZAO Inventos (inventos.ru)
 * copyright (c) 2010 jk@inventos.ru
 * copyright (c) 2010 kuzalex@inventos.ru
 * copyright (c) 2010 artint@inventos.ru
 *
 * This file is part of mp4frag.
 *
 * mp4grag is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * mp4frag is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 */
#include "mp4.hh"
#include <boost/foreach.hpp>
#include <boost/make_shared.hpp>
#include <stdexcept>
#include <assert.h>
#include <limits.h>
#include <stdint.h>
#include <memory.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#define MP4_CHECK(exp) if ( !(exp) ) { throw std::runtime_error(std::string("mp4 format error: ") + #exp); } else
using boost::make_shared;

namespace {
    uint16_t __swab16(uint16_t x)
    {
        return x<<8 | x>>8;
    }
    uint32_t __swab32(uint32_t x)
    {
        return x<<24 | x>>24 |
          (x & (uint32_t)0x0000ff00UL)<<8 |
          (x & (uint32_t)0x00ff0000UL)>>8;
    }
    uint64_t __swab64(uint64_t x)
    {
        return x<<56 | x>>56 |
          (x & (uint64_t)0x000000000000ff00ULL)<<40 |
          (x & (uint64_t)0x0000000000ff0000ULL)<<24 |
          (x & (uint64_t)0x00000000ff000000ULL)<< 8 |
          (x & (uint64_t)0x000000ff00000000ULL)>> 8 |
          (x & (uint64_t)0x0000ff0000000000ULL)>>24 |
          (x & (uint64_t)0x00ff000000000000ULL)>>40;
    }
}

namespace mp4 {

#define UINT8(addr) (*reinterpret_cast<const uint8_t*>(addr))
#define UINT16(addr) (__swab16(*reinterpret_cast<const uint16_t*>(addr)))
#define UINT32(addr) (__swab32(*reinterpret_cast<const uint32_t*>(addr)))
#define UINT64(addr) (__swab64(*reinterpret_cast<const uint64_t*>(addr)))

#define EQ(addr, TAG) ( memcmp(addr, TAG, 4) == 0)


    void Context::push_state(const ::boost::shared_ptr<Parser>& p, unsigned total, unsigned wants) {
        assert ( total >= wants );
        p->_up = _parser;
        p->_total = total;
        p->_wants = wants;
        p->_to_skip = 0;
        p->_ctx = this;
        _parser = p;
    }

    void Context::pop_state() {
        // std::cerr << "pop_state\n";
        _parser = _parser->_up;
    }

    Parser::~Parser() {} 

    struct TopLevel : public Parser {
        virtual void parse(const char *data);
    };

    struct Moov : public Parser {
        virtual void parse(const char *data);
    };

    struct Trak : public Parser {
        virtual void parse(const char *data);
    };

    struct Mdia : public Parser {
        virtual void parse(const char *data);
    };

    struct Tkhd : public Parser {
        virtual void parse(const char *data);
    };

    struct Minf : public Parser {
        // Minf(size_t total) : Parser(total) { _wants = 8; }
        virtual void parse(const char *data);
    };

    struct Mdhd : public Parser {
        // Mdhd(size_t total) : Parser(total) { _wants = total; }
        virtual void parse(const char *data);
    };

    struct Hdlr : public Parser {
        // Hdlr(size_t total) : Parser(total) { _wants = total; }
        virtual void parse(const char *data);
    };

    struct Stbl : public Parser {
        // Stbl(size_t total) : Parser(total) { _wants = 8; }
        virtual void parse(const char *data);
    };

    struct Stsz : public Parser {
        // Stsz(size_t total) : Parser(total) { _wants = total; }
        virtual void parse(const char *data);
    };

    struct Stco : public Parser {
        // Stco(size_t total) : Parser(total) { _wants = total; }
        virtual void parse(const char *data);
    };

    struct Stss : public Parser {
        // Stss(size_t total) : Parser(total) { _wants = total; }
        virtual void parse(const char *data);
    };

    struct Ctts : public Parser {
        // Stss(size_t total) : Parser(total) { _wants = total; }
        virtual void parse(const char *data);
    };

    struct Stsc : public Parser {
        // Stsc(size_t total) : Parser(total) { _wants = total; }
        virtual void parse(const char *data);
    };

    struct Co64 : public Parser {
        // Co64(size_t total) : Parser(total) { _wants = total; }
        virtual void parse(const char *data);
    };

    struct Stts : public Parser {
        // Stts(size_t total) : Parser(total) { _wants = total; }
        virtual void parse(const char *data);
    };

    struct Stsd : public Parser {
        // Stts(size_t total) : Parser(total) { _wants = total; }
        virtual void parse(const char *data);
    };



    void TopLevel::parse(const char *data) {
        uint32_t sz = UINT32(data);
        std::cerr << "TopLevel: " << sz << "," << _total << "," << std::string(data + 4, 4) << "\n";
        assert( sz <= _total );
        _total -= sz;
        sz -= 8;
        if ( EQ(data + 4, "moov") ) {
            _ctx->push_state(make_shared<Moov>(), sz, 8);
        }
        else {
            skip(sz);
        }
    }

    void Moov::parse(const char *data) {
        uint32_t sz = UINT32(data);
        assert ( _total >= sz );
        _total -= sz;
        sz -= 8;
        // std::cerr << "Moov: " << sz << "," << _total << "," << std::string(data + 4, 4) << "\n";
        if ( EQ(data+4, "trak") ) {
            _ctx->push_state(make_shared<Trak>(), sz, 8);
            _ctx->_current_parsed.reset(new Track);
        }
        else {
            skip(sz);
        }
    }

    void Trak::parse(const char *data) {
        uint32_t sz = UINT32(data);
        // std::cerr << "Trak: " << "sz=" <<  sz << ", total= " << _total << "," << std::string(data + 4, 4) << "\n";
        assert ( _total >= sz );
        _total -= sz;
        sz -= 8;
        if ( EQ(data+4, "mdia") ) {
            _ctx->push_state(make_shared<Mdia>(), sz, 8);
        }
        else if ( EQ(data + 4, "tkhd") ) {
            _ctx->push_state(make_shared<Tkhd>(), sz, sz);
        }
        else {
            skip(sz);
        }
    }

    void Mdia::parse(const char *data) {
        uint32_t sz = UINT32(data);
        // std::cerr << "Mdia: " << sz << "," << _total << "," << std::string(data + 4, 4) << "\n";
        assert ( _total >= sz );
        _total -= sz;
        sz -= 8;
        if ( EQ(data+4, "minf") ) {
            _ctx->push_state(make_shared<Minf>(), sz, 8);
        }
        else if ( EQ(data+4, "mdhd") ) {
            _ctx->push_state(make_shared<Mdhd>(), sz, sz);
        }
        else if ( EQ(data+4, "hdlr") ) {
            _ctx->push_state(make_shared<Hdlr>(), sz, sz);
        }
        else {
            skip(sz);
        }
    }

    void Tkhd::parse(const char *data) {
        uint16_t version = UINT8(data);
        if ( version == 1 ) {
            // LOG(lMp4, 5, Param("duration"), UINT64(data + 28));
            data += 28 + 8;
        }
        else {
            // std::cerr << "duration: " << UINT32(data + 20) << '\n';
            data += 20 + 4;
        }
        _ctx->_current_parsed->_pwidth = UINT32(data + 52);
        _ctx->_current_parsed->_pheight = UINT32(data + 56);
        /*
        if ( uint64_t pheight = _ctx->_current_parsed->_pheight ) {
            // LOG(lMp4, 5, Param("aspect"), double(_ctx->_current_parsed->_pwidth)/pheight);
        }
        */
        _ctx->pop_state();
    }

    void Minf::parse(const char *data) {
        uint32_t sz = UINT32(data);
        _total -= sz;
        sz -= 8;
        if ( EQ(data+4, "stbl") ) {
            _ctx->push_state(make_shared<Stbl>(), sz, 8);
        }
        else {
            skip(sz);
        }
    }

    void Stbl::parse(const char *data) {
        uint32_t sz = UINT32(data);
        // std::cerr << sz << "," << _total << "," << std::string(data + 4, 4) << "\n";
        _total -= sz;
        sz -= 8;
        if ( EQ(data+4, "stsz") ) {
            _ctx->push_state(make_shared<Stsz>(), sz, sz);
        }
        else if ( EQ(data+4, "stco") ) {
            _ctx->push_state(make_shared<Stco>(), sz, sz);
        }
        else if ( EQ(data+4, "stss") ) {
            _ctx->push_state(make_shared<Stss>(), sz, sz);
        }
        else if ( EQ(data+4, "stts") ) {
            _ctx->push_state(make_shared<Stts>(), sz, sz);
        }
        else if ( EQ(data+4, "stsc") ) {
            _ctx->push_state(make_shared<Stsc>(), sz, sz);
        }
        else if ( EQ(data+4, "co64") ) {
            _ctx->push_state(make_shared<Co64>(), sz, sz);
        }
        else if ( EQ(data+4, "stsd") ) {
            _ctx->push_state(make_shared<Stsd>(), sz, sz);
        }
        else if ( EQ(data+4, "ctts") ) {
            _ctx->push_state(make_shared<Ctts>(), sz, sz);
        }
        else {
            skip(sz);
        }
    }

    void Mdhd::parse(const char *data) {
        uint16_t version = UINT8(data);
        if ( version == 1 ) {
            MP4_CHECK ( _total == 4 + 28 + 4 );
            _ctx->_current_parsed->_timescale = UINT32(data + 20);
            _ctx->_current_parsed->_duration = UINT64(data + 24);
        }
        else {
            MP4_CHECK ( version == 0 );
            MP4_CHECK ( _total == 4 + 16 + 4 );
            _ctx->_current_parsed->_timescale = UINT32(data + 12);
            _ctx->_current_parsed->_duration = UINT32(data + 16);
        }
        // std::cerr << "mdhd::version=" << version << ", timescale=" << _ctx->_current_parsed->_timescale << ", duration=" << _ctx->_current_parsed->_duration << "\n";
        _ctx->pop_state();
    }

    void Hdlr::parse(const char *data) {
        MP4_CHECK ( _total >  24 );
        if ( memcmp(data + 8, "vide", 4) == 0 ) {
            _ctx->_video = _ctx->_current_parsed;
            // LOG(lMp4, 5, "current_parsed is video");
        }
        else if ( memcmp(data + 8, "soun", 4) == 0 ) {
            _ctx->_audio = _ctx->_current_parsed;
            // LOG(lMp4, 5, "current_parsed is audio");
        }
        _ctx->pop_state();
    }

    void Stsz::parse(const char *data) {
        MP4_CHECK ( _total >= 12 );
        MP4_CHECK ( UINT16(data + 2) == 0 );
        uint32_t sample_size = UINT32(data + 4);
        uint32_t sample_count = UINT32(data + 8);
        _ctx->_current_parsed->_sample_size.resize(0);
        _ctx->_current_parsed->_sample_size.reserve(sample_count);
        if ( sample_size == 0 ) {
            data += 12;
            for ( unsigned i = 0; i < sample_count; ++i ) {
                _ctx->_current_parsed->_sample_size.push_back(UINT32(data));
                data += 4;
            }
        }
        else {
            for ( unsigned i = 0; i < sample_count; ++i ) {
                _ctx->_current_parsed->_sample_size.push_back(sample_size);
            }
        }
        _ctx->pop_state();
    }

    void Ctts::parse(const char *data) {
        MP4_CHECK ( _total >= 12 );
        //MP4_CHECK ( UINT16(data + 2) == 0 );

        uint32_t entry_count = UINT32(data + 4);
        const char* cur_record = data + 8;
        for (uint32_t i = 0; i < entry_count; ++i)
        {
        	MP4_CHECK(_total >= unsigned(cur_record + 8 - data));
        	_ctx->_current_parsed->_compos_deltas.push_back(std::pair<uint32_t, uint32_t>(UINT32(cur_record), UINT32(cur_record + 4)));
        	cur_record += 8;
        }
        _ctx->pop_state();
    }


    void Stco::parse(const char *data) {
        MP4_CHECK(UINT16(data) == 0);
        MP4_CHECK(UINT16(data + 2) == 0);
        uint32_t entry_count = UINT32(data + 4);
        MP4_CHECK(_total == 8 + entry_count * 4);
        _ctx->_current_parsed->_chunk_offsets.resize(0);
        _ctx->_current_parsed->_chunk_offsets.reserve(entry_count);
        data += 8;
        for ( unsigned i = 0; i < entry_count; ++i ) {
            _ctx->_current_parsed->_chunk_offsets.push_back(UINT32(data));
            data += 4;
        }
        _ctx->pop_state();
    }

    void Co64::parse(const char *data) {
        MP4_CHECK(UINT16(data) == 0);
        MP4_CHECK(UINT16(data + 2) == 0);
        uint32_t entry_count = UINT32(data + 4);
        MP4_CHECK(_total == 8 + entry_count * 8);
        _ctx->_current_parsed->_chunk_offsets.resize(0);
        _ctx->_current_parsed->_chunk_offsets.reserve(entry_count);
        data += 8;
        for ( unsigned i = 0; i < entry_count; ++i ) {
            _ctx->_current_parsed->_chunk_offsets.push_back(UINT64(data));
            data += 8;
        }
        _ctx->pop_state();
    }

    void Stss::parse(const char *data) {
        MP4_CHECK( UINT16(data) == 0 );
        MP4_CHECK( UINT16(data + 2) == 0);
        uint32_t entry_count = UINT32(data + 4);
        MP4_CHECK( _total == 8 + entry_count * 4 );
        _ctx->_current_parsed->_stss.resize(0);
        _ctx->_current_parsed->_stss.reserve(entry_count);
        data += 8;
        for ( unsigned i = 0; i < entry_count; ++i ) {
            _ctx->_current_parsed->_stss.push_back(UINT32(data));
            data += 4;
        }
        _ctx->pop_state();
    }

    void Stts::parse(const char *data) {
        MP4_CHECK( UINT16(data) == 0 );
        MP4_CHECK( UINT16(data + 2) == 0 );
        uint32_t entry_count = UINT32(data + 4);
        MP4_CHECK( _total == 8 + entry_count * 8 );
        _ctx->_current_parsed->_stts.resize(0);
        _ctx->_current_parsed->_stts.reserve(entry_count);
        data += 8;
        for ( unsigned i = 0; i < entry_count; ++i ) {
            _ctx->_current_parsed->_stts.push_back(std::make_pair(UINT32(data), UINT32(data + 4)));
            data += 8;
        }
        _ctx->pop_state();
    }

    void Stsc::parse(const char *data) {
        MP4_CHECK( UINT16(data) == 0 );
        MP4_CHECK( UINT16(data + 2) == 0 );
        uint32_t entry_count = UINT32(data + 4);
        MP4_CHECK( _total == 8 + entry_count * 12 );
        _ctx->_current_parsed->_samples_to_chunk.resize(0);
        _ctx->_current_parsed->_samples_to_chunk.reserve(entry_count);
        data += 8;
        for ( unsigned i = 0; i < entry_count; ++i ) {
            _ctx->_current_parsed->_samples_to_chunk.push_back(Track::SampleToChunk(UINT32(data), UINT32(data + 4), UINT32(data + 8)));
            data += 12;
        }
        // push anchor:
        _ctx->_current_parsed->_samples_to_chunk.push_back(Track::SampleToChunk(UINT_MAX, 0, 1));
        _ctx->pop_state();
    }

    void Stsd::parse(const char *data) {
        // uint16_t data_reference_index = UINT16(data + 6);
        uint32_t entries = UINT32(data + 4);
        data += 8;
        const char *end = data + _total;
        for ( uint32_t iii = 0; iii < entries; ++iii ) {
            uint32_t sampleentrysize = UINT32(data);
            // data, data+4 - size; data+4, data+8 - signature; 
            // std::cerr << "Stsd: " << sampleentrysize << ":" << std::string(data+4, data+8) << std::endl;
            if ( _ctx->_current_parsed == _ctx->_video ) {
                // parse videosampleentry
                MP4_CHECK ( UINT16(data + 16) == 0 );       // pre_defined
                MP4_CHECK ( UINT16(data + 18) == 0 );      // reserved
                MP4_CHECK ( UINT32(data + 20) == 0 );      // pre_defined
                MP4_CHECK ( UINT32(data + 24) == 0 );      // pre_defined
                MP4_CHECK ( UINT32(data + 28) == 0 );      // pre_defined
                _ctx->_width = UINT16(data + 32);
                _ctx->_height = UINT16(data + 34);
                uint32_t h_res = UINT32(data + 36);
                MP4_CHECK(h_res == 0x480000);
                uint32_t v_res = UINT32(data + 40);
                MP4_CHECK(v_res == 0x480000);
                MP4_CHECK ( UINT32(data + 44) == 0 );      // reserved
                MP4_CHECK ( UINT16(data + 48) == 1 );      // frame_count
                // string[32] - compressorname, all 0's
                MP4_CHECK ( UINT16(data + 82) == 0x18 );   // depth
                MP4_CHECK ( UINT16(data + 84) == 0xFFFFU ); // pre_defined
                //
                if ( data + 86 < end ) {
                    MP4_CHECK ( end - data >= 86 + 8 );
                    uint32_t cab_size = UINT32(data + 86);

                    std::vector<char> tmp(data + 94, data + 94 + cab_size - 8);
                    std::swap(_ctx->_video->_extradata, tmp);

                    // std::cerr << "avcC[0]=" << std::hex << unsigned(data[94]) << " avcC[4]=" << unsigned(data[98]) << std::dec << std::endl;
                    
                }
            }
            else if ( _ctx->_current_parsed == _ctx->_audio ) {
                // parse audiosampleentry
                uint32_t esds_size = UINT32(data + 36);
#if 0
                std::cerr << "-- channelcount=" << UINT16(data + 24) << "\n"
                          << "-- samplesize=" << UINT16(data + 26) << "\n"
                          << "-- samplerate=" << (UINT32(data + 32) >> 16) << "\n"
                          << "esds: " << esds_size << ":" << std::string(data + 40, 4) << "\n";
#endif
                _ctx->_audio->_extradata.clear();
                _ctx->_audio->_extradata.reserve(2);
                _ctx->_audio->_extradata.push_back(data[36 + esds_size - 5]);
                _ctx->_audio->_extradata.push_back(data[36 + esds_size - 4]);
            }
            data += sampleentrysize;
        }
        _ctx->pop_state();
    }


    struct VoidParser : public Parser {
        void parse(const char *) {}
    };

    Context::Context() {
        push_state(make_shared<VoidParser>(), 0, 0);
        push_state(make_shared<TopLevel>(), UINT_MAX, 8);
    }

    Context::~Context() {
    }

    size_t Context::feed(const char *data, size_t sz) {
        _buffer.insert(_buffer.end(), data, data + sz);
        while ( 1 ) {
            while ( _parser->_total == 0 ) {
                if ( _parser->_wants == 0 ) { // признак верхушки стека, конец парсинга
                    return sz;
                }
                unsigned to_skip = _parser->_to_skip;   // XXX BAD!
                pop_state();
                _parser->_to_skip = to_skip;            // XXX BAD!
            }
            unsigned wants = _parser->_wants;
            unsigned buffersize = _buffer.size();
            // LOG(lMp4, 0, "feeding", Param("to_skip"), _parser->_to_skip, Param("wants"), wants, Param("buffersize"), buffersize);
            if ( wants <= buffersize ) {
                _parser->parse(&_buffer[0]);
                // std::cerr << "after parsing skip=" << _parser->_to_skip << ", wants=" << wants << ", buffersize=" << buffersize << "\n";
                unsigned to_skip = _parser->_to_skip + wants;
                if ( long(to_skip) != long(_parser->_to_skip) + long(wants) ) { // overflow
                    // LOG(lMp4, 0, "overflow", Param("to_skip"), _parser->_to_skip, Param("wants"), wants, Param("buffersize"), buffersize);
                }
                if ( (unsigned long)(_parser->_to_skip) + (unsigned long)(wants) < buffersize ) {
                    if ( _parser->_to_skip >= buffersize ) {
                        std::cerr << "to_skip=" << _parser->_to_skip << ", buffersize=" << buffersize << std::endl;
                        assert ( _parser->_to_skip < buffersize );
                    }
                    assert ( wants < buffersize );
                    _buffer.erase(_buffer.begin(), _buffer.begin() + to_skip);
                    assert ( _buffer.size() == buffersize - to_skip );
                    _parser->_to_skip = 0;
                }
                else {
                    _parser->_to_skip -= buffersize - wants;
                    _buffer.erase(_buffer.begin(), _buffer.end());
                }
            }
            else {
                break;
            }
        }
        return sz;
    }


    void Context::_unfold(const ::boost::shared_ptr<Track>& track, bool is_video) {
        uint32_t number = 0;
        uint64_t current_time = 0;
        typedef std::pair<uint64_t,uint64_t> stts_entry_type;
        unsigned keyframe_index = 0;

        std::vector<Track::SampleToChunk>::const_iterator chunk_run = track->_samples_to_chunk.begin();

        uint32_t samples_per_chunk = chunk_run->_samples_per_chunk;
        uint32_t current_chunk = 0;
        uint32_t offset_in_current_chunk = 0;
        uint32_t sample_number_in_chunk = 0;
        ++chunk_run;

        assert ( chunk_run->_desc_index == 1 );

        typedef std::pair<uint32_t,uint32_t> ctts_entry_type;
        unsigned int order_n = 0;
        std::vector<ctts_entry_type>::iterator cur_ctts_entry = track->_compos_deltas.begin();

        BOOST_FOREACH( const stts_entry_type& entry, track->_stts ) {
            for ( unsigned iii = 0; iii < entry.first; ++iii ) {
                _samples.push_back(SampleInfo());
                SampleInfo& last = _samples.back();
                last._timestamp = current_time;
                last._timescale = track->_timescale;

                uint32_t compos_delta;
                if (cur_ctts_entry != track->_compos_deltas.end())
                {
                	compos_delta = cur_ctts_entry->second;
                	++order_n;
                	if (order_n >= cur_ctts_entry->first)
                	{
                		++cur_ctts_entry;
                		order_n = 0;
                	}
                }
                else
                {
                	compos_delta = 0;
                }

                last._composition_offset = compos_delta;


                uint32_t sample_size = track->_sample_size[number];
                last._sample_size = sample_size;

                last._offset = track->_chunk_offsets[current_chunk] + offset_in_current_chunk;
                if ( ++sample_number_in_chunk < samples_per_chunk ) {
                    offset_in_current_chunk += sample_size;
                }
                else {
                    sample_number_in_chunk = 0;
                    offset_in_current_chunk = 0;
                    if ( ++current_chunk + 1 >= chunk_run->_first_chunk ) {
                        assert ( current_chunk + 1 == chunk_run->_first_chunk );
                        samples_per_chunk = chunk_run->_samples_per_chunk;
                        ++chunk_run;
                        assert ( chunk_run->_desc_index == 1 );
                    }
                }
                last._video = is_video;
                if ( is_video && keyframe_index < track->_stss.size() && track->_stss[keyframe_index] == number + 1 ) {
                    last._keyframe = true;
                    ++keyframe_index;
                }
                else {
                    last._keyframe = false;
                }
                ++number;
                current_time += entry.second;
            }
        }
    }
    namespace {
        struct SampleSorter {
            bool operator()(const SampleInfo& sample1, const SampleInfo& sample2) const {
                return double(sample1._timestamp) / sample1._timescale < double(sample2._timestamp) / sample2._timescale ||
                       ( double(sample1._timestamp) / sample1._timescale == double(sample2._timestamp) / sample2._timescale &&
                         sample1._offset < sample2._offset );
            }
            bool operator()(const SampleInfo& sample1, double timestamp) const {
                return double(sample1._timestamp) / sample1._timescale < timestamp ;
            }
            bool operator()(double timestamp, const SampleInfo& sample2) const {
                return timestamp < double(sample2._timestamp) / sample2._timescale ;
            }
        };
    }

    void Context::finalize() {
        if ( _video ) _unfold(_video, true);
        if ( _audio ) _unfold(_audio, false);
        std::sort(_samples.begin(), _samples.end(), SampleSorter());

        // XXX
        uint64_t nbits = 0;
        double maxtime = 0.0;
        BOOST_FOREACH(SampleInfo& si, _samples) {
            nbits += si._sample_size;
            double ts = si.timestamp();
            if ( ts > maxtime ) {
                maxtime = ts;
            }
        }
        _bitrate = nbits * 8.0 / maxtime;
        // std::cerr << "maxtime=" << maxtime << std::endl;
        // XXX
#if 0        
        unsigned count = 0;
        for ( ::std::vector<SampleInfo>::const_iterator it = _samples.begin(); it < _samples.end() && count < 30; ++it, ++count) {
            if ( it->_video ) {
                std::cerr << "---" << *it << std::endl;
            }
        }
#endif

        _duration = 0.0;
        if ( (_has_video = _video) ) {
            _duration = double(_video->_duration) / _video->_timescale;
            _pwidth = _video->_pwidth;
            _pheight = _video->_pheight;
            _videoextra.assign(&_video->_extradata[0], _video->_extradata.size());
            _video.reset();
        }
        if ( (_has_audio = _audio) ) {
            double audio_duration = double(_audio->_duration) / _audio->_timescale;
            if ( _duration < audio_duration ) {
                _duration = audio_duration;
            }
            _audioextra.assign(&_audio->_extradata[0], _audio->_extradata.size());
            _audio.reset();
        }
        _current_parsed.reset();
    }

    unsigned Context::find_sample(double timestamp) {
        //decltype(_samples.begin()) iter = ::std::lower_bound(_samples.begin(), _samples.end(), timestamp, SampleSorter());
//        if ( iter != _samples.end() ) {
//            if ( iter == _samples.begin() ) {
//                return 0;
//            }
//            if ( double(iter->_timestamp) / iter->_timescale < timestamp ) {
//                ++iter;
//            }
//            return iter - _samples.begin();
//        }
        return _samples.size();
    }



} // namespace mp4


::std::ostream& operator<<(::std::ostream& out, const mp4::Track& t) {
    return
      out << "-- timescale = " << t._timescale << "\n"
      << "-- sample_size: " << t._sample_size.size() << " entries\n"
      << "-- chunk_offsets: " << t._chunk_offsets.size() << " entries\n"
      << "-- stss: " << t._stss.size() << " entries\n"
      << "-- stts: " << t._stts.size() << " entries\n"
      << "-- sample_to_chunk: " << t._samples_to_chunk.size() << " entries;\n"
      << "-- presentation width: " << t._pwidth << ";\n"
      << "-- presentation height: " << t._pheight << ";\n"
      << "\n";
}

::std::ostream& operator<<(::std::ostream& o, const mp4::SampleInfo& si) {
    return o
      << "[ " << si._timestamp << "/"
              << si._timescale << "="
              << si.timestamp() << ", "
              // << si._number << ", "
              << si._sample_size << ", "
              << si._offset << ", "
              << si._video << ", "
              << si._keyframe << " ]";
}
