#!/usr/bin/python
# -*- coding: utf-8 -*-

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

# all_languages = [ "ru", "fr", "de", "es", "pt", "pl", "ja", "tr", "zh" ]
all_languages = [ "ru", "fr", "de", "pl", "ja", "zh" ]
steam_languages = {"ru":"russian", "en":"english", "de":"german", "fr":"french", "pl":"polish", "sv":"swedish",
                   "ko":"korean", "ja":"japanese", "zh":"schinese", "es":"spanish", "pt":"portugese", "tr":"turkish"}
languages = all_languages
set_languages = False
luafiles = ["tips", "popups", "messages", "tutorial"] # text.lua is special
txtfiles = ["ships"]

imprt = False
export = False
dozip = False
dryrun = False
validate = False
lua2po_dir = None
docount = False
doone = False

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
        print (u"%s:%d: error: format mismatch %r -> %r" % (fil, linenum, orig, repl)).encode("utf-8")
    elif orig_f:
        # print "%s -> %s" % (orig_f, repl_f)
        pass
    orig_c = count_occur(x.group(1) for x in color_re.finditer(orig))
    repl_c = count_occur(x.group(1) for x in color_re.finditer(repl))
    # print '%r -> %r' % (orig_c, repl_c)
    # orig_u = orig.encode("utf-8")
    # repl_u = repl.encode("utf-8")
    orig_u = "'" + orig.replace("\n", "\\n") + "'"
    repl_u = "'" + repl.replace("\n", "\\n") + "'"
    if (orig_c != repl_c):
        print (u"%s:%d: warning: color mismatch (%s -> %s) %s -> %s" % (fil, linenum, orig_c, repl_c, orig_u, repl_u)).encode("utf-8")
    # print "'%s' -> '%s'" % (orig, repl) 
    orig_ws = ws_re.match(orig).groups()
    repl_ws = ws_re.match(repl).groups()
    # print orig_ws, "->", repl_ws
    if (orig_ws != repl_ws and \
        not (orig_ws == ("", " ") and repl_ws == ("", "") and \
             ((orig.endswith(": ") and repl.endswith(u"：")) or (orig.endswith(", ") and repl.endswith(u"，"))))):
        print (u"%s:%d: error: whitespace mismatch %s -> %s" % (fil, linenum, orig_u, repl_u)).encode("utf-8")
    orig_cap_m = capital_re.match(orig)
    repl_cap_m = capital_re.match(repl)
    if orig_cap_m and repl_cap_m:
        orig_cap = orig_cap_m.group(1)
        repl_cap = repl_cap_m.group(1)
        orig_is_upper = orig_cap == orig_cap.upper()
        repl_is_upper = repl_cap == repl_cap.upper()
        if orig_is_upper != repl_is_upper:
            print (u"%s:%d: warning: %r capitalization mismatch %s -> %s" % (fil, linenum, orig_u, orig_cap, repl_cap)).encode("utf-8")
    orig_keys = key_re.findall(orig)
    repl_keys = key_re.findall(repl)
    if (orig_keys != repl_keys):
        print (u"%s%d: error: key escape mismatch (%s -> %s) %s -> %s" % (fil, linenum, orig_keys, repl_keys, orig_u, repl_u)).encode("utf-8")
        

        
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
    i = 1
    for en, ln in zip(etext, ltext):
        # print i, en
        i += 1
        check_format_strings(lpath, i, en, ln)

        
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
            check_format_strings(lpath, 0, etext[key], ltext[key])


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
        
def header(luaf, comment=""):
    if comment:
        comment = " (%s)" % comment
    return ("\nReassembly %s localization%s\n" % (os.path.basename(luaf), comment)) + """
Copyright (C) 2015-2019 Arthur Danskin
Content-Type: text/plain; charset=utf-8
"""

def metadata(lang):
    return { "Language": lang,
             "MIME-Version": "1.0",
             "Content-Type": "text/plain; charset=UTF-8",
             "Content-Transfer-Encoding": "8bit", }

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
    pofil.header = header("data/" + name)
    pofil.save(pof)
    print "wrote " + os.path.relpath(pof) + " (%d entries updated)" % len(entries)

def read_lua_strings(luaf):
    with io.open(luaf, encoding="utf-8") as fil:
        luadata = fil.read()
    if "web_header" in luadata:
        luadata = luadata[:luadata.index("web_header")]
    luadata = re.sub("--.*\n", "", luadata)
    strs = []
    for st in re.findall('"[^"]+"', luadata):
        st = st[1:-1].replace("\\\n", "")
        if len(st) > 1:
            strs.append(st)
    return strs
    
def lua2po(luaf, pof):
    newpo = polib.POFile()
    for st in read_lua_strings(luaf):
        newpo.append(polib.POEntry(msgid=st))
    if os.path.exists(pof):
        pofil = open_po(pof)
        pofil.merge(newpo)
    else:
        pofil = newpo
    pofil.header = header("data/" + luaf)
    pofil.save(pof)

    
def lua2po_merge(baselua, translua, pof):
    pofil = polib.POFile()
    for base, trans in zip(read_lua_strings(baselua), read_lua_strings(translua)):
        if "tutorial" in baselua:
            print baselua,  base, trans
        pofil.append(polib.POEntry(msgid=base, msgstr=trans))
    pofil.header = header(translua, "fan")
    pofil.save(pof)

    
def pofil_merge(pofs, output, lang):
    pofil = polib.POFile()
    entries = {}
    for pof in pofs:
        for pe in open_po(pof):
            mid = pe.msgid
            if pe.msgid not in entries or not entries[mid].msgstr:
                # pe.obsolete = False
                entries[mid] = pe
    for en in sorted(entries.values()):
        pofil.append(en)
    pofil.header = header(output)
    pofil.metadata = metadata(lang)
    pofil.save(output)


def count_pos(lang, pos):
    total = [0, 0, 0]
    untrans = [0, 0, 0]
    pos = glob.glob(os.path.relpath(os.path.join(ROOT, "lang", lang, "*.po")))
    for po in pos:
        for pe in open_po(po):
            words = len(pe.msgid.split())
            chars = len(pe.msgid)
            total[0] += 1
            total[1] += words
            total[2] += chars
            if not pe.msgstr:
                untrans[0] += 1
                untrans[1] += words
                untrans[2] += chars
    print "%4s%6d %5d/%5d %5d/%5d %6d/%6d (%.1f%%)" % (lang, len(pos), untrans[0], total[0],
                                                      untrans[1], total[1],
                                                      untrans[2], total[2], 100.0 * untrans[2] / total[2])
    
    
def lua2po_simple(luaf, pof):
    dat = read_lua(luaf)
    pofil = polib.POFile()
    for key in sorted(dat.keys()):
        pofil.append(polib.POEntry(msgid=unicode(key), msgstr=unicode(dat[key])))
    pofil.header = header(luaf, "fan")
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


HTML_TEMPL = """
<html>
<head> <meta charset="UTF-8"/> </head>
<body>
%s
</body>
</html>
"""

STORE_ENTRIES = ["Reassembly Short Description", "Reassembly Long Description",
                 "Fields Expansion DLC Short Description", "Fields Expansion DLC Long Description"]

def bbcode2html(s):
    s = s.replace("[h2]", "<h2>")
    s = s.replace("[/h2]", "</h2>")
    s = s.replace("[b]", "<b>")
    s = s.replace("[/b]", "</b>")

    s = s.replace("[list]", "<ul>")
    s = s.replace("[*]", "<li>")
    s = s.replace("[/list]", "</ul>")

    s = re.sub("\\[img\\][^[]*\\[/img\\]", "", s)
    
    return s

def store_html(langs):
    html = ""
    txt = ""
    first = True
    for lang in langs:
        fname = os.path.relpath(os.path.join(ROOT, "lang", lang, "store.po"))
        try:
            pof = open_po(fname)
        except:
            continue
        if first:
            first = False
            i = 0
            for pe in pof:
                txt += "\n\n## " + STORE_ENTRIES[i] + "\n"
                txt += pe.msgid
                html += "<h1> English " + STORE_ENTRIES[i] + "</h1>\n"
                html += bbcode2html(pe.msgid)
                i += 1
        i = 0
        for pe in pof:
            if pe.msgstr == pe.msgid:
                continue
            txt += "\n\n## " + steam_languages[lang].capitalize() + " " + STORE_ENTRIES[i] + "\n"
            txt += pe.msgstr
            html += "<h1>" + steam_languages[lang].capitalize() + " " + STORE_ENTRIES[i] + "</h1>\n"
            html += bbcode2html(pe.msgstr)
            i += 1
    html = HTML_TEMPL % html
    with io.open(os.path.expanduser("~/Downloads/reassembly_store.html"), "w", encoding="utf-8") as f:
        f.write(html)
    with io.open(os.path.expanduser("~/Downloads/reassembly_store.txt"), "w", encoding="utf-8") as f:
        f.write(txt)
            
            
def po2lua_replace(outlua, baselua, pof):
    with io.open(baselua, encoding="utf-8") as fil:
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
    fil.write("{\n" + ",\n".join('"%s" = "%s"' % (escape(k), escape(v)) for k, v in sorted(dct.items()) if k != v) + "\n}")
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


def import_po2lua(lang):
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

def import_onelang(lang):
    pos = glob.glob(os.path.relpath(os.path.join(ROOT, "lang", lang, "*.po")))

    outp = "~/Downloads/reassembly_%s.po" % lang
    entries = {}
    try:
        f = open_po(os.path.expanduser(outp))
    except:
        return
    for pe in f:
        if pe.msgstr:
            entries[pe.msgid] = pe.msgstr
    print "%s: %d entries" % (outp, len(entries))
    for pof in pos:
        pofil = open_po(pof)
        changed = 0
        for pe in pofil:
            if not pe.msgstr and pe.msgid in entries:
                pe.msgstr = entries[pe.msgid]
                changed += 1
            elif pe.msgstr and pe.msgid in entries and entries[pe.msgid] != pe.msgstr:
                print ("%s: %s -> %s" % (pe.msgid, pe.msgstr, entries[pe.msgid])).encode("utf-8")
                pe.msgstr = entries[pe.msgid]
                changed += 1
        if changed:
            print "%s: %d entries changed" % (pof, changed)
            pofil.save(pof)

            
def printhelp():
    print """%s [-ielhn] <language0> [language1...]
-i : import .po files to game format
-e : export .po files from game source
-n : don't modify anything (combine with -t)
-z : create zip file
-o : create one po file per language
-t : validate
-c : count words
-p <DIR> : import .po files from dir 
-h : print this message""" % sys.argv[0]
    exit(0)

opts, cargs = getopt.getopt(sys.argv[1:], "iehzntp:co")
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
    elif opt == "-p":
        lua2po_dir = val
    elif opt == "-c":
        docount = True
    elif opt == "-o":
        doone = True

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
    # if not set_languages:
        # languages = [x for x in os.listdir(os.path.join(ROOT, "lang/")) if len(x) == 2]
    print "importing languages: %s" % ", ".join(languages)
    for lang in languages:
        if doone:
            import_onelang(lang)
        import_po2lua(lang)
    achievements_po2vdf(os.path.expanduser("~/Downloads/achievements.vdf"), languages)
    store_html(languages)
    print "import complete"
elif validate:
    for lang in languages:
        check_dict("text.lua", lang)
        check_values("messages.lua", lang)
        check_list("tips.lua", lang)
elif not dozip and not lua2po_dir and not docount and not doone:
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
    
if doone and not imprt:
    for lang in languages:
        pos = glob.glob(os.path.relpath(os.path.join(ROOT, "lang", lang, "*.po")))
        # pos.append(os.path.join(ROOT, "lang", "store.po"))
        outp = "~/Downloads/reassembly_%s.po" % lang
        pofil_merge(pos, os.path.expanduser(outp), lang)
        print "wrote %d files to %s" % (len(pos), outp)
    for lang in languages:
        outp = "~/Downloads/reassembly_%s.po" % lang
        count_pos(lang, outp)

if lua2po_dir:
    print "importing lua files from %s" % lua2po_dir
    based  = os.path.join(ROOT, "data")
    if lua2po_dir.endswith(".lua"):
        lang = os.path.basename(os.path.dirname(lua2po_dir))
        fils = [ lua2po_dir ]
    else:
        lang = os.path.basename(lua2po_dir)
        fils = glob.glob(os.path.join(lua2po_dir, "*.lua"))
    outd = os.path.join(ROOT, "lang", lang)
    mkdir_p(outd)
    for fil in fils:
        pof = os.path.join(outd, os.path.basename(fil)).replace(".lua", ".po")
        baselua = os.path.join(based, os.path.basename(fil))
        if "text.lua" in fil:
            lua2po_simple(fil, pof)
        else:
            lua2po_merge(baselua, fil, pof)
        print "writing %s -> %s" % (os.path.relpath(fil), os.path.relpath(pof))
    print "translated %d lua files from %s" % (len(fils), lua2po_dir)

if docount:
    pos = []
    print "lang  files   strings    words       chars"
    for lang in languages:
        pos = glob.glob(os.path.relpath(os.path.join(ROOT, "lang", lang, "*.po")))
        count_pos(lang, pos)
        # pos.extend(glob.glob(os.path.relpath(os.path.join(ROOT, "lang", lang, "*.po"))))
    # allpo = "all.po"
    # print "concatenating %d files > %s..." % (len(pos), allpo)
    # with open(allpo, "w") as f:
    #     subprocess.call(["cat"] + pos, stdout=f)
    # print "counting..."
    # subprocess.call(["pocount", allpo])
    # os.remove(allpo)
