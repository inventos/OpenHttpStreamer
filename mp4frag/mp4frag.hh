#ifndef __mp4frag_hh__ac9d6258_0bd9_4b2f_b934_984389f48934
#define __mp4frag_hh__ac9d6258_0bd9_4b2f_b934_984389f48934

#include "mapping.hh"
#include <boost/function.hpp>
#include <boost/shared_ptr.hpp>
#include <vector>
#include <string>
#include <streambuf>
#include <memory>
#include <stdint.h>

struct SampleEntry {
    off_t _offset;
    uint32_t _size;
    uint32_t _timestamp;
    uint32_t _composition_offset;
    off_t offset() const { return _offset; }
    uint32_t size() const { return _size; }
    uint32_t timestamp() const { return _timestamp; }
    uint32_t composition_offset() const { return _composition_offset & 0xFFFFFF; }
    bool video() const { return _composition_offset & 0x80000000; }
    bool keyframe() const { return _composition_offset & 0x40000000; }

    SampleEntry(off_t o, uint32_t s, uint32_t ts, uint32_t comp, bool isvideo, bool iskey = false) {
        _offset = o;
        _size = s;
        _timestamp = ts;
        _composition_offset = comp & 0xFFFFFF;
        if ( isvideo ) {
           _composition_offset |= 0x80000000;
           if ( iskey ) {
               _composition_offset |= 0x40000000;
           }
        }
    }
};

struct Fragment {
    double _duration;
    uint32_t _totalsize;
    std::vector<SampleEntry> _samples;
    Fragment() : _duration(0), _totalsize(0) {}
    uint32_t timestamp_ms() const { return _samples[0].timestamp(); }
    double duration() const { return _duration; }
};

struct Media {
    std::string medianame;
    unsigned width;
    unsigned height;
    double duration;
    std::auto_ptr<Mapping> mapping;
    std::vector<Fragment> fragments;
    std::string videoextra, audioextra;
};

boost::shared_ptr<Media> make_fragments(const std::string& filename, unsigned fragment_duration);
void get_manifest(std::streambuf* sb, const std::vector< boost::shared_ptr<Media> >& medialist,
                  const std::string& video_id);
void serialize_fragment(std::streambuf *sb, const boost::shared_ptr<Media>& media, unsigned fragnum);

void serialize(std::streambuf *sbuf, const std::vector< boost::shared_ptr<Media> >& medialist);
void get_fragment(std::streambuf *out, 
                  unsigned medianum, unsigned fragnum, const char *index, size_t indexsize,
                  const boost::function<boost::shared_ptr<Mapping> (const std::string&)>& mfactory
                  );

#endif
