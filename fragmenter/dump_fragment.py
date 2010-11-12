#!/usr/bin/python

 # copyright (c) 2010 ZAO Inventos (inventos.ru)
 #
 # This file is part of VideoCycle.
 #
 # VideoCycle is free software; you can redistribute it and/or
 # modify it under the terms of the GNU Lesser General Public
 # License as published by the Free Software Foundation; either
 # version 2.1 of the License, or (at your option) any later version.
 #
 # VideoCycle is distributed in the hope that it will be useful,
 # but WITHOUT ANY WARRANTY; without even the implied warranty of
 # MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 # Lesser General Public License for more details.
 #
 # You should have received a copy of the GNU Lesser General Public
 # License along with FFmpeg; if not, write to the Free Software
 # Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA

import sys
from struct import unpack

fd = open(sys.argv[1])
offset = 0
data = fd.read()
tagsize = unpack(">I", data[:4])[0]
assert len(data) == tagsize, "len=%d, tagsize=%d" % (len(data), tagsize)
assert data[4:8] == "mdat", data[4:8]

def advance(nbytes):
    global data, offset
    data = data[nbytes:]
    offset += nbytes

advance(8)
while data:
    print "%08x" % offset, 
    tag = ord(data[0])
    size = unpack(">I", chr(0) + data[1:4])[0]
    timestamp = unpack(">I", data[7] + data[4:7])[0]
    if tag == 9:
        print "video, composition offset = %s" % (unpack(">I", chr(0) + data[13:16])[0],),
    elif tag == 8:
        print "audio, ",
    else:
        print "tag=%d, " % tag,
    print "size=%d, timestamp=%d" % (size, timestamp)
    total = size + 11
    advance(total)
    assert unpack(">I", data[:4])[0] == total
    advance(4)
