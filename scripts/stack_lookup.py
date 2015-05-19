#!/usr/bin/python

import sys, re, os
import gzip
import glob
import math
import datetime
from copy import copy
import bisect
import cProfile
import getopt
import subprocess

UNDNAME = "C:/Program Files (x86)/Microsoft Visual Studio 12.0/VC/bin/undname.exe"
DIA2DUMP = "C:/Users/Arthur/Documents/DIA2Dump/Release/Dia2Dump.exe"
if sys.platform.startswith("darwin"):
    PDB_SYMBOLS = ["/Volumes/BOOTCAMP/symbols",
                   "/Volumes/Users/Arthur/AppData/Local/Temp/SymbolCache"]
    OUTLAWS_WIN32 = ["win32",
                     "/Volumes/BOOTCAMP/Users/Arthur/Documents/outlaws/win32",
                     "/Volumes/Users/Arthur/Documents/outlaws/win32"]
else:
    PDB_SYMBOLS = ["C:/symbols",
                   "//THOR/Users/Arthur/AppData/Local/Temp/SymbolCache"]
    OUTLAWS_WIN32 = ["win32", "//THOR/Users/Arthur/Documents/outlaws/win32"]
    
REL_SYM_PATH = {"ReassemblyRelease":["steam", "release"],
                "ReassemblyBuilder":["builder"],
                "ReassemblyRelease64":["release"],
                "ReassemblySteam32":["steam"]}
BOOTCAMP = "/Volumes/BOOTCAMP"
BOOTCAMP_OUTLAWS = os.path.join(BOOTCAMP, "Users/Arthur/Documents/outlaws/")
READELF = "readelf -lsW -wL %s | c++filt"
TRIAGE_IGNORE_TRACE = set(["posix_signal_handler(int, siginfo_t*, void*)", # clang
                           "posix_signal_handler(int, __siginfo*, void*)", # gcc
                           "_sigtramp", "_init", "_L_unlock_13", "0x0",
                           "posix_print_stacktrace", "print_backtrace()", "OL_OnTerminate",
                           "void terminate()", "_Call_func"
                       ])

ROOT = "./"

TRIAGE_STACK_SIZE = 3
MAX_STACK_DUMP = 30

class map_entry:
    def __init__(self, sym, base):
        self.sym = sym
        self.base = base
        self.line = -1
        self.file = None
                

class module_entry:
    pass

def demangle_sym(sym):
    name = sym
    # print name
    if sym.startswith("?") or sym.startswith("_"):
        if os.path.exists(UNDNAME):
            txt = subprocess.check_output([UNDNAME, sym])
            m = re.search('is :- "([^"]*)"', txt)
            if m:
                name = m.group(1)
        
        if name.startswith("?") or name.startswith("_"):
            # ?insertPoint@?$spacial_hash@UPort@@@@QAEXU?$tvec2@M$0A@@detail@glm@@ABUPort@@@Z
            m = re.match("\?([a-zA-Z0-9_]+)@\?\$([a-zA-Z0-9_]+)@U([a-zA-Z0-9_]+)@", name)
            if m:
                return "%s<%s>::%s" % (m.group(2), m.group(3), m.group(1))
            m = re.match("\?([a-zA-Z0-9_]+)@([a-zA-Z0-9_]+)@@", name)
            if m:
                return "%s::%s" % (m.group(2), m.group(1))
            m = re.match("\?([a-zA-Z0-9_]+)@@", name)
            if m:
                return m.group(1)
            m = re.match("\?\?\$([a-zA-Z_0-9]+)@.*@([a-zA-Z_0-9]+)@std@@", name)
            if m:
                return "std::%s::%s" % (m.group(2), m.group(1))
            m = re.match("\?_Go@\?\$_LaunchPad", name)
            if m:
                return "LaunchPad"
            m = re.match("_([a-zA-Z0-9_]+)@", name)
            if m:
                return m.group(1)
    # dia2dump pdb lines data symbols sometimes have the symbol, then again in parens for some reason
    m = re.match(r"([a-zA-Z0-9_:]+)\((.*\1.*)\)$", name)
    if m:
        name = m.group(2)
    name = re.sub("(__(cdecl|thiscall)|(public|private|protected):|struct |class ) *", "", name)
    name = name.replace("glm::detail::tvec2<float,0>", "float2")
    name = name.replace("glm::detail::tvec2<float, (glm::precision)0>", "float2")
    name = name.replace("std::basic_string<char,std::char_traits<char>,std::allocator<char> >", "string")
    name = name.replace("std::basic_string<char, std::char_traits<char>, std::allocator<char> >", "string")
    name = name.replace("unsigned int", "uint")
    name = name.replace("(void)", "()")
    return name


def safe_listdir(dirn):
    try:
        return os.listdir(dirn)
    except:
        return []

    
def abbreviate_fname(fname):
    # return fname
    return fname.replace(os.path.expanduser("~"), "~").replace(
        "/cygdrive/c/Users/Arthur", "~").replace("~/Documents/outlaws", "~/outlaws")
    

def shorten_fname(fname):
    if not fname:
        return fname
    fname = fname.replace("\\", "/").replace(".obj", ".cpp")
    fname = os.path.normpath(fname)
    idx = fname.find("outlaws/")
    if idx > 0:
        fname = os.path.join(ROOT, fname[idx+len("outlaws/"):])
    elif len(fname) > 1 and fname[1] == ':':
        return fname
    else:
        # base = "linux" if sys.platform.startswith("linux") else ""
        fname = os.path.normpath(os.path.join(ROOT, fname))
    if os.path.exists(fname):
        return abbreviate_fname(fname)
    # fix case insensitiveity
    dirn = os.path.dirname(fname)
    base = os.path.basename(fname).lower()
    for fil in safe_listdir(dirn):
        if fil.lower() == base:
            return abbreviate_fname(os.path.join(os.path.dirname(fname), fil))
    # oh well
    return fname


def glob_files(patterns):
    paths = []
    for pat in patterns:
        paths.extend(glob.glob(pat))
    return paths
    

def get_dll_symbols(modname):
    pdb = modname.replace(".dll", ".pdb")
    lpdb = pdb.lower()
    if lpdb in ("msvcp120.pdb", "msvcr120.pdb"):
        lpdb = lpdb.replace(".pdb", ".i386.pdb")

    pdb_path = [os.path.join(ROOT, "win32", pdb)]
    if sys.platform.startswith("darwin"):
        pdb_path.extend(os.path.join(w32, pdb) for w32 in OUTLAWS_WIN32)
    pdb_path.extend(os.path.join(sp, lpdb, "*", lpdb) for sp in PDB_SYMBOLS)
    paths = glob_files(pdb_path)
    if len(paths) == 0:
        return None, []
    pdb_path = paths[0]

    ext, flags = ("line", ("-l",)) if "win32" in pdb_path else ("globals", ("-g", "-p"))

    gpath = "%s.%s.gz" % (pdb_path, ext)
    if not os.path.exists(gpath) and os.path.exists(DIA2DUMP):
        fil = gzip.open(gpath, "w")
        if not fil:
            print "can't open", gpath
            exit(1)
        for flag in flags:
            dat = subprocess.check_output([DIA2DUMP, flag, pdb_path])
            if len(dat) > 100:
                break;
        fil.write(dat)
        fil.close()
    if os.path.exists(gpath):
        return ext, gzip.open(gpath)

    symbol_path = [os.path.join(ROOT, w32, "%s.line.gz" % pdb) for w32 in OUTLAWS_WIN32]
    symbol_path.extend(os.path.join(ROOT, w32, "symbols", "%s.globals.gz" % lpdb) for w32 in OUTLAWS_WIN32)
    
    paths = glob_files(symbol_path)
    for pat in paths:
        ext = "line" if pat.endswith(".line.gz") else "globals"
        return ext, gzip.open(pat)
    return None, []    


def get_so_symbols(modname, version=""):
    gzpath = os.path.join(ROOT, "linux", "symbols", os.path.basename(modname) + version + ".elf.gz")
    if not os.path.exists(gzpath):
        if os.path.exists(modname):
            fil = gzip.open(gzpath, "w")
            fil.write(subprocess.check_output(READELF % modname, shell=True))
            fil.close()
        else:
            return None, []
    return "elf", gzip.open(gzpath)
    

def get_symbol_type_handle(modname, version):
    
    if modname.endswith(".dll"):
        return get_dll_symbols(modname)
    elif ".so." in modname:
        return get_so_symbols(modname)
    elif version:
        basename = os.path.splitext(modname)[0]
        platform = "win32" if modname.endswith(".exe") else "linux"
        dirns = REL_SYM_PATH.get(basename, "")
        for dirn in dirns:
            for typ in ("line", "elf"):
                symbol_path = OUTLAWS_WIN32 if platform == "win32" else [platform]
                # print symbol_path
                paths = glob_files(os.path.join(ROOT, p, dirn, version, "%s.%s.gz" % (basename, typ)) for p in symbol_path)
                if len(paths):
                    # print "using", paths[0]
                    return typ, gzip.open(paths[0])
        if sys.platform.startswith("linux"):
            return get_so_symbols(os.path.join(ROOT, "linux", modname), version)
    return None, []

parsed = {}
def parse_symbol_map(modname, version):

    key = (modname, version)
    if key in parsed:
        return parsed[key]

    entries = parsed.setdefault(key, [])
    typ, handle = get_symbol_type_handle(modname, version)

    if typ == "line":
        sym_re = re.compile("[*][*] (.*)")
        line_re = re.compile("line ([0-9]+) at \[([0-9A-F]+)\]\[([0-9A-F]+):([0-9A-F]+)\], len = 0x[A-F0-9]+(\t(.+) \(MD5)?")
        cursym = None
        curfil = None
        for line in handle:
            line = line.strip()
            m = sym_re.match(line)
            if m:
                cursym = m.group(1)
                continue

            m = line_re.match(line)
            if m:
                lnum, rva, seg, offset, _, fil = m.groups()
                if fil:
                    curfil = shorten_fname(fil)
                entry = map_entry(cursym, int(rva, 16))
                entry.line, entry.file = (int(lnum), curfil)
                entries.append(entry)
    elif typ == "globals":
        func_re = re.compile("(Function|PublicSymbol): \[([0-9A-F]+)\]\[([0-9A-F]+):([0-9A-F]+)\] (.*)")
        for line in handle:
            m = func_re.match(line)
            if m:
                entries.append(map_entry(m.group(5).strip(), int(m.group(2), 16)))
    elif typ == "map":
        load_addr = 0
        load_re = re.compile("Preferred load address is ([a-fA-F0-9]+)")
        for line in handle:
            line = line.strip()
            m = load_re.match(line)
            if m:
                load_addr = int(m.group(1), 16)
                continue
            try:
                fields = line.split()
                entry = map_entry(fields[1], int(fields[2], 16) - load_addr)
                entry.file = shorten_fname(fields[-1])
                if (entry.base >= 0 and (entry.sym.startswith("?") or entry.sym.startswith("_"))):
                    entries.append(entry)
            except Exception, e:
                pass
    elif typ == "elf":
        # Type           Offset   VirtAddr           PhysAddr           FileSiz  MemSiz   Flg Align
        load_re = re.compile(" *LOAD +0x[0-9a-f]+ (0x[0-9a-f]+)")
        #    Num:    Value          Size Type    Bind   Vis      Ndx Name
        sym_re = re.compile(" *[0-9]+: ([0-9a-f]+) +[0-9]+ FUNC *[A-Z]+ *[A-Z]+ *[A-Z0-9]+ +(.*)")
        fline_re = re.compile("([.A-Za-z0-9_/]+):$")
        line_re = re.compile("([^ ]+) +([0-9]+) +0x([0-9a-f]+)")

        baseaddr = 0
        entrypoint = None
        for line in handle:
            m = load_re.match(line)
            if m:
                baseaddr = int(m.group(1), 16)
                break

        funcs = []        
        for line in handle:
            m = sym_re.match(line)
            if m:
                addr = int(m.group(1), 16) - baseaddr
                if addr < 0:
                    continue
                entry = map_entry(m.group(2), addr)
                funcs.append(entry)
            elif line.startswith("File name"):
                break
            
        funcs.sort(key=lambda x: x.base)
        func_keys = [f.base for f in funcs]

        curfile = None

        for line in handle:
            if len(line) < 4:
                continue
            m = line_re.match(line)
            if m:
                addr = int(m.group(3), 16) - baseaddr
                if addr < 0:
                    continue
                func = funcs[bisect.bisect_left(func_keys, addr)-1]
                #func = lookup_entry(func_keys, funcs, addr)
                entry = map_entry(func.sym, addr)
                entry.line = int(m.group(2))
                entry.file = curfile
                entries.append(entry)

                if func.line == -1:
                    func.line = entry.line
                    func.file = curfile
                else:
                    func.line = min(func.line, entry.line)
            else:
                m = fline_re.match(line)
                if m:
                    curfile = os.path.normpath(os.path.join("linux", m.group(1)))
        entries.extend(funcs)
    else:
        print "WARNING: No symbols found for %s@%s" % (modname, version)
        return entries

    assert len(entries) > 0, "%s, %s, %s" % (typ, handle, entries)
    entries.sort(key=lambda x: x.base)
    return entries


def lookup_entry(keys, symbols, raddr):
    idx = max(0, bisect.bisect_left(keys, raddr) - 1)
    assert 0 <= idx < len(symbols), (idx, len(symbols))
    return symbols[idx]


# function, line, file
def lookup_address(mmap, addr):
    module = None
    for entr in mmap:
        if entr.base <= addr < entr.base + entr.size:
            module = entr
            break
        # elif addr >= entr.base:
            # print hex(addr), entr.name, hex(entr.base), hex(entr.base + entr.size)
    if (module is None):
        return (None, -1, None)

    raddr = addr - module.base

    if module.symbols is None:
        module.symbols = parse_symbol_map(module.name, module.version)
        module.keysyms = [f.base for f in module.symbols]

    if len(module.symbols) == 0:
        return (module.name, -1, None)

    entry = lookup_entry(module.keysyms, module.symbols, raddr)
    if entry:
        dsym = demangle_sym(entry.sym)
        if entry.line == -1:
            return (dsym, -1, os.path.basename(module.name))
        else:
            return (dsym, entry.line, shorten_fname(entry.file))
    return (module.name, -1, "<unknown_function>")


def strtime(seconds):
    if seconds < 60:
        return "%3.f sec" % seconds
    if seconds < 60 * 60:
        return "%3.1f min" % (seconds / 60.0)
    return "%3.1f hrs" % (seconds / (60.0 * 60))

def sdict(dic, lmba=lambda x: x[1]):
    return sorted(list(dic.items()), key=lmba, reverse=True)

def dict_incr(mp, val, amt=1):
    mp[val] = mp.get(val, 0) + amt

class Variance:

    def __init__(self):
        self.n = 0
        self.mean = 0
        self.M2 = 0
        self.minv = 99999999999

    def onData(self, x):
        self.n = self.n + 1
        delta = x - self.mean
        self.mean = self.mean + delta/self.n
        self.M2 = self.M2 + delta*(x - self.mean)
        self.minv = min(self.minv, x)

    def stddev(self):
        return 0.0 if self.n < 2 else math.sqrt(self.M2 / (self.n - 1))

class TimestampHandler:

    timestampre = re.compile("\[TIMESTAMP\] ([0-9.]+) (GameState)?([A-Z][a-zA-Z0-9_]+) * ([0-9]+) fps ([0-9]+)/([0-9]+) ups")

    def __init__(self):
        self.count = 0
        self.states = dict()
        self.fps = Variance()
        self.ups = Variance()
        self.totaltime = 0

    def onLine(self, line):
        m = self.timestampre.match(line)
        if not m:
            return line
        time, _, state, fps, ups, upstotal = m.groups()
        time = float(time)
        fps = float(fps)
        ups = float(ups)
        upstotal = float(upstotal)

        dict_incr(self.states, state)
        if self.count > 3:
            self.fps.onData(fps)
            self.ups.onData(ups)
        self.count += 1
        self.totaltime = time
        return line

    def onFinish(self):
        if self.count == 0:
            return
        print "============== Time stamp Info ==============="
        step = self.totaltime / self.count
        histogram = list(self.states.items())
        histogram.sort(key=lambda x: x[1], reverse=True)
        for x in histogram:
            print "%20s: %s (%2.f%%)" % (x[0], strtime(x[1] * step), 100.0 * x[1] / self.count)
        print "avg/stddev/min fps: %.f, %.1f, %.f" % (self.fps.mean, self.fps.stddev(), self.fps.minv)
        print "avg/stddev/min ups: %.f, %.1f, %.f" % (self.ups.mean, self.ups.stddev(), self.ups.minv)
        print "total time: %s" % (strtime(self.totaltime))


class AssertionHandler:

    assert_re = re.compile("([^:]+)(:.* )ASSERT.*?([(]([0-9]+) times[)])?$")

    def __init__(self):
        self.assertions = dict()
        self.names = dict()
        self.total = 0

    def onLine(self, line):
        m = self.assert_re.match(line)
        if not m:
            return line
        # print line, m.groups()
        key = shorten_fname(m.group(1)) + m.group(2)
        if m.group(3):
            asrt = m.group(0)[m.end(2):m.start(3)]
        else:
            asrt = m.group(0)[m.end(2):]
        asrt = asrt.strip()
        times = int(m.group(4)) if m.group(4) else 1
        dict_incr(self.assertions, key, times)
        self.names[key] = asrt
        self.total += times
        return asrt

    def onFinish(self):
        if not len(self.assertions):
            return
        print "============== ASSERTIONS ==============="
        for x in sdict(self.assertions):
            line = x[0] + self.names[x[0]]
            if x[1] > 1:
                print "%s (%d times, %.1f%%)" % (line, x[1], 100.0 * x[1] / self.total)
            else:
                print "%s" % line


class NotificationHandler:

    notif_re = re.compile("\[NOTIFICATION\] Notification {type=([a-zA-Z_]+)[, ]*(.*)$")
    value_re = re.compile("value=([-.0-9]+)")
    name_re = re.compile("\"(.*) was destroyed")
    sname_re = re.compile("\[STEAM\] User '([^']*)'")
    killed_re = re.compile("block=<([^>:]+).*<([^>:]+)")
    progress_re = re.compile("\[SPAWN\] On Progress: ([a-zA-Z0-9_]*)")

    def __init__(self):
        self.notifs = dict()
        self.killed = dict()
        self.name = ""
        self.sname = ""
        self.progress = []

    def onLine(self, line):
        r = self.progress_re.match(line)
        if r:
            self.progress.append(r.group(1))
        m = self.notif_re.match(line)
        if not m:
            return line
        typ, vals = m.groups()
        n = self.value_re.match(vals)
        val = 0
        if n:
            val = float(n.group(1))
        if val < 0:
            typ = typ.replace("GAIN", "LOST")
            val = -val
        if typ == "PLAYER_KILLED":
            q = self.killed_re.match(vals)
            if q:
                bl = q.group(2)
                dict_incr(self.killed, bl)
                
        orig = self.notifs.get(typ, (0, 0.0))
        self.notifs[typ] = (orig[0] + 1, orig[1] + val)

        if len(self.name) == 0:
            p = self.name_re.search(vals)
            if p:
                self.name = p.group(1)
        if len(self.sname) == 0:
            p = self.sname_re.search(vals)
            if p:
                self.sname = p.group(1)
        return line
    
    def onFinish(self):
        print "============== NOTIFICATIONS ==============="
        if len(self.name):
            print "Name: " + self.name
        y = sdict(self.notifs, lambda x: x[1][0])
        for x in y:
            print "%4d x %20s" % (x[1][0], x[0]),
            if (x[1][1] != 0):
                print ": %.f" % x[1][1]
            else:
                print ""
        if len(self.progress):
            print "============== PROGRESS ==============="
            print ", ".join(self.progress)
        print "============== KILLED ==============="
        z = sdict(self.killed)
        z0 = z[:len(z)/2]
        z1 = z[len(z)/2:]
        for cl in zip(z0, z1):
            for l in cl:
                print "%3d x %-30s" % (l[1], l[0]),
            print ""

def format_address(addr, func, lino, fil):
    if func:
        if lino > 0:
            fmt = "%s at %s:%d" % (func, fil, lino)
        elif fil:
            fmt = "%s in %s" % (func, fil)
        else:
            fmt = func
    else:
        fmt = "<unknown>"
    return hex(addr) + " " + fmt


class extract_opts:
    def __init__(self):
        self.printall = False
        self.versions = []

            
def extract_callstack(logf, opts, triage=None, handlers=None):

    module_map = []
    version = ""
    platform = ""
    stacktrace = []
    trace_has_game = False
    lastfew = []
    exception_lines = []

    version_re = re.compile("Build Version: ([a-zA-Z]+).*(Release|Debug|Develop|Builder|Steam)(32|64) ([^,]*),")
    basere = re.compile("'([^']+)' base address is 0x([a-fA-F0-9]+), size is 0x([a-fA-F0-9]+)")
    addrre = re.compile("[cC]alled from 0x([a-fA-F0-9]+)")
    mac_stack_re = re.compile("0x[a-fA-F0-9]+ (.+) [+] [0-9]+ [(]([^)]+)[)]")
    hexre = re.compile("0x([a-fA-F0-9]{6,})")

    if handlers is None:
        if triage is None:
            handlers = [TimestampHandler(), NotificationHandler(), AssertionHandler()]
        else:
            handlers = []

    if logf == "-":
        data = sys.stdin
    elif logf.endswith(".gz"):
        data = gzip.open(logf)
    else:
        data = open(logf)
    symbols = None

    # header, version
    for line in data:
        m = version_re.match(line)
        if m:
            platform = m.group(1)
            build = m.group(2) + m.group(3)
            date = datetime.datetime.strptime(m.group(4), "%b %d %Y")
            version = datetime.date.strftime(date, "%Y_%m_%d")
            # print "version is", version
            if (opts.versions and version not in opts.versions):
                return
            if triage is None:
                print line,
                print data.next(), # platform info
            break

    # analytic log handlers
    for line in data:
        line = line.strip()
        for h in handlers:
            line = h.onLine(line)
        if opts.printall:
            print line
        elif triage is None:
            lastfew.append(line)
            while (len(lastfew) > 5):
                lastfew.pop(0)
        if ("Unhandled Top Level Exception" in line or \
            "Caught SIG" in line or \
            "Dumping stack for thread" in line or\
            "Terminate Handler" in line):
            break

    if triage is None:
        for h in handlers:
            h.onFinish()

    for ln in lastfew:
        print ln

    # crash dump
    stacklines = 0
    skipped = 0
    for line in data:
        line = line.replace("\r", "")
        m = basere.search(line)
        if m:
            mod = module_entry()
            mod.name = m.group(1)
            mod.base = int(m.group(2), 16)
            mod.size = int(m.group(3), 16)
            mod.version = version if (mod.name.endswith(".exe") or "Reassembly" in mod.name) else None
            mod.symbols = None # load lazily
            module_map.append(mod)
            continue
        elif triage is None and module_map:
            for ln in exception_lines:
                func, lino, fil = lookup_address(module_map, ln[1])
                if func:
                    fmt = format_address(addr, func, lino, fil)
                    print ln[0] + fmt + ln[2],
            exception_lines = []

        if "Dumping stack" in line:
            if skipped and triage is None:
                print "...skipped %d stack frames" % skipped
            stacklines = 0
            skipped = 0
            if stacktrace:
                break
            line = "\n" + line

        m = addrre.search(line)
        if m:
            stacklines += 1
            addr = int(m.group(1), 16)
            n = mac_stack_re.search(line)
            if n:
                func, module = n.groups()
                has_game = module == "Reassembly"
                line = line.replace("[POSIX] ", "").replace("called from", "from")
            else:
                func, lino, fil = lookup_address(module_map, addr)
                has_game = lino > 0
                fmt = format_address(addr, func, lino, fil)
                line = line[:m.start()] + fmt + line[m.end():]
                line = line.replace("[win32] ", "").replace("called from", "from")

            if (triage != None and \
                func and func not in TRIAGE_IGNORE_TRACE and
                (len(stacktrace) == 0 or stacktrace[-1] != func)):
                if has_game:
                    trace_has_game = True
                if func.endswith (".DLL"):
                    func = func.lower()
                if "(" in func:
                    func = func[:func.index("(")]
                stacktrace.append(func)
                if len(stacktrace) > TRIAGE_STACK_SIZE and trace_has_game:
                    break
        elif triage is None:
            m = hexre.search(line)
            if m:
                addr = int(m.group(1), 16)
                if module_map:
                    func, lino, fil = lookup_address(module_map, addr)
                    fmt = format_address(addr, func, lino, fil)
                    line = line[:m.start()] + fmt + line[m.end():]
                else:
                    exception_lines.append((line[:m.start()], addr, line[m.end():]))

        if stacklines > MAX_STACK_DUMP:
            skipped += 1
        elif triage is None:
            print line,
            
    if skipped and triage is None:
        print "...skipped %d stack frames" % skipped

    if triage != None:
        ver = version or "<unknown>"
        key = "<no stack>"
        if stacktrace:
            key = " <- ".join(stacktrace)
            if len(key) > 100:
                key = key.replace(" <- ", "\n   <- ")
        triage.setdefault(ver, dict()).setdefault(key, []).append(logf)


if __name__ == '__main__':

    ROOT = os.path.abspath(os.path.join(os.path.dirname(sys.argv[0]), ".."))
    ROOT = ROOT.replace("/cygdrive/c/", "c:/")

    ops = extract_opts()
    
    opts, args = getopt.getopt(sys.argv[1:], "av:")
    for (opt, val) in opts:
        if opt == "-a":
            ops.printall = True
        if opt == "-v":
            ops.versions.append(val)

    
    print "-*- mode: compilation -*-"
    print "ROOT=%s" % ROOT
    if len(args) == 0:
        logs = glob.glob(os.path.join(os.path.expanduser("~/Downloads"), "Reassembly_*.txt"))
        latest = os.path.join(ROOT, "data", "log_latest.txt")
        if os.path.exists(latest):
            logs.append(latest)
        if len(logs):
            logs.sort(key=lambda x: os.path.getmtime(x))
            last = logs[-1]
            print "picking most recently modified log"
            print "selected %s (considered %d)" % (last, len(logs))
            extract_callstack(last, ops)
            exit(0)
        print "usage: %s <log>\n       %s <log1> <logn...>" % (sys.argv[0], sys.argv[0])
    elif len(args) == 1 and "*" not in args[0]:
        # cProfile.run("extract_callstack(args[1], None)", sort="tottime")
        extract_callstack(args[0], ops)
    else:
        files = []
        for fil in args:
            if "*" in fil:
                files.extend(glob.glob(fil))
            else:
                files.append(fil)
        print "processing %d logs" % (len(args))
        triage = {}
        handlers = [AssertionHandler()]
        ops1 = copy(ops)
        ops1.printall = False
        for fil in files:
            fil = os.path.normpath(fil)
            extract_callstack(fil, ops1, triage, handlers)
        tfiles = []
        for version, data in sdict(triage, lambda x: x[0]):
            total = sum(len(x) for x in data.itervalues())
            print "============= %s (%d total) ================" % (version, total)
            for stack, files in sdict(data, lambda x: len(x[1])):
                print "%d(%.1f%%). %s" % (len(files), 100.0 * len(files) / total, stack)
                for fl in sorted(files):
                    print "     ", fl.replace(os.path.expanduser("~"), "~")
                    tfiles.append(fl)
        for h in handlers:
            h.onFinish()
        # exit(0)

        print ""
        print ""
        for fl in tfiles:
            print "#" * (len(fl) + 4)
            print "#", fl
            print "#" * (len(fl) + 4)
            extract_callstack(fl, ops)
            print ""

                
    
    
