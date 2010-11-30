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

#include "mp4frag.hh"
#include "mp4.hh"
#include "base64.hh"
#include <boost/system/system_error.hpp>
#include <boost/system/error_code.hpp>
#include <boost/foreach.hpp>
#include <sstream>
#include <stdexcept>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>

using namespace boost::system;

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
    unsigned char bytes[4];
    bytes[0] = (value / 0x1000000) & 0xFF;
    bytes[1] = (value / 0x10000) & 0xFF;
    bytes[2] = (value / 0x100) & 0xFF;
    bytes[3] = value & 0xFF;
    buf.sputn((char*)bytes, 4);
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

void writestring(std::streambuf& buf, const std::string& str) {
    write16(buf, str.size());
    buf.sputn(str.c_str(), str.size());
}

uint64_t read64(std::streambuf *in) {
    char d[8];
    in->sgetn(d, 8);
    return (d[0] & 0xff) * 0x100000000000000ULL + 
           (d[1] & 0xff) * 0x1000000000000ULL +
           (d[2] & 0xff) * 0x10000000000ULL +
           (d[3] & 0xff) * 0x100000000ULL +
           (d[4] & 0xff) * 0x1000000ULL +
           (d[5] & 0xff) * 0x10000 +
           (d[6] & 0xff) * 0x100 +
           (d[7] & 0xff);
}

uint32_t read32(std::streambuf *in) {
    unsigned char d[4];
    in->sgetn((char*)d, 4);
    return (d[0] & 0xff) * 0x1000000 + (d[1] & 0xff) * 0x10000 + (d[2] & 0xff) * 0x100 + (d[3] & 0xff);
}

uint32_t read24(std::streambuf *in) {
    char d[3];
    in->sgetn(d, 3);
    return (d[0] & 0xff) * 0x10000 + (d[1] & 0xff) * 0x100 + (d[2] & 0xff);
}

uint16_t read16(std::streambuf *in) {
    char d[2];
    in->sgetn(d, 2);
    return (d[0] & 0xff) * 0x100 + (d[1] & 0xff);
}

std::string readstring(std::streambuf *in) {
    uint16_t strsz = read16(in);
    if ( strsz < 256 ) {
        char buffer[256];
        in->sgetn(buffer, strsz);
        return std::string(buffer, strsz);
    }
    else {
        std::vector<char> buffer(strsz);
        in->sgetn(&buffer[0], strsz);
        return std::string(&buffer[0], strsz);
    }
}


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


Media::~Media() {
    if ( mapping != 0 ) {
        munmap(const_cast<char*>(mapping), filesize);
    }
}


void serialize_fragment(std::streambuf *sb, const boost::shared_ptr<Media>& pmedia, unsigned fragnum) {
    const Fragment& fr = pmedia->fragments.at(fragnum);
    write32(*sb, fr._totalsize + 8 +
            4 + 11 + 5 + pmedia->videoextra.size() +
            4 + 11 + 2 + pmedia->audioextra.size()
           );
    sb->sputn("mdat", 4);

    write_fragment_prefix(sb, pmedia->videoextra, pmedia->audioextra, fr.timestamp_ms());
    BOOST_FOREACH(const SampleEntry& se, fr._samples) {
        if ( se.video() ) {
            write_video_packet(sb, se.keyframe(), false, se.composition_offset(),
                               se.timestamp(),
                               pmedia->mapping + se.offset(), se.size());
        }
        else {
            write_audio_packet(sb, false, se.timestamp(),
                               pmedia->mapping + se.offset(), se.size());
        }
    }
}


void serialize_fragment_to_template(std::streambuf *sb, 
                                 const boost::shared_ptr<Media>& pmedia, 
                                 unsigned fragnum) {
    const Fragment& fr = pmedia->fragments.at(fragnum);
    write32(*sb, fr._totalsize);
    writestring(*sb, pmedia->videoextra);
    writestring(*sb, pmedia->audioextra);
    write32(*sb, fr.timestamp_ms());
    write32(*sb, fr._samples.size());

    BOOST_FOREACH(const SampleEntry& se, fr._samples) {
        write32(*sb, se.timestamp());
        write32(*sb, se.offset());
        write32(*sb, se.size());
        uint32_t comp = se.composition_offset();
        assert(comp < (1 << 24));
        write32(*sb, comp);
        sb->sputc( int(se.video()) | (int(se.keyframe()) << 1) );
    }
}

void serialize_fragment_from_template(std::streambuf *inbuf, std::streambuf *sb, const char *mapping) {
    uint32_t totalsize = read32(inbuf);
    std::string videoextra = readstring(inbuf);
    std::string audioextra = readstring(inbuf);
    write32(*sb, totalsize + 8 +
            4 + 11 + 5 + videoextra.size() +
            4 + 11 + 2 + audioextra.size()
           );
    sb->sputn("mdat", 4);
    uint32_t timestamp_ms = read32(inbuf);
    write_fragment_prefix(sb, videoextra, audioextra, timestamp_ms);
    uint32_t nsamples = read32(inbuf);
    for ( unsigned int nnn = 0; nnn < nsamples; ++nnn ) {
        uint32_t timestamp = read32(inbuf);
        uint32_t offset = read32(inbuf);
        uint32_t size = read32(inbuf);
        uint32_t comp_offset = read32(inbuf);
        uint8_t frame = inbuf->sbumpc();
        if ( frame & 1 ) {
            write_video_packet(sb, frame & 2, false, comp_offset,
                               timestamp,
                               mapping + offset, size);
        }
        else {
            write_audio_packet(sb, false, timestamp, mapping + offset, size);
        }
    }
}

void get_manifest(std::streambuf* sb, const std::vector< boost::shared_ptr<Media> >& medialist,
                  const std::string& video_id) {
    if ( medialist.size() == 0 ) {
        throw std::runtime_error("No media");
    }
    const boost::shared_ptr<Media>& firstmedia = medialist.front();
    unsigned total_fragments = firstmedia->fragments.size();

    std::stringbuf abst;
    abst.sputc(0); // version
    abst.sputn("\0\0", 3); // flags;
    write32(abst, 14); // bootstrapinfoversion
    abst.sputc(0); // profile, live, update
    write32(abst, 1000); // timescale
    write64(abst, firstmedia->duration * 1000); // currentmediatime
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
        write64(afrt, uint64_t(firstmedia->fragments[ifragment].timestamp_ms()));
        write32(afrt, uint32_t(firstmedia->fragments[ifragment].duration() * 1000));
        if ( firstmedia->fragments[ifragment].duration() == 0 ) {
            if ( ifragment != total_fragments - 1 ) {
                throw std::runtime_error("Unexpected: fragment with zero duration");
            }
            afrt.sputc(0);
        }
    }
    writebox(abst, "afrt", afrt.str());

    std::stringbuf bootstrapinfo_stream;
    writebox(bootstrapinfo_stream, "abst", abst.str());
    std::string bootstrapinfo = bootstrapinfo_stream.str();

    const char *info = "bootstrap";

    std::ostream manifest_out(sb);
    manifest_out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
      "<manifest xmlns=\"http://ns.adobe.com/f4m/1.0\">\n"
      "<id>" << video_id << "</id>\n"
      "<streamType>recorded</streamType>\n"
      "<duration>" << firstmedia->duration << "</duration>\n"
      "<bootstrapInfo profile=\"named\" id=\"" << info << "\">";
    base64::encode(manifest_out.rdbuf(), bootstrapinfo.c_str(), bootstrapinfo.size());
    manifest_out << "</bootstrapInfo>\n";
    BOOST_FOREACH( const boost::shared_ptr<Media>& fi, medialist ) {
        manifest_out <<  
          "<media streamId=\"" << video_id << "\" url=\"" << fi->medianame << "/\" bootstrapinfoId=\"" << info << "\" "
          "bitrate=\"" << int(fi->filesize / fi->duration / 10000 * 8) * 10000 << "\" "
          " />\n";
    }
    manifest_out << "</manifest>\n";
    if ( !manifest_out ) {
        throw std::runtime_error("Error writing manifest");
    }
}




boost::shared_ptr<Media> make_fragments(const std::string& filename, unsigned fragment_duration) {
    boost::shared_ptr<Media> finfo(new Media);

    struct stat st;
    if ( stat(filename.c_str(), &st) < 0 ) {
        throw system_error(errno, get_system_category(), "stat");
    }
    finfo->filesize = st.st_size;

    int fd = open(filename.c_str(), O_RDONLY);
    if ( fd == -1 ) {
        throw system_error(errno, get_system_category(), "opening " + filename);
    }
    void *mapping = mmap(NULL, st.st_size, PROT_READ, MAP_SHARED, fd, 0);
    if ( mapping == (void*)-1 ) {
        close(fd);
        throw system_error(errno, get_system_category(), "mmaping " + filename);
    }
    close(fd);
    finfo->mapping = (const char*)mapping;

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
    ctxt->finalize();

    finfo->duration = ctxt->duration();

    finfo->videoextra = ctxt->videoextra();
    finfo->audioextra = ctxt->audioextra();

    generate_fragments(finfo->fragments, ctxt, fragment_duration / 1000.0);
    return finfo;
}
