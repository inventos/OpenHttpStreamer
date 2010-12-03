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

#ifndef __mapping_hh__739ccb21_1898_4774_9e40_797f62fbbdaf
#define __mapping_hh__739ccb21_1898_4774_9e40_797f62fbbdaf

#include <utility>
#include <memory.h>

class Mapping {
public:
    template<class ARG> Mapping(ARG a) { make_mapping(a); }
    ~Mapping();
    const char *data() const { return (const char *)(_data); }
    size_t size() const { return _size; }
private:
    void *_data;
    size_t _size;
    void make_mapping(int);
    void make_mapping(const char *);
};

#endif
