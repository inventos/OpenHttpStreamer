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
#include "base64.hh"
#include <boost/program_options.hpp>
#include <boost/system/system_error.hpp>
#include <boost/system/error_code.hpp>
#include <boost/foreach.hpp>
#include <sstream>
#include <fstream>
#include <stdexcept>
#include <netinet/in.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

using namespace boost::system;

namespace {
    std::string manifest_name = "manifest.f4m";
    std::string docroot = ".";
    std::string basedir = ".";
    std::string video_id("some_video");
    std::vector<std::string> srcfiles;
    int fragment_duration;
}


void write16(std::streambuf& buf, uint16_t value) {
    buf.sputc( (value / 0x100) & 0xFF );
    buf.sputc( value & 0xFF );
}

void write24(std::streambuf& buf, uint32_t value) {
    char bytes[3];
    bytes[0] = (value / 0x10000) & 0xFF;
    bytes[1] = (value / 0x100) & 0xFF;
    bytes[2] = value & 0xFF;
    buf.sputn(bytes, 3);
}

void write32(std::streambuf& buf, uint32_t value) {
    char bytes[4];
    bytes[0] = (value / 0x1000000) & 0xFF;
    bytes[1] = (value / 0x10000) & 0xFF;
    bytes[2] = (value / 0x100) & 0xFF;
    bytes[3] = value & 0xFF;
    buf.sputn(bytes, 4);
}

void write64(std::streambuf& buf, uint64_t value) {
    char bytes[8];
    bytes[0] = (value / 0x100000000000000ULL) & 0xFF;
    bytes[1] = (value / 0x1000000000000ULL) & 0xFF;
    bytes[2] = (value / 0x10000000000ULL) & 0xFF;
    bytes[3] = (value / 0x100000000ULL) & 0xFF;
    bytes[4] = (value / 0x1000000) & 0xFF;
    bytes[5] = (value / 0x10000) & 0xFF;
    bytes[6] = (value / 0x100) & 0xFF;
    bytes[7] = value & 0xFF;
    buf.sputn(bytes, 8);
}

void writebox(std::streambuf& buf, const char *name, const std::string& box) {
    write32(buf, box.size() + 8);
    buf.sputn(name, 4);
    buf.sputn(box.c_str(), box.size());
}



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





struct fileinfo {
    std::string filename;
    std::string dirname;
    unsigned width;
    unsigned height;
    double duration;
    const char *mapping;
    off_t filesize;
    std::vector<Fragment> fragments;
    std::string videoextra, audioextra;
};



void generate_fragments(std::vector<Fragment>& fragments, boost::shared_ptr<mp4::Context>& ctx, double timestep) {
    double limit_ts = timestep;
    double now;
    unsigned nsample = 0;
    fragments.clear();

    fragments.push_back(Fragment());
    std::vector<SampleEntry> *samplelist = &(fragments.back()._samples);

    while ( nsample < ctx->nsamples() ) {
        mp4::SampleInfo *si = ctx->get_sample(nsample++);
        now = si->timestamp();

        if ( si->_video && si->_keyframe && now >= limit_ts ) {
            fragments.push_back(Fragment());
            samplelist = &(fragments.back()._samples);
            limit_ts = now + timestep;
        }
        samplelist->push_back(SampleEntry(si->_offset, si->_sample_size,
                                          now * 1000.0,
                                          si->_composition_offset * 1000.0 / si->_timescale,
                                          si->_video,
                                          si->_keyframe));
        fragments.back()._duration += now;
        fragments.back()._totalsize += si->_sample_size + 11 + (si->_video ? 5 : 2) + 4;
    }
}




void write_video_packet(std::streambuf *sb, bool key, bool sequence_header, uint32_t comp_offset,
                        uint32_t timestamp,
                        const char *packetdata, uint32_t packetsize) {
    sb->sputc(9);
    write24(*sb, packetsize + 5);
    write24(*sb, timestamp & 0xffffff);
    sb->sputc((timestamp >> 24) & 0xff);
    write24(*sb, 0);
    sb->sputc( ((key || sequence_header) ? 0x10 : 0x20) | 7 );
    sb->sputc( sequence_header ? 0 : 1 );
    write24(*sb, comp_offset);
    sb->sputn(packetdata, packetsize);
    write32(*sb, packetsize + 5 + 11);
}


void write_audio_packet(std::streambuf *sb, bool sequence_header,
                        uint32_t timestamp,
                        const char *packetdata, uint32_t packetsize) {
    sb->sputc(8);
    write24(*sb, packetsize + 2);
    write24(*sb, timestamp & 0xffffff);
    sb->sputc((timestamp >> 24) & 0xff);
    write24(*sb, 0);
    sb->sputc(0xaf);
    sb->sputc( sequence_header ? 0 : 1 );
    sb->sputn(packetdata, packetsize);
    write32(*sb, packetsize + 2 + 11);
}


void write_fragment_prefix(std::streambuf *sb,
                           const std::string& videoextra,
                           const std::string& audioextra,
                           uint32_t ts) {
    if ( videoextra.size() ) {
        write_video_packet(sb, true, true, 0, ts, 
                           videoextra.c_str(), videoextra.size());
    }
    if ( audioextra.size() ) {
        write_audio_packet(sb, true, ts, audioextra.c_str(), audioextra.size());
    }
}


fileinfo make_fragments(const std::string& filename) {
    fileinfo finfo;
    finfo.filename = filename;

    std::string::size_type i_lastslash = filename.rfind('/');
    if ( i_lastslash == std::string::npos ) {
        finfo.dirname = filename + ".d";
    }
    else {
        finfo.dirname.assign(filename, i_lastslash + 1, filename.size());
        finfo.dirname += ".d";
    }

    struct stat st;
    if ( stat(filename.c_str(), &st) < 0 ) {
        throw system_error(errno, get_system_category(), "stat");
    }
    finfo.filesize = st.st_size;

    int fd = open(filename.c_str(), O_RDONLY);
    if ( fd == -1 ) {
        throw system_error(errno, get_system_category(), "opening " + filename);
    }
    void *mapping = mmap(NULL, st.st_size, PROT_READ, MAP_SHARED, fd, 0);
    if ( mapping == (void*)-1 ) {
        throw system_error(errno, get_system_category(), "mmaping " + filename);
    }
    finfo.mapping = (const char*)mapping;

    boost::shared_ptr<mp4::Context> ctxt(new mp4::Context);
    off_t offset = 0;
    while ( unsigned nbytes = ctxt->wants() ) {
        if ( unsigned s = ctxt->to_skip() ) {
            offset += s;
            ctxt->skip(0);                    
        }
        if ( offset >= st.st_size ) {
            break;
        }
        ctxt->feed( (char*)mapping + offset, nbytes );
        offset += nbytes;
    }
    close(fd);
    ctxt->finalize();

    finfo.duration = ctxt->duration();

    finfo.videoextra = ctxt->videoextra();
    finfo.audioextra = ctxt->audioextra();

    generate_fragments(finfo.fragments, ctxt, fragment_duration / 1000.0);
    return finfo;
}



void parse_options(int argc, char **argv) {
    namespace po = boost::program_options;

    po::options_description desc("Allowed options");
    desc.add_options()
      ("help", "produce help message")
      ("src", po::value< std::vector<std::string> >(&srcfiles), "source mp4 file name")
      ("docroot", po::value<std::string>(&docroot)->default_value("."), "docroot directory")
      ("basedir", po::value<std::string>(&basedir)->default_value("."), "base directory for manifest file")
      ("video_id", po::value<std::string>(&video_id)->default_value("some_video"), "video id for manifest file")
      ("manifest", po::value<std::string>(&manifest_name)->default_value("manifest.f4m"), "manifest file name")
      ("fragmentduration", po::value<int>(&fragment_duration)->default_value(3000), "single fragment duration, ms")
    ;

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);    

    if (vm.count("help") || argc == 1 || srcfiles.size() == 0) {
        std::cerr << desc << "\n";
        exit(1);
    }
    
}



int main(int argc, char **argv) try {
    parse_options(argc, argv);

    std::vector<fileinfo> fileinfo_list;
    for ( std::vector<std::string>::const_iterator b = srcfiles.begin(), e = srcfiles.end(); b != e; ++b ) {
        fileinfo_list.push_back(make_fragments(*b));

        // writing
        const fileinfo& last = fileinfo_list.back();

        // create the directory if needed:
        std::string::size_type i_lastslash = last.filename.rfind('/');
        std::string dirname;
        if ( i_lastslash == std::string::npos ) {
            dirname = last.filename + ".d";
        }
        else {
            dirname.assign(last.filename, i_lastslash + 1, last.filename.size());
            dirname += ".d";
        }
        if ( mkdir(dirname.c_str(), 0755) == -1 && errno != EEXIST ) {
            throw system_error(errno, get_system_category(), "mkdir " + dirname);
        }
        for ( unsigned fragment = 1; fragment <= last.fragments.size(); ++fragment ) {
            const Fragment& fr = last.fragments[fragment-1];
            // write content
            std::filebuf out;
            std::stringstream fragment_file;
            fragment_file << dirname << "/Seg1-Frag" << fragment;
            if ( out.open(fragment_file.str().c_str(), std::ios::out | std::ios::binary | std::ios::trunc) ) {
                write32(out, fr._totalsize + 8 +
                             4 + 11 + 5 + last.videoextra.size() +
                             4 + 11 + 2 + last.audioextra.size()
                       );
                out.sputn("mdat", 4);

                write_fragment_prefix(&out, last.videoextra, last.audioextra, fr.timestamp_ms());
                BOOST_FOREACH(const SampleEntry& se, fr._samples) {
                    if ( se.video() ) {
                        write_video_packet(&out, se.keyframe(), false, se.composition_offset(),
                                           se.timestamp(),
                                           last.mapping + se.offset(), se.size());
                    }
                    else {
                        write_audio_packet(&out, false, se.timestamp(),
                                           last.mapping + se.offset(), se.size());
                    }
                }
                if ( !out.close() ) {
                    std::stringstream errmsg;
                    errmsg << "Error closing " << fragment_file.str();
                    throw std::runtime_error(errmsg.str());
                }
            }
            else {
                std::stringstream errmsg;
                errmsg << "Error opening " << fragment_file.str();
                throw std::runtime_error(errmsg.str());
            }
        }
        
    }

    unsigned total_fragments = fileinfo_list[0].fragments.size();

    std::stringbuf abst;
    abst.sputc(0); // version
    abst.sputn("\0\0", 3); // flags;
    write32(abst, 14); // bootstrapinfoversion
    abst.sputc(0); // profile, live, update
    write32(abst, 1000); // timescale
    write64(abst, fileinfo_list[0].duration * 1000); // currentmediatime
    write64(abst, 0); // smptetimecodeoffset
    abst.sputc(0); // movieidentifier
    abst.sputc(0); // serverentrycount
    abst.sputc(0); // qualityentrycount
    abst.sputc(0); // drmdata
    abst.sputc(0); // metadata
    abst.sputc(1); // segment run table count

    std::stringbuf asrt;
    asrt.sputc(0); // version
    write24(asrt, 0); // flags
    asrt.sputc(0); // qualityentrycount
    write32(asrt, 1); // segmentrunentry count
    write32(asrt, 1); // first segment
    write32(asrt, total_fragments); // fragments per segment

    writebox(abst, "asrt", asrt.str());

    abst.sputc(1); // fragment run table count

    std::stringbuf afrt;
    afrt.sputc(0); // version
    write24(afrt, 0); // flags
    write32(afrt, 1000); // timescale
    afrt.sputc(0); // qualityentrycount
    write32(afrt, total_fragments); // fragmentrunentrycount
    for ( unsigned ifragment = 0; ifragment < total_fragments; ++ifragment ) {
        write32(afrt, ifragment + 1);
        write64(afrt, uint64_t(fileinfo_list[0].fragments[ifragment].timestamp_ms()));
        write32(afrt, uint32_t(fileinfo_list[0].fragments[ifragment].duration() * 1000));
        if ( fileinfo_list[0].fragments[ifragment].duration() == 0 ) {
            assert ( ifragment == total_fragments - 1 );
            afrt.sputc(0);
        }
    }
    writebox(abst, "afrt", afrt.str());

    std::stringbuf bootstrapinfo_stream;
    writebox(bootstrapinfo_stream, "abst", abst.str());
    std::string bootstrapinfo = bootstrapinfo_stream.str();

    std::string info("bt");
    std::stringstream manifestname;
    manifestname << docroot << '/' << basedir << '/' << manifest_name;
    std::ofstream manifest_out(manifestname.str().c_str());
    if ( !manifest_out ) {
        throw std::runtime_error("Error opening " + manifestname.str());
    }
    manifest_out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
      "<manifest xmlns=\"http://ns.adobe.com/f4m/1.0\">\n"
      "<id>" << video_id << "</id>\n"
      "<streamType>recorded</streamType>\n"
      "<duration>" << fileinfo_list[0].duration << "</duration>\n"
      "<bootstrapInfo profile=\"named\" id=\"" << info << "\">";
    base64::encode(manifest_out.rdbuf(), bootstrapinfo.c_str(), bootstrapinfo.size());
    manifest_out << "</bootstrapinfo>\n";
    BOOST_FOREACH( const fileinfo& fi, fileinfo_list ) {
        manifest_out <<  
          "<media streamId=\"" << video_id << "\" url=\"" << fi.dirname << "/\" bootstrapinfoId=\"" << info << "\" "
          "bitrate=\"" << int(fi.filesize / fi.duration / 10000 * 8) * 10000 << "\" "
          " />\n";
    }
    manifest_out << "</manifest>\n";
    manifest_out.close();
    if ( manifest_out.fail() || manifest_out.bad() ) {
        std::stringstream errmsg;
        errmsg << "Error writing " << manifestname;
        throw std::runtime_error(errmsg.str());
    }
}
catch ( std::exception& e ) {
    std::cerr << e.what() << "\n";
}
