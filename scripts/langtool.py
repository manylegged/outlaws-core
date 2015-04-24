#!/opt/local/bin/python2.7

# source -> .po
# po -> .lua

import sys, os
import getopt
import subprocess
import glob, re
import codecs

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(sys.argv[0])))

sys.path.append(os.path.join(ROOT, "server", "sync"))
from slpp import slpp as lua

import polib

languages = [ "de_DE", "fr_FR", "ru_RU", "nl_NL", "pl_PL", "sv_SE", "ko_KR", "ja_JP", "zh_CN", "zh_TW" ]
luafiles = ["tips", "popups", "messages", "tutorial"] # text.lua is special

imprt = False
export = False

def mkdir_p(path):
    try:
        os.makedirs(path)
    except OSError as exc: # Python >2.5
        import errno
        if exc.errno == errno.EEXIST and os.path.isdir(path):
            pass
        else: raise
        

def lua2po(luaf, pof):
    with open(luaf) as fil:
        luadata = fil.read()
    newpo = polib.POFile()
    if "web_header" in luadata:
        luadata = luadata[:luadata.index("web_header")]
    for st in re.findall('"[^"]+"', luadata):
        st = st[1:-1].replace("\\\n", "")
        if len(st) > 2:
            newpo.append(polib.POEntry(msgid=st))

    if os.path.exists(pof):
        pofil = polib.pofile(pofile=pof)
        pofil.merge(newpo)
    else:
        pofil = newpo
    pofil.header = ("\nReassembly data/%s localization file\n" % os.path.basename(luaf)) + """
Copyright (C) 2015 Arthur Danskin
Content-Type: text/plain; charset=utf-8
"""
    pofil.save(pof)

    
def po2lua_replace(outlua, baselua, pof):
    with open(baselua) as fil:
        dat = fil.read()
    count = 0
    for pe in polib.pofile(pof):
        if pe.msgstr:
            count += 1
            dat = dat.replace(pe.msgid, pe.msgstr, 1)
    if count == 0:
        return
    mkdir_p(os.path.dirname(outlua))
    with codecs.open(outlua, "w", "utf-8") as fil:
        fil.write(dat)

        
def po2lua_simple(outlua, pof):
    dct = {}
    print outlua, pof
    for pe in polib.pofile(pof):
        if pe.msgstr:
            dct[pe.msgid] = pe.msgstr
    if dct:
        mkdir_p(os.path.dirname(outlua))
        fil = codecs.open(outlua, "w", "utf-8")
        fil.write(lua.encode(dct))
        fil.close()

def printhelp():
    print """%s
-i : import .po files to game format
-e : export .po files from game source
-l <lang> : select language (e.g. en_US)
-h : print this message""" % sys.argv[0]
    exit(0)
            
opts, args = getopt.getopt(sys.argv[1:], "iel:h")
for (opt, val) in opts:
    if opt == "-h":
        printhelp()
    elif opt == "-l":
        languages = [ val ]
        print "processing %s" % val
    elif opt == "-i":
        imprt = True
    elif opt == "-e":
        export = True

if export:
    sources = []
    for d in ("game", "core"):
        for e in (".cpp", ".h"):
            pat = os.path.join(ROOT, "%s/*%s") % (d, e)
            sources.extend(glob.glob(pat))
    print "exporting from %d files" % len(sources)
    for lang in languages:
        lroot = os.path.join(ROOT, "lang", lang)
        outpt = os.path.join(lroot, "text.po")
        mkdir_p(lroot)
        args = ["xgettext", "--keyword=_", "--output=" + outpt]
        # if os.path.exists(outpt):
            # args.append("--join-existing")
        ret = subprocess.call(args + sources)
        if ret != 0:
            print "%s\nreturned exit code %d" % (" ".join(args + sources), ret)
            exit(ret)
        for fil in luafiles:
            lua2po(os.path.join(ROOT, "data", fil + ".lua"),
                   os.path.join(lroot, fil + ".po"))
        print "wrote %d .po files for %s" % (len(luafiles), lang)
    print "export complete"
elif imprt:
    for lang in languages:
        dbase = os.path.join(ROOT, "data")
        droot = os.path.join(dbase, "lang", lang)
        lroot = os.path.join(ROOT, "lang", lang)
        po2lua_simple(os.path.join(droot, "text.lua"), os.path.join(lroot, "text.po"))
        for fil in luafiles:
            po2lua_replace(os.path.join(droot, fil + ".lua"),
                           os.path.join(dbase, fil + ".lua"),
                           os.path.join(lroot, fil + ".po"))
    print "import complete"
        

else:
    printhelp()        
