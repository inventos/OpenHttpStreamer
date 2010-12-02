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

#ifndef __serialize_hh__49618ca1_770a_4058_b2e7_dc5e3a953d44
#define __serialize_hh__49618ca1_770a_4058_b2e7_dc5e3a953d44

#include <streambuf>
#include <string>
#include <stdint.h>

inline void write16(std::streambuf& buf, uint16_t value) {
    buf.sputc( (value / 0x100) & 0xFF );
    buf.sputc( value & 0xFF );
}

inline void write24(std::streambuf& buf, uint32_t value) {
    char bytes[3];
    bytes[0] = (value / 0x10000) & 0xFF;
    bytes[1] = (value / 0x100) & 0xFF;
    bytes[2] = value & 0xFF;
    buf.sputn(bytes, 3);
}

inline void write32(std::streambuf& buf, uint32_t value) {
    unsigned char bytes[4];
    bytes[0] = (value / 0x1000000) & 0xFF;
    bytes[1] = (value / 0x10000) & 0xFF;
    bytes[2] = (value / 0x100) & 0xFF;
    bytes[3] = value & 0xFF;
    buf.sputn((char*)bytes, 4);
}

inline void write64(std::streambuf& buf, uint64_t value) {
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

inline void writebox(std::streambuf& buf, const char *name, const std::string& box) {
    write32(buf, box.size() + 8);
    buf.sputn(name, 4);
    buf.sputn(box.c_str(), box.size());
}

inline void writestring(std::streambuf& buf, const std::string& str) {
    write16(buf, str.size());
    buf.sputn(str.c_str(), str.size());
}

inline uint64_t read64(const char *d) {
    return (d[0] & 0xff) * 0x100000000000000ULL + 
           (d[1] & 0xff) * 0x1000000000000ULL +
           (d[2] & 0xff) * 0x10000000000ULL +
           (d[3] & 0xff) * 0x100000000ULL +
           (d[4] & 0xff) * 0x1000000ULL +
           (d[5] & 0xff) * 0x10000 +
           (d[6] & 0xff) * 0x100 +
           (d[7] & 0xff);
}

inline uint64_t read64(std::streambuf *in) {
    char d[8];
    in->sgetn(d, 8);
    return read64(d);
}

inline uint32_t read32(const char *d) {
    return (d[0] & 0xff) * 0x1000000 + (d[1] & 0xff) * 0x10000 + (d[2] & 0xff) * 0x100 + (d[3] & 0xff);
}

inline uint32_t read32(std::streambuf *in) {
    char d[4];
    in->sgetn(d, 4);
    return read32(d);
}

inline uint32_t read24(const char *d) {
    return (d[0] & 0xff) * 0x10000 + (d[1] & 0xff) * 0x100 + (d[2] & 0xff);
}

inline uint32_t read24(std::streambuf *in) {
    char d[3];
    in->sgetn(d, 3);
    return read32(d);
}

inline uint16_t read16(const char *d) {
    return (d[0] & 0xff) * 0x100 + (d[1] & 0xff);
}

inline uint16_t read16(std::streambuf *in) {
    char d[2];
    in->sgetn(d, 2);
    return read16(d);
}

std::string readstring(const char *);
std::string readstring(std::streambuf *in);


#endif
