#!/usr/bin/python
import sys
from struct import unpack

fd = open(sys.argv[1])
data = fd.read()
tagsize = unpack(">I", data[:4])[0]
assert len(data) == tagsize, "len=%d, tagsize=%d" % (len(data), tagsize)
assert data[4:8] == "mdat", data[4:8]
data = data[8:]
while data:
    tag = ord(data[0])
    size = unpack(">I", chr(0) + data[1:4])[0]
    timestamp = unpack(">I", data[7] + data[4:7])[0]
    if tag == 9:
        print "video, ",
    elif tag == 8:
        print "audio, ",
    else:
        print "tag=%d, " % tag,
    print "size=%d, timestamp=%d" % (size, timestamp)
    total = size + 11
    data = data[total:]
    assert unpack(">I", data[:4])[0] == total
    data = data[4:]
