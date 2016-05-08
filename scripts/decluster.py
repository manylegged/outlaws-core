#!/usr/bin/python

# decompress Reassembly clustered sector files
# sector files contain stored map data and are just gzipped text
# sector clusters are a bunch of sector files with a header - up to 256 sectors per cluster

import struct, sys, re
import getopt
import cStringIO, gzip

printcontents = False
opts, args = getopt.getopt(sys.argv[1:], "x")
for (opt, val) in opts:
    if opt == "-x":
        printcontents = True

for fname in args:
    if fname.endswith(".lua"):
        fil = open(fname)
        dat = fil.read()
        fil.close()
        if printcontents:
            print dat,
        else:
            print "scanning " + fname
            print "sector file: %.1f KB uncompressed" % (len(dat) / 1024.0)
        continue
    elif fname.endswith(".lua.gz"):
        fil = gzip.open(fname)
        dat = fil.read()
        fil.close()
        if printcontents:
            print dat,
        else:
            print "scanning " + fname
            print "sector file: %.1f KB uncompressed" % (len(dat) / 1024.0)
        continue
    if not printcontents:
        print "scanning " + fname
        print "index sector start - end (size) reserved: uncompressed size"
    fil = open(fname)
    if not fil:
        sys.stderr.write("unable to open '%s'\n" % fname)
        continue
    data = fil.read()
    fil.close()
    offset = 0
    count = struct.unpack_from("i", data, offset)[0]
    if not printcontents:
        print "%d sectors, %.1f KB" % (count, len(data) / 1024.0)
    offset += 4
    uncompressed_total = 0
    base_x = 0
    base_y = 0
    m = re.search("cluster([0-9]+)x([0-9]+)_([0-9]+)x([0-9]+).dat", fname)
    if m:
        base_x = int(m.group(1))
        base_y = int(m.group(2))
    for idx in xrange(count):
        px, py, offs, size, res = struct.unpack_from("iiiii", data, offset)
        gzf = gzip.GzipFile(fileobj=cStringIO.StringIO(data[offs:offs+size]))
        err = ""
        try:
            sector = gzf.read()
        except Exception as e:
            err = " [" + str(e) + "]"
            sector = ""
        gzf.close()
        uncompressed_total += len(sector)
        if printcontents:
            print sector,
        else:
            print "%3d. {%2d,%2d} {%2d,%2d} %8d - %8d (%5.1f KB) %d: %5.1f KB%s" % \
                (idx, px, py, base_x + px, base_y + py, offs, offs+size, size / 1024.0, res, len(sector) / 1024.0, err)
        offset += 4 * 5
    if not printcontents:
        print "end at %d (%.1f MB), uncompressed total is %.1f MB" % \
            (len(data), len(data) / (1024.0 * 1024.0), uncompressed_total / (1024.0 * 1024.0))
    
