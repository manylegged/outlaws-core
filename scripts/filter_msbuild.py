#!/usr/bin/python

# remove unnecessary information from msbuild output

import re, sys, os, os.path

basedir = os.path.dirname(os.path.dirname(sys.argv[0]))
projdir = "win32/Outlaws2/Outlaws2"
filterpaths = [re.compile(re.escape(x), re.IGNORECASE) for x in
               ["win32\\Outlaws2\\Main\\\\..\\..\\..\\",
                "C:\\Program Files (x86)\\Microsoft Visual Studio 12.0\\VC\\bin\\",
                "C:\\Users\\Arthur\\Documents\\outlaws\\",
                "..\\..\\..\\"]]
file_re = re.compile(r" *([.][.][.\\]+[^(): ]+).*")
ignore_re = re.compile("(( *(Build|Project|Done|Touching|Deleting|Creating) )|" +
                       "([A-Z][a-zA-Z]+(Status|Configuration):)|ClCompile:|Lib:|.*unsuccessfulbuild.*).*")

def write(x):
    sys.stdout.write(x)
    sys.stdout.flush()

def short_path(fname):
    return os.path.normpath(os.path.join(basedir, projdir, fname.replace("\\", "/")))

for line in sys.stdin:
    if ignore_re.match(line):
        continue
    if "All outputs are up-to-date" in line:
        continue
    for x in filterpaths:
        line = x.sub("", line)
    line = line.replace("\\", "/")
    m = file_re.match(line)
    if m:
        write(short_path(m.group(1)) + line[m.end(1):])
        continue
    write(line)
    
