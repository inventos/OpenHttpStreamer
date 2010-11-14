/*
 * 
 * copyright (c) 2010 ZAO Inventos (inventos.ru)
 * copyright (c) 2010 jk
 *
 * This file is part of VideoCycle.
 *
 * VideoCycle is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * VideoCycle is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 */

#include "base64.hh"

namespace base64 {

    namespace {
        const char letters[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    }

    void encode(std::streambuf *buf, const char *bytes, unsigned size) {
        unsigned rest = size % 3;
        const char *limit = bytes + size - rest;
        for ( const char *ptr = bytes; ptr < limit; ptr += 3 ) {
            buf->sputc(letters[ (ptr[0] >> 2) & 0x3F ]);
            buf->sputc(letters[ ((ptr[0] << 4) & 0x30) | ((ptr[1] >> 4) & 0xF) ]);
            buf->sputc(letters[ ((ptr[1] << 2) & 0x3C) | ((ptr[2] >> 6) & 0x3) ]);
            buf->sputc(letters[ ptr[2] & 0x3F ]);
        }
        
        switch ( rest ) {
        case 1:
            buf->sputc(letters[ (limit[0] >> 2) & 0x3F ]);
            buf->sputc(letters[(limit[0] << 4) & 0x30]);
            buf->sputc('=');
            buf->sputc('=');
            break;
        case 2:
            buf->sputc(letters[ (limit[0] >> 2) & 0x3F ]);
            buf->sputc(letters[((limit[0] << 4) & 0x30) | ((limit[1] >> 4) & 0xF)]);
            buf->sputc(letters[(limit[1] << 2) & 0x3C]);
            buf->sputc('=');
            break;
        }
    }

}
