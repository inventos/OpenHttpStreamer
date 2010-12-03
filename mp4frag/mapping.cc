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

#include "mapping.hh"
#include <boost/system/system_error.hpp>
#include <boost/system/error_code.hpp>
#include <memory.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <errno.h>

using namespace boost::system;

void Mapping::make_mapping(int fd) {
    struct stat st;
    memset(&st, 0, sizeof(st));
    if ( fstat(fd, &st) == -1 ) {
        throw system_error(errno, get_system_category(), "fstat");
    }
    _size = st.st_size;
    _data = mmap(0, st.st_size, PROT_READ, MAP_SHARED, fd, 0);
    if ( _data == (void *)(-1) ) {
        throw system_error(errno, get_system_category(), "mmap");
    }
}

void Mapping::make_mapping(const char *name) {
    int fd = open(name, O_RDONLY);
    if ( fd == -1 ) {
        throw system_error(errno, get_system_category(), std::string("opening ") + name);
    }

    struct fd_guard {
        int _fd;
        fd_guard(int f) : _fd(f) {}
        ~fd_guard() { close(_fd); }
    } guard(fd);

    make_mapping(fd);
}

Mapping::~Mapping() {
    if ( _data != (void*)-1 ) {
        munmap((void *)_data, _size);
    }
}
