#!/usr/bin/python

# source code line counting script

import glob
import getopt
import sys
import os
import re
import math
from datetime import datetime

graph = {}

which_hg = "/opt/local/bin/hg" if sys.platform.startswith("darwin") else "hg"

class Counts:
    def __init__(self, fl):
        self.fil = fl
        self.total = 0
        self.code = 0
        self.comments = 0
        self.space = 0
        self.byte = 0
        self.subs = []
        self.parent = None
    def __iadd__(self, o):
        self.total += o.total
        self.code += o.code
        self.comments += o.comments
        self.space += o.space
        self.byte += o.byte
        if o.code:
            self.subs.append(o)
            o.parent = self
        return self
    def fmt(self, maxw):
        st = "%" + ("-" if self.fil.endswith("/") else "") + ("%d" % (1+maxw)) + "s%9d%9d%9d%9d%9d\n"
        return st % (self.fil, self.code, self.comments, self.space, self.total, self.byte/1024)
    def rwidth(self, depth):
        maxw = len(self.fil)
        if depth > 0:
            for cn in self.subs:
                maxw = max(maxw, cn.rwidth(depth-1))
        return maxw
    def rfmt(self, depth, maxw=0):
        st = ""
        if maxw == 0:
            maxw = self.rwidth(depth)
            st = (("%%%ds" % (1+maxw)) + "%9s%9s%9s%9s%9s\n") % ("file", "code", "comment", "space", "total", "kbytes")
        childs = 0
        pchilds = 0
        if depth > 0:
            self.subs.sort(key=lambda x: x.code)
            for cn in self.subs:
                s = cn.rfmt(depth-1, maxw)
                childs += 1
                if s:
                    pchilds += 1
                    st += s
        if self.parent and childs == 0 and not printfiles:
            # print "abort", self.fil
            return st
        if pchilds == 0 or pchilds > 1:
            st += self.fmt(maxw)
        return st
        

def countlines(c, lines):
    includes = []
    commentst = "#" if any(c.fil.endswith(x) for x in (".py", ".sh", ".cgi")) else "//"
    for line in lines:
        c.total += 1
        line = line.strip()
        c.byte += len(line)
        if line.startswith('#include "'):
            includes.append(line[10:-1])
        if line.isspace() or len(line) == 0:
            c.space += 1
        elif (line.startswith(commentst) or 
              (line.startswith("/*") and line.endswith("*/"))):
            c.comments += 1
        # elif ";" in line:
        else:
            c.code += 1
    return c

def countfile(fil, rev):
    c = Counts(fil)

    if os.path.isdir(fil):
        dirn = os.path.normpath(fil)
        dr = Counts(dirn + "/")
        for ex in ["c", "m", "cpp", "h", "hpp", "inl", "py", "sh", "cgi", "gm"]:
            for fil in glob.glob("%s/*.%s" % (dirn, ex)):
                dr += countfile(fil, rev)
        if recursive:
            for dn in glob.glob("%s/*/" % dirn):
                dr += countfile(dn, rev)
        return dr

    if fil == '-':
        lines = sys.stdin
    elif rev:
        lines = os.popen3("%s cat -r %s %s" % (which_hg, rev, fil))[1]
    else:
        lines = open(fil)
    graph[fil] = countlines(c, lines)
    lines.close()
    return c

printdepth = 1
printfiles = False
printdot   = False
recursive  = False
revision = None
rstep = 20
currev = 0

opts, args = getopt.getopt(sys.argv[1:], "vVp:r:Rdhs:")
for (opt, val) in opts:
    if opt == "-v":
        printdepth = 99999999
    elif opt == "-V":
        printfiles = True
        printdepth = 99999999
    elif opt == "-p":
        printdepth = int(val)
    elif opt == "-s":
        rstep = int(val)
    elif opt == "-r":
        if ":" in val:
            revs = [(int(x) if x else -1) for x in val.split(":")]
            currev = 1 + int(re.match("changeset: *([0-9]+)", os.popen("%s log -l1" % which_hg).read()).group(1))
            if revs[0] < 0:
                revs[0] += currev
            if revs[1] < 0:
                revs[1] += currev
            revision = [min(revs[0], revs[1]), max(revs[0], revs[1])]
        else:
            revision = int(val)
    elif opt == "-R":
        recursive = True
    elif opt == "-d":
        printdot = True
        printdepth = 1
    elif opt == "-h":
        print "%s: -fdh [files]"
        print "  -v: print individual dir counts"
        print "  -V: print individual file and dir counts"
        print "  -p <depth>: set print depth"
        print "  -d: generate dot #include graph"
        print "  -R: recursive"
        print "  -r <hg revision>[:<hg revision>] use specific revision, or print range"
        print "  -h: show help"
        exit(0)

def counttotal(rev):
    tot = Counts("TOTAL")
    if args:
        for fil in args:
            tot += countfile(fil, rev)
    else:
        for dirn in (
                "core", "game",
                "sdl_os", "linux/src", "osx/Outlaws", "ios", "win32/Outlaws2/Main",
                "scripts", "server", "server/sync"
                # "glm", "glm/gtc", "glm/detail",
                # "chipmunk/src", "chipmunk/include/chipmunk",
                # "cAudio/cAudio/Headers", "cAudio/cAudio/src", "cAudio/cAudio/include"
        ):
            tot += countfile(dirn, rev)
    return tot

if (isinstance(revision, list)):
    step = int(math.ceil((revision[1] - revision[0]) / float(rstep)))
    print "revision\tdate\ttotal [%d:%d by %d]" % (revision[0], revision[1], step)
    revs = range(revision[0], revision[1], step)
    if revs[-1] != revision[1]:
        revs.append(revision[1])
    for idx in revs:
        print "%d\t" % idx,
        #  Wed Sep 25 08:54:46 2013 -0700
        hglog = os.popen("%s log -r%d" % (which_hg, idx)).read()
        date = datetime.strptime(re.search("date:[ \t]*([^\n]+) -?[0-9]+", hglog).group(1), "%c")
        print "%s\t" % date.strftime("%m/%d/%Y"),
        sys.stdout.flush()
        tot = counttotal(idx if idx != currev else None)
        print "%d" % (tot.code)
else:
    print "counting lines of code"
    if revision:
        print "using hg revision %s" % (revision,)
    tot = counttotal(revision)
    print tot.rfmt(printdepth)

if printdot:
    f = open("temp.dot", "w")
    f.write("digraph G {\n")
    clusters = {}
    for fil in graph:
        if "StdAfx" in fil:
            continue
        path = fil.split("/")
        dirn = "/".join(path[0:-1])
        fnam = path[-1]
        if dirn in clusters:
            clusters[dirn].append((fil, fnam))
        else:
            clusters[dirn] = [(fil, fnam)]
        f.write('"%s" [color=%s]\n' % (fnam, fnam.endswith(".h") and "red" or "blue"))
        for inc in graph[fil]:
            if "." in inc and ".." not in inc and "StdAfx" not in inc:
                f.write('"%s" -> "%s"\n' % (fnam, inc))
    for cl in clusters:
        f.write('subgraph "cluster%s" {\n' % cl)
        f.write('label = "%s"\n' % cl)
        if cl == "core":
            f.write("node [rank=min]\n")
        for (fil, fnam) in clusters[cl]:
            # if fnam.endswith(".h") and fil.replace(".h", ".cpp") in graph:
            #     f.write('subgraph "cluster%s" { "%s"; "%s" }\n' % (fnam[0:-2], fnam, fnam.replace(".h", ".cpp")))
            f.write('"%s"\n' % fnam)
        f.write("}\n")
    f.write("}\n")
    f.close()
    print "running dot..."
    os.system("dot -Tpdf temp.dot > temp.pdf")
    print "wrote temp.pdf"
        
        
