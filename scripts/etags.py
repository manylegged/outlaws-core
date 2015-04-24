#!/usr/bin/python

# extract macro data definitions and convert them to etags format.
# for example:
#
# #define BLOCK_FEATURES(F)                       \
#     F(COMMAND,   1<<0)                          \
#     F(THRUSTER,  1<<1)                          \
#
# or
#
# #define SERIAL_CANNON_FIELDS(F)                                         \
#     F(float,   roundsPerSec,   0, _) /* this many rounds per second, on average, regardless of burst size */ \
#     F(uint,    roundsPerBurst, 1, _) /* each burst shoots this many bullets, evenly spread out over the time slice */ \
#

import subprocess
import re
import os
import sys

files = {}

def abspath(x):
    if x.startswith("c:/") or x.startswith("C:/"):
        return x
    path = os.path.abspath(x)
    if path.startswith("/cygdrive"):
        path = path.replace("/cygdrive/", "")
        path = path[0] + ":" + path[1:]
    return path

def doLines(regexp, callback):
    fnames = [abspath(x) for x in sys.argv[1:]]
    if (len(fnames) == 0):
        return
    try:
        cmd = (r"grep -nbH '%s' " % regexp) + " ".join(fnames)
        # print cmd
        lines = subprocess.check_output(["/bin/sh", "-c", cmd])
    except Exception, e:
        return

    n = True if ":" in fnames[0] else False
        
    for line in lines.split('\n'):
        if not len(line) or line.isspace():
            continue
        fields = line.split(":")
        if n:
            # windows support :-/
            fields[0] += ":" + fields[1]
            fields = fields[:1] + fields[2:]
        fil, linenum, byte = fields[:3]
        txt = ":".join(fields[3:])
        txt = txt[0:-1].rstrip()
        if txt.startswith(".."):
            continue
        defn = callback(txt)
        if defn:
            etag = "%s\x7f%s\x01%s,%s\n" % (txt, defn, linenum, byte)
            files[fil] = files.get(fil, "") + etag
        
    
def doSerialFields(txt):
    if txt.count(",") == 1:
        m = re.search(" F[(]([^, ]+)[ ,]*([^, ]+)?[)]", txt)
        if not m:
            sys.stderr.write(txt)
        if not m.group(2) or "<<" in m.group(2) or m.group(2).isdigit():
            return m.group(1)
        else:
            return m.group(2)
    else:
        return re.search(" F[(][^,]+, *([^, ]+)", txt).group(1)

    
typenames = set(("bool", "int", "uint", "float", "double", "uchar", "char", 
                 "int2", "float2", "lstring", "string", "uint64"))

def doCvars(txt):
    if "#define" in txt:
        return None
    m = re.search("(DEFINE|DECLARE)_[A-Z_0-9]*[(]([^,]*), *([^,)]*)", txt)
    if m:
        return m.group(3) if (m.group(2) in typenames or
                              m.group(3).startswith("k") or
                              re.search("[]{}().]", m.group(2))) else m.group(2)
    return None
        

if __name__ == '__main__':

    doLines(r"^ *F(.*).*\\", doSerialFields)
    doLines(r"DEFINE\|DECLARE", doCvars)
    for fil in files:
        print "\x0c"
        print "%s,%d" % (fil, len(files[fil]) + 1)
        print files[fil],

