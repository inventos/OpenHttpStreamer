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
#include <vector>

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

std::string readstring(const char *d) {
    uint16_t strsz = read16(d);
    return std::string(d + 2, strsz);
}

