#!/usr/bin/python

# remove unnecessary information from xcodebuild output

import re, sys, os, os.path

clang_re = re.compile(" */.+usr/bin/([a-z+]+).+( -c [^ ]+).* -o (.+)")
link_re = re.compile(" */.+usr/bin/([a-z+]+).* -o (.+)")
basedir = os.path.dirname(os.path.dirname(sys.argv[0]))
homedir = os.path.expanduser("~")
ignores = re.compile(" *(CompileC|GenerateDSYMFile|Touch|RegisterWithLaunchServices|" +
                     "Check dependencies|PhaseScriptExecution|Ld /|cd /|export |" +
                     "builtin-lsRegisterURL|builtin-copy|PBXCp|/usr/bin/touch|ProcessPCH|" +
                     "([0-9]+ warnings? generated)).*")

def write(x):
    sys.stdout.write(x)
    sys.stdout.flush()

for line in sys.stdin:
    if (ignores.match(line) or "IDELogStore" in line or line.isspace()):
        # write(".")
        continue
    line = line.replace(basedir, "")
    line = line.replace(homedir, "~")
    # line = line.replace("\ ", "_")
    m = clang_re.match(line)
    if m:
        # write(line)
        write("%s%s -o %s\n" % (m.group(1),
                                m.group(2).replace("~/Documents/outlaws/", ""),
                                os.path.basename(m.group(3))))
        continue
    m = link_re.match(line)
    if m:
        write("%s -o %s\n" % (m.group(1), os.path.basename(m.group(2))))
        continue
    write(line)
    
