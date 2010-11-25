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
    buf.sputc( (value >> 8) & 0xFF );
    buf.sputc( value & 0xFF );
}

void write24(std::streambuf& buf, uint32_t value) {
    buf.sputc( (value >> 16) & 0xFF );
    buf.sputc( (value >> 8) & 0xFF );
    buf.sputc( value & 0xFF );
}

void write32(std::streambuf& buf, uint32_t value) {
    buf.sputc( (value >> 24) & 0xFF );
    buf.sputc( (value >> 16) & 0xFF );
    buf.sputc( (value >> 8) & 0xFF );
    buf.sputc( value & 0xFF );
}

void write64(std::streambuf& buf, uint64_t value) {
    buf.sputc( (value >> 56) & 0xFF );
    buf.sputc( (value >> 48) & 0xFF );
    buf.sputc( (value >> 40) & 0xFF );
    buf.sputc( (value >> 32) & 0xFF );
    buf.sputc( (value >> 24) & 0xFF );
    buf.sputc( (value >> 16) & 0xFF );
    buf.sputc( (value >> 8) & 0xFF );
    buf.sputc( value & 0xFF );
}

void writebox(std::streambuf& buf, const char *name, const std::string& box) {
    write32(buf, box.size() + 8);
    buf.sputn(name, 4);
    buf.sputn(box.c_str(), box.size());
}



struct Fragment {
    double _ts;
    double _dur;
    Fragment(unsigned ts, unsigned dur) : _ts(ts), _dur(dur) {}
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
};



void close_fragment(fileinfo *finfo, 
                    unsigned segment, unsigned fragment, 
                    const std::string& fragdata, 
                    double ts, double duration) {
    if ( duration == 0 ) {
        throw std::runtime_error("Error writing fragment: duration == 0");
    }
    if ( fragdata.size() ) {

        std::stringstream sdirname;
        sdirname << docroot << '/' << basedir << '/' << finfo->dirname;
        std::string dirname(sdirname.str());
        mkdir(dirname.c_str(), 0755);
        
        std::stringstream filename;
        filename << dirname << "/Seg" << segment << "-Frag" << fragment;
        size_t fragment_size = fragdata.size() + 8;
        char prefix[8];
        prefix[0] = (fragment_size >> 24) & 0xFF;
        prefix[1] = (fragment_size >> 16) & 0xFF;
        prefix[2] = (fragment_size >> 8) & 0xFF;
        prefix[3] = fragment_size & 0xFF;
        prefix[4] = 'm';
        prefix[5] = 'd';
        prefix[6] = 'a';
        prefix[7] = 't';

        std::filebuf out;
        if ( out.open(filename.str().c_str(), std::ios::out | std::ios::binary | std::ios::trunc) ) {
            out.sputn(prefix, 8);
            out.sputn(fragdata.c_str(), fragdata.size());
            if ( !out.close() ) {
                std::stringstream errmsg;
                errmsg << "Error closing " << filename;
                throw std::runtime_error(errmsg.str());
            }
        }
        else {
            std::stringstream errmsg;
            errmsg << "Error opening " << filename;
            throw std::runtime_error(errmsg.str());
        }

        finfo->fragments.push_back(Fragment(ts, duration));
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


void write_fragment_prefix(std::streambuf *sb, boost::shared_ptr<mp4::Context>& ctx, uint32_t ts) {
    if ( ctx->has_video() ) {
        write_video_packet(sb, true, true, 0, ts, 
                           ctx->videoextra().c_str(), ctx->videoextra().size());
    }
    if ( ctx->has_audio() ) {
        write_audio_packet(sb, true, ts, ctx->audioextra().c_str(), ctx->audioextra().size());
    }
}



void write_fragments(fileinfo *finfo, boost::shared_ptr<mp4::Context>& ctx, double timelimit) {
    unsigned segment = 1;
    unsigned fragment = 1;
    double limit_ts = timelimit;
    double old_ts = 0;
    double now;
    unsigned nsample = 0;
    std::stringbuf sb;

    write_fragment_prefix(&sb, ctx, 0);

    while ( nsample < ctx->nsamples() ) {
        mp4::SampleInfo *si = ctx->get_sample(nsample++);
        now = si->timestamp();

        if ( si->_video ) {
            if ( si->_keyframe && now >= limit_ts ) {
                close_fragment(finfo, segment, fragment++, sb.str(), old_ts, now - old_ts);
                old_ts = now;
                limit_ts = now + timelimit;
                sb.str(std::string());
                write_fragment_prefix(&sb, ctx, now * 1000);
            }
            write_video_packet(&sb, si->_keyframe, false, si->_composition_offset * 1000.0 / si->_timescale,
                               now * 1000, finfo->mapping + si->_offset, si->_sample_size);
        }
        else {
            write_audio_packet(&sb, false, now * 1000, finfo->mapping + si->_offset, si->_sample_size);
        }
    }
    close_fragment(finfo, segment, fragment, sb.str(), now, ctx->duration() - now);
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

    write_fragments(&finfo, ctxt, fragment_duration / 1000.0);
/*
    int bitrate = int(st.st_size / ctx->duration);

    std::cout << ctx->_samples.size() << " samples found.\n";
    std::cout << "\nnsamples: " << ctx->nsamples()
      << "\nhas video: " << (ctx->has_video() ? "yes" : "no")
      << "\nhas audio: " << (ctx->has_audio() ? "yes" : "no")
      << "\nwidth: " << ctx->width()
      << "\nheight: " << ctx->height()
      << "\npwidth: " << ctx->pwidth()
      << "\npheight: " << ctx->pheight()
      << "\nbitrate: " << ctx->bitrate()
      << "\nduration: " << ctx->duration()
      << std::endl;

    std::cout << "writing..." << std::endl;
    std::vector<Fragment> fragments = write_fragments(ctx, fragment_duration, finfo.mapping); 
*/
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
        write64(afrt, uint64_t(fileinfo_list[0].fragments[ifragment]._ts * 1000));
        write32(afrt, uint32_t(fileinfo_list[0].fragments[ifragment]._dur * 1000));
        if ( fileinfo_list[0].fragments[ifragment]._dur == 0 ) {
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
