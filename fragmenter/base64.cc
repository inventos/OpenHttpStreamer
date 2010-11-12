#include "base64.hh"

namespace base64 {

    namespace {
        const char letters[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    }

    void encode(std::streambuf *buf, const char *bytes, unsigned size) {
        unsigned rest = size % 3;
        const char *limit = bytes + size - rest;
        for ( const char *ptr = bytes; ptr < limit; ptr += 3, size -= 3 ) {
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
