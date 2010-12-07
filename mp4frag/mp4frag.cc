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

#include "serialize.hh"
#include "mp4frag.hh"
#include "mp4.hh"
#include "base64.hh"
#include "mapping.hh"
#include <boost/system/system_error.hpp>
#include <boost/system/error_code.hpp>
#include <boost/foreach.hpp>
#include <sstream>
#include <stdexcept>

using namespace boost::system;

void generate_fragments(std::vector<Fragment>& fragments, boost::shared_ptr<mp4::Context>& ctx, double timestep) {
    double limit_ts = timestep;
    double now;
    double timebase = 0;
    unsigned nsample = 0;
    fragments.clear();

    fragments.push_back(Fragment());
    std::vector<SampleEntry> *samplelist = &(fragments.back()._samples);

    while ( nsample < ctx->nsamples() ) {
        mp4::SampleInfo *si = ctx->get_sample(nsample++);
        now = si->timestamp();

        if ( si->_video && si->_keyframe && now >= limit_ts ) {
            if ( fragments.size() ) {
                fragments.back()._duration = now - timebase;
                timebase = now;
            }
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
    if ( fragments.size() ) {
        fragments.back()._duration = ctx->duration() - timebase;
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
                               pmedia->mapping->data() + se.offset(), se.size());
        }
        else {
            write_audio_packet(sb, false, se.timestamp(),
                               pmedia->mapping->data() + se.offset(), se.size());
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
    for ( unsigned imedia = 0; imedia < medialist.size(); ++imedia ) {
        const boost::shared_ptr<Media>& fi = medialist[imedia];
        manifest_out <<  
          "<media streamId=\"" << video_id << "\" url=\"" << imedia << "/\" bootstrapinfoId=\"" << info << "\" "
          "bitrate=\"" << int(fi->mapping->size() / fi->duration / 10000 * 8) * 10000 << "\" "
          " />\n";
    }
    manifest_out << "</manifest>\n";
    if ( !manifest_out ) {
        throw std::runtime_error("Error writing manifest");
    }
}




boost::shared_ptr<Media> make_fragments(const std::string& filename, unsigned fragment_duration) {
    boost::shared_ptr<Media> finfo(new Media);
    finfo->mapping.reset(new Mapping(filename.c_str()));
    boost::shared_ptr<mp4::Context> ctxt(new mp4::Context);
    off_t offset = 0;
    while ( unsigned nbytes = ctxt->wants() ) {
        if ( unsigned s = ctxt->to_skip() ) {
            offset += s;
            ctxt->skip(0);                    
        }
        if ( offset >= (off_t)finfo->mapping->size() ) {
            break;
        }
        ctxt->feed( finfo->mapping->data() + offset, nbytes );
        offset += nbytes;
    }
    ctxt->finalize();

    finfo->duration = ctxt->duration();

    finfo->videoextra = ctxt->videoextra();
    finfo->audioextra = ctxt->audioextra();

    generate_fragments(finfo->fragments, ctxt, fragment_duration / 1000.0);
    return finfo;
}


void serialize(std::streambuf *sbuf, const std::vector< boost::shared_ptr<Media> >& medialist) {

    const char signature[] = "mp4frag";

    unsigned nmedia = medialist.size();

    uint32_t header_size = strlen(signature) + 1 + 2 + nmedia * 4;

    std::vector< uint32_t> fragmentdir_map;
    std::vector< std::vector<uint32_t> > fragments_by_media;
    BOOST_FOREACH(const boost::shared_ptr<Media>& pmedia, medialist) {
        fragmentdir_map.push_back(header_size);
        header_size += 2 + pmedia->medianame.size() +
                       2 + pmedia->videoextra.size() +
                       2 + pmedia->audioextra.size() +
                       2 + pmedia->fragments.size() * 4;
        fragments_by_media.resize(fragments_by_media.size() + 1);
        std::vector<uint32_t>& last = fragments_by_media.back();
        BOOST_FOREACH(const Fragment& fragment, pmedia->fragments) {
            last.push_back(header_size);
            header_size += 2 // nsamples in the fragment
              + 4 // total sum of all samples size in the fragment
              + fragment._samples.size() * (
                4 + // offset in file
                4 + // size
                4 + // timestamp, ms
                3 + // composition offset
                1   // flags
                );
        }
    }

    sbuf->sputn(signature, strlen(signature));
    sbuf->sputc(1); // version
    write16(*sbuf, nmedia);
    for ( unsigned n = 0; n < nmedia; ++n ) {
        write32(*sbuf, fragmentdir_map[n]);
    }

    for ( unsigned n = 0; n < nmedia; ++n ) {
        const boost::shared_ptr<Media>& pmedia = medialist[n];
        writestring(*sbuf, pmedia->medianame);
        writestring(*sbuf, pmedia->videoextra);
        writestring(*sbuf, pmedia->audioextra);
        write16(*sbuf, pmedia->fragments.size());
        std::vector<uint32_t>& fragment_offsets = fragments_by_media[n];
        assert ( fragment_offsets.size() == pmedia->fragments.size() );
        BOOST_FOREACH(uint32_t offset, fragment_offsets) {
            write32(*sbuf, offset);
        }

    // fragment templates:
    // 4 bytes - offset
    // 4 bytes - size
    // 4 bytes - timestamp
    // 3 bytes - composition timestamp
    // 1 byte  - flags
        for ( unsigned m = 0; m < pmedia->fragments.size(); ++m ) {
            const Fragment& fragment = pmedia->fragments[m];
            write16(*sbuf, fragment._samples.size());
            // total sum of all samples size in the fragment:
            uint32_t fragmentsize_total = 8 /* mdat box tag */ + 
              1 + 3 + 4 + 3 + 5 + pmedia->videoextra.size() + 4 +
              1 + 3 + 4 + 3 + 2 + pmedia->audioextra.size() + 4;
            BOOST_FOREACH(const SampleEntry& se, fragment._samples) {
                fragmentsize_total += 1 + 3 + 4 + 3 + (se.video() ? 5 : 2) + se.size() + 4;
            }
            write32(*sbuf, fragmentsize_total);
            BOOST_FOREACH(const SampleEntry& se, fragment._samples) {
                write32(*sbuf, se.offset());
                write32(*sbuf, se.size());
                write32(*sbuf, se.timestamp());
                write24(*sbuf, se.composition_offset());
                sbuf->sputc( se.video() ? (se.keyframe() ? 3 : 1) : 0 );
            }
        }
    }
}


void get_fragment(std::streambuf *out, 
                  unsigned medianum, unsigned fragnum, const char *index, size_t indexsize,
                  const boost::function<boost::shared_ptr<Mapping> (const std::string&)>& mapping_factory
                  ) {
    if ( memcmp(index, "mp4frag", 7) != 0 || index[7] != 1 ) {
        throw std::runtime_error("Wrong file format");
    }
    uint16_t nmedia = read16(index + 8);
    if (medianum >= nmedia ) {
        throw std::runtime_error("No media");
    }
    uint32_t mediaoffset = read32(index + 10 + medianum * 4);
    assert ( mediaoffset < indexsize );
    const char *mediaptr = index + mediaoffset;
    indexsize -= mediaoffset;

    std::string filename = readstring(mediaptr);
    mediaptr += 2 + filename.size();

    std::string videoextra = readstring(mediaptr);
    mediaptr += 2 + videoextra.size();

    std::string audioextra = readstring(mediaptr);
    mediaptr += 2 + audioextra.size();

    uint16_t nfragments = read16(mediaptr);
    assert ( fragnum < nfragments );
    mediaptr += 2;
    uint32_t fragment_offset = read32(mediaptr + 4 * fragnum);

    const char *fragment = index + fragment_offset;

    uint16_t nsamples = read16(fragment);
    uint32_t totalsize = read32(fragment + 2);
    fragment += 6;

    uint32_t fragment_ts = read32(fragment + 8);

    write32(*out, totalsize);
    out->sputn("mdat", 4);
    write_fragment_prefix(out, videoextra, audioextra, fragment_ts);

    boost::shared_ptr<Mapping> mapping = mapping_factory(filename);
    for ( unsigned iii = 0; iii < nsamples; ++iii ) {
        uint32_t offset = read32(fragment);
        uint32_t size = read32(fragment + 4);
        if ( mapping->size() <= offset + size ) {
            std::stringstream msg;
            msg << "Out of file: mapping.size= " << mapping->size() << ", offset=" << offset << ", size=" << size;
            throw std::runtime_error(msg.str());
        }
        uint32_t timestamp = read32(fragment + 8);
        uint32_t composition_offset = read24(fragment + 12);
        uint8_t flags = fragment[15];
        fragment += 16;
        if ( flags ) {
            write_video_packet(out, flags & 2, false, composition_offset, timestamp,
                               mapping->data() + offset, size);
        }
        else {
            write_audio_packet(out, false, timestamp, mapping->data() + offset, size);
        }
    }
}
