#!/usr/bin/python
#!/opt/local/bin/python2.7

# source -> .po
# po -> .lua

import sys, os
import getopt
import subprocess
import glob, re
import codecs, io
import vdf
import shutil

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(sys.argv[0])))

sys.path.append(os.path.join(ROOT, "server", "sync"))
from slpp import slpp as lua

import polib

all_languages = [ "ru", "fr", "de", "es", "pt", "pl", "ja", "tr", "zh" ]
steam_languages = {"ru":"russian", "en":"english", "de":"german", "fr":"french", "pl":"polish", "sv":"swedish",
                   "ko":"korean", "ja":"japanese", "zh":"chinese", "es":"spanish", "pt":"portugese", "tr":"turkish"}
languages = all_languages
set_languages = False
luafiles = ["tips", "popups", "messages", "tutorial"] # text.lua is special
txtfiles = ["ships"]

imprt = False
export = False
dozip = False
dryrun = False
validate = False

def count_occur(v):
    y = {}
    for x in v:
        y[x] = y.get(x, 0) + 1
    return y

fmt_re = re.compile("%[-'+ #0]*[0-9.]*[hljztL]*[a-zA-Z]")
ws_re = re.compile("(\s*).*?(\s*)$", re.DOTALL)
color_re = re.compile("([\^][0-9])[^\n^]*([\^]7)")
key_re = re.compile("[$][a-zA-Z0-9]+")
capital_re = re.compile("\s*(\w)")

def check_format_strings(fil, linenum, orig, repl):
    fil = os.path.relpath(fil)
    orig_f = fmt_re.findall(orig)
    repl_f = fmt_re.findall(repl)
    if (orig_f != repl_f):
        print "%s:%d: error: format mismatch %r -> %r" % (fil, linenum, orig, repl)
    elif orig_f:
        # print "%s -> %s" % (orig_f, repl_f)
        pass
    orig_c = count_occur(x.group(1) for x in color_re.finditer(orig))
    repl_c = count_occur(x.group(1) for x in color_re.finditer(repl))
    # print '%r -> %r' % (orig_c, repl_c)
    if (orig_c != repl_c):
        print "%s:%d: warning: color mismatch (%s -> %s) %r -> %r" % (fil, linenum, orig_c, repl_c, orig, repl)
    # print "'%s' -> '%s'" % (orig, repl) 
    orig_ws = ws_re.match(orig).groups()
    repl_ws = ws_re.match(repl).groups()
    # print orig_ws, "->", repl_ws
    if orig_ws != repl_ws:
        print "%s:%d: error: whitespace mismatch %r -> %r" % (fil, linenum, orig, repl)
    orig_cap_m = capital_re.match(orig)
    repl_cap_m = capital_re.match(repl)
    if orig_cap_m and repl_cap_m:
        orig_cap = orig_cap_m.group(1)
        repl_cap = repl_cap_m.group(1)
        orig_is_upper = orig_cap == orig_cap.upper()
        repl_is_upper = repl_cap == repl_cap.upper()
        if orig_is_upper != repl_is_upper:
            print "%s:%d: warning: %r capitalization mismatch %s -> %s" % (fil, linenum, orig, orig_cap, repl_cap)
    orig_keys = key_re.findall(orig)
    repl_keys = key_re.findall(repl)
    if (orig_keys != repl_keys):
        print "%s%d: error: key escape mismatch (%s -> %s) %r -> %r" % (fil, linenum, orig_keys, repl_keys, orig, repl)
        

        
def read_lua(path):
    fil = io.open(path, encoding="utf-8")
    text = lua.decode(fil.read())
    fil.close()
    return text


def check_dict(name, lang):
    path = os.path.join(ROOT, "data/lang", lang, name)
    text = read_lua(path)
    print "checking %s/%s with %d items" % (lang, name, len(text))
    for en, ln in text.iteritems():
        # print ('%r, %r' % (en, ln)), type(en), type(ln)
        check_format_strings(path, 0, en, ln)


def check_len(epath, lpath, etext, ltext):
    print "checking %s with %d items" % (os.path.relpath(lpath), len(ltext))
    if len(etext) != len(ltext):
        print "%s has %d items but %s has %d items" % (os.path.relpath(epath), len(etext),
                                                       os.path.relpath(lpath), len(ltext))
        
def check_list(name, lang):
    epath = os.path.join(ROOT, "data", name)
    lpath = os.path.join(ROOT, "data/lang", lang, name)
    etext = read_lua(epath)
    ltext = read_lua(lpath)
    check_len(epath, lpath, etext, ltext)
    # i = 1
    for en, ln in zip(etext, ltext):
        # print i, en
        # i += 1
        check_format_strings(lpath, en, ln)

        
def check_values(name, lang):
    epath = os.path.join(ROOT, "data", name)
    lpath = os.path.join(ROOT, "data/lang", lang, name)
    etext = read_lua(epath)
    ltext = read_lua(lpath)
    # for x in etext:
        # print "[" + x + "]", etext[x]
    check_len(epath, lpath, etext, ltext)
    for key in etext:
        if val in ltext:
            check_format_strings(lpath, etext[key], ltext[key])


def mkdir_p(path):
    try:
        os.makedirs(path)
    except OSError as exc: # Python >2.5
        import errno
        if exc.errno == errno.EEXIST and os.path.isdir(path):
            pass
        else: raise

def open_po(pof):
    fil = open(pof)
    s = fil.read()
    fil.close()
    if s.startswith("\xef\xbb\xbf"):
        s = s[3:]
        fil = open(pof, "w")
        fil.write(s)
        fil.close()
    return polib.pofile(pof)
        
def header(luaf):
    return ("\nReassembly data/%s localization file\n" % os.path.basename(luaf)) + """
Copyright (C) 2015-2016 Arthur Danskin
Content-Type: text/plain; charset=utf-8
"""


def list2po(name, words, pof, comment=""):
    entries = {}
    if os.path.exists(pof):
        for pe in open_po(pof):
            entries[pe.msgid] = pe.msgstr

    words = sorted(list(set(words)))
    pofil = polib.POFile()
    for word in words:
        # skip A-100 type names
        if len(word) < 7 and (word[1] == "-" or word[2] == "-"):
            continue
        args = { "msgid":word, "tcomment":comment }
        if word in entries:
            args["msgstr"] = entries[word]
        pofil.append(polib.POEntry(**args))
    pofil.header = header(name)
    pofil.save(pof)
    print "wrote " + os.path.relpath(pof) + " (%d entries updated)" % len(entries)

    
def lua2po(luaf, pof):
    with open(luaf) as fil:
        luadata = fil.read()
    newpo = polib.POFile()
    if "web_header" in luadata:
        luadata = luadata[:luadata.index("web_header")]
    luadata = re.sub("--.*\n", "", luadata)
    for st in re.findall('"[^"]+"', luadata):
        st = st[1:-1].replace("\\\n", "")
        if len(st) > 2:
            newpo.append(polib.POEntry(msgid=st))

    if os.path.exists(pof):
        pofil = open_po(pof)
        pofil.merge(newpo)
    else:
        pofil = newpo
    pofil.header = header(luaf)
    pofil.save(pof)


def load_achievements():
    return vdf.load(open(os.path.join(ROOT, "lang", "329130_loc_all.vdf")))["lang"]["english"]["Tokens"]


def achievements_vdf2po(pof):
    if os.path.exists(pof):
        return
    pofil = polib.POFile()
    for key, val in load_achievements().iteritems():
        pofil.append(polib.POEntry(msgid=val, tcomment=key))
        pofil.header = header("achievements")
    if dryrun:
        return
    pofil.save(pof)
    print "wrote " + pof


def achievements_po2vdf(vdff, langs):
    english = load_achievements().items()
    dat = {}
    for lang in langs:
        trans = dict((pe.msgid, pe.msgstr) for pe in open_po(os.path.join(ROOT, "lang", lang, "achievements.po")))
        dat[steam_languages[lang]] = {"Tokens":dict((k, trans[v]) for k, v in english)}
    if dryrun:
        return
    vdf.dump({"lang":dat}, io.open(vdff, 'w', encoding='utf-8'), pretty=True)
    print "wrote " + vdff
    
    
def po2lua_replace(outlua, baselua, pof):
    with open(baselua) as fil:
        dat = fil.read()
    count = 0
    for pe in open_po(pof):
        if pe.msgstr:
            check_format_strings(pof, pe.linenum, pe.msgid, pe.msgstr)
            count += 1
            msgstr = pe.msgstr.replace('"', '\\"')
            dat = dat.replace('"' + pe.msgid + '"', '"' + msgstr + '"', 1)
            dat = dat.replace('"\\\n' + pe.msgid + '"', '"\\\n' + msgstr + '"', 1)
    if count == 0 or dryrun:
        return
    mkdir_p(os.path.dirname(outlua))
    with io.open(outlua, "w", encoding="utf-8") as fil:
        fil.write(dat)

        
def escape(x):
    return x.replace('"', '\\"').replace("\n", "\\n")


def po2lua_simple(outlua, pofs):
    dct = {}
    print outlua, pofs
    for pof in pofs:
        if not os.path.exists(pof):
            continue
        for pe in open_po(pof):
            if pe.msgstr:
                check_format_strings(pof, pe.linenum, pe.msgid, pe.msgstr)
                dct[pe.msgid] = pe.msgstr
    if not dct or dryrun:
        return
    mkdir_p(os.path.dirname(outlua))
    fil = codecs.open(outlua, "w", "utf-8")
    fil.write("{" + ",\n".join('"%s" = "%s"' % (escape(k), escape(v)) for k, v in sorted(dct.items()) if k != v) + "}")
    fil.close()


def po2txt(outtxt, pof):
    if not os.path.exists(pof):
        return
    fil = codecs.open(outtxt, "w", "utf-8")
    for pe in open_po(pof):
        if pe.msgstr:
            fil.write(pe.tcomment + "\n")
            fil.write(pe.msgstr + "\n\n")
    fil.close()
    
    
def printhelp():
    print """%s [-ielh] <langage0> [language1...]
-i : import .po files to game format
-e : export .po files from game source
-l : export translated lua files
-n : only check, don't modify
-h : print this message""" % sys.argv[0]
    exit(0)

opts, cargs = getopt.getopt(sys.argv[1:], "iehznt")
for (opt, val) in opts:
    if opt == "-h":
        printhelp()
    elif opt == "-i":
        imprt = True
    elif opt == "-e":
        export = True
    elif opt == "-z":
        dozip = True
    elif opt == "-n":
        dryrun = True
    elif opt == "-t":
        validate = True

if cargs:
    if cargs[0][0] == '-':
        for ar in cargs:
            languages.remove(ar[1:])
    else:
        languages = cargs
    set_languages = True
    print languages

if export:
    sources = []
    for d in ("game", "core"):
        for e in (".cpp", ".h"):
            sources.extend(glob.glob(os.path.join(ROOT, d, "*%s" % e)))
    for l in ["factions.lua"]:
        sources.append(os.path.join(ROOT, "data", l))
    print "exporting languages: %s" % ", ".join(languages)
    print "exporting from %d files" % len(sources)
    pofils = []
    for lang in languages:
        lroot = os.path.join(ROOT, "lang", lang)
        outpt = os.path.join(lroot, "text.po")
        pofils.append(outpt)
        mkdir_p(lroot)
        args = ["xgettext", "--keyword=_", "--output=" + outpt, "--omit-header",
                "--no-location", "--no-wrap",
                "--copyright-holder=2015-2016 Arthur Danskin"]
        if os.path.exists(outpt):
            args.append("--join-existing")
        ret = subprocess.call(args + sources) if not dryrun else 0
        # ret = 0
        if ret != 0:
            print "%s\nreturned exit code %d" % (" ".join(args + sources), ret)
            exit(ret)
        for fil in luafiles:
            pof = os.path.join(lroot, fil + ".po")
            pofils.append(pof)
            lua2po(os.path.join(ROOT, "data", fil + ".lua"), pof)
        print "wrote %d lua .po files for %s" % (len(luafiles), lang)
        for fil in txtfiles:
            pof = os.path.join(lroot, fil + ".po")
            pofils.append(pof)
            with open(os.path.join(ROOT, "lang", fil + ".txt")) as fil:
                snames = [l.strip() for l in fil]
            lua_names = read_lua(os.path.join(ROOT, "data/shipnames.lua"))
            # for key, val in lua_names.iteritems():
                # print repr(key), repr(val)
            snames.extend(lua_names.values())
            list2po("shipnames.lua", snames, pof, comment="this is the name of a ship")
        achievements_vdf2po(os.path.join(lroot, "achievements.po"))
    # subprocess.call(["unix2dos", "-q"] + pofils)
    print "export complete"
elif imprt:
    if not set_languages:
        languages = [x for x in os.listdir(os.path.join(ROOT, "lang/")) if len(x) == 2]
    print "importing languages: %s" % ", ".join(languages)
    for lang in languages:
        dbase = os.path.join(ROOT, "data")
        droot = os.path.join(dbase, "lang", lang)
        lroot = os.path.join(ROOT, "lang", lang)
        # german uses english ship names at request of translator
        mytxt = ["text"] if (lang == "de") else txtfiles + ["text"]
        po2lua_simple(os.path.join(droot, "text.lua"),
                      [os.path.join(lroot, x + ".po") for x in mytxt])
        for fil in luafiles:
            po2lua_replace(os.path.join(droot, fil + ".lua"),
                           os.path.join(dbase, fil + ".lua"),
                           os.path.join(lroot, fil + ".po"))
        po2txt(os.path.expanduser("~/Downloads/store_" + lang + ".txt"), os.path.join(lroot, "store.po"))

    achievements_po2vdf(os.path.expanduser("~/Downloads/achievements.vdf"), languages)
    print "import complete"
elif validate:
    for lang in languages:
        check_dict("text.lua", lang)
        check_values("messages.lua", lang)
        check_list("tips.lua", lang)
        

elif not dozip:
    printhelp()

if dozip:
    zipfile = os.path.expanduser("~/Downloads/Reassembly_lang.zip")
    if os.path.exists(zipfile):
        os.remove(zipfile)
    pos = []
    for lang in languages:
        pos.extend(glob.glob(os.path.relpath(os.path.join(ROOT, "lang", lang, "*.po"))))
    subprocess.call(["zip", zipfile] + pos)
    print "wrote %s" % (zipfile)
