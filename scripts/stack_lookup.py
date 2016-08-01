#!/usr/bin/python

# This script parses Reassembly crashlog files and looks up symbols for the stack traces
# Supports windows (visual studio) and linux
# Also supports osx traces but assumes symbols are already looked up

# Can also triage stack traces - generate a histogram of most stacks from thousands of logs

# It also aggregates asserts and makes playtime histograms from timestamp logs
# symbol paths are set in the following variables

# Dia2Dump is a program that converts .pdb files to various text formats
# it comes with Visual Studio as a sample application but needs to be compiled

import sys, re, os
import gzip
import glob
import math
from datetime import datetime, date, timedelta
from copy import copy
import bisect
import cProfile
import getopt
import subprocess
import shutil

UNDNAME = "C:/Program Files (x86)/Microsoft Visual Studio 12.0/VC/bin/undname.exe"
DIA2DUMP = "C:/Users/Arthur/Documents/DIA2Dump/Release/Dia2Dump.exe"
if sys.platform.startswith("darwin"):
    PDB_SYMBOLS = ["/Volumes/BOOTCAMP/symbols",
                   "/Volumes/Users/Arthur/AppData/Local/Temp/SymbolCache"]
    OUTLAWS_WIN32 = ["win32",
                     "/Volumes/BOOTCAMP/Users/Arthur/Documents/outlaws/win32",
                     "/Volumes/Users/Arthur/Documents/outlaws/win32"] # thor/eclipse
    OUTLAWS_LINUX = ["linux", "linux/symbols", "/Volumes/arthur/outlaws/linux"]
else:
    PDB_SYMBOLS = ["C:/symbols",
                   "//THOR/Users/Arthur/AppData/Local/Temp/SymbolCache"]
    OUTLAWS_WIN32 = ["win32", "//THOR/Users/Arthur/Documents/outlaws/win32"]
    OUTLAWS_LINUX = ["linux", "linux/symbols", "//AUXILIA/arthur/Documents/outlaws/linux"]

REL_SYM_PATH = {"ReassemblyRelease":["steam", "release"],
                "ReassemblyBuilder":["builder"],
                "ReassemblyRelease64":["release"],
                "ReassemblySteam32":["steam"]}
READELF = "readelf -lsW -wL %s | c++filt"

# ignore these symbols when triaging stack traces
TRIAGE_IGNORE_TRACE = set(["posix_signal_handler",
                           "_sigtramp", "_init", "_L_unlock_13", "0x0",
                           "posix_print_stacktrace", "print_backtrace", "OL_OnTerminate",
                           "void terminate", "_Call_func",

                          "_callthreadstartex", "_threadstartex",
                           "kernel32.dll", "ntdll.dll",

                           "__vsnprintf_l", "__vsnprintf",

                           # around _free
                           "LdrGetProcedureAddressForCaller",
                           "_WER_HEAP_MAIN_HEADER * __ptr64 WerpGetHeapHandle",
                           
                           # always together with mtx_do_lock or sleep (msvc120)
                          "__Mtx_lock",
                           "bool Concurrency::critical_section::_Acquire_lock",
                           "void Concurrency::critical_section::lock",
                           "void Concurrency::details::LockQueueNode::UpdateQueuePosition",
                           "void Concurrency::details::_Timer::_Start",
                           "void Concurrency::details::ReferenceLoadLibrary",
                           "_TP_TIMER * Concurrency::details::RegisterAsyncTimerAndLoadLibrary",
                           "virtual void Concurrency::details::ExternalContextBase::Block",
                           "RtlAcquireSRWLockExclusive",
                           "RtlInsertElementGenericTableFullAvl",
                           "LdrLogNewDataDllLoad",
                           "EtwpCreateFile", # windows 7
                           "BaseCheckVDMp$fin$0" # windows 7
                           "LdrResGetRCConfig",

                           # linux around myTerminateHandler
                           "std::locale::locale@@GLIBCXX_3.4",
                           "std::moneypunct<>::moneypunct@@GLIBCXX_3.4",
                           "posix_spawnattr_setschedparam@@GLIBC_2.2.5",

                           # around error handling in msvc140
                           "??_C@_0CA@IFNNBHIE@FwGetRpcCallersProcessImageName?$AA@",
                           "___scrt_fastfail",
                           #"void Concurrency::details::_ReportUnobservedException",
                           "__Mtx_clear_owner",
                           "AslpFileQueryExportName$filt$0",
                           "RtlpHpLfhOwnerMoveSubsegment",
                           "int Concurrency::details::_Schedule_chore",
                           "BasepCreateTokenFromLowboxToken",
                           "long WerpAddGatherToPEB",
                           "LdrpReportError",
                           "RtlpReAllocateHeap",
                           "RtlpHpLargeAlloc",
                           "EtwpWriteToPrivateBuffers",

                           # noise at bottom of stack
                           # "CompatCacheLookupExe",
                           "RtlGuardCheckImageBase",
                           "LdrpResGetMappingSize",
                           "RtlCompressBufferXpressHuffStandard",
                           "EtwpLogger",
                           "std::_LaunchPad<>::_Go",
                           "RtlpGetStackTraceAddressEx",
                           "_callthreadstartex",
                           "_threadstartex",
                           "WinMain",
                           "vDbgPrintExWithPrefixInternal",
                           "RtlIpv6AddressToStringA",
                           "A_SHAUpdate",
                           "CompatCacheLookupExe",
])

paren_re = re.compile("[(][^()]*[)] *(const)?")
angle_re = re.compile("[<][^<>]+[>]")
bracket_re = re.compile("[[][^][]+[]]")
def remove_parens1(func, regex, repl):
    count = 1
    while count > 0:
        func, count = regex.subn(repl, func)
    return func

def remove_parens(func):
    func = remove_parens1(func, paren_re, "")
    func = remove_parens1(func, angle_re, "{}").replace("{}", "<>")
    func = bracket_re.sub("", func)
    return func.strip()
        

def ignore_func(func):
    return func and (remove_parens(func) in TRIAGE_IGNORE_TRACE)

ROOT = "./"

TRIAGE_STACK_SIZE = 4
MAX_STACK_DUMP = 30

def log(*x):
    print "INFO:", " ".join(str(y) for y in x)

def warning(*x):
    print "WARNING:", " ".join(str(y) for y in x)

def parse_version(ver):
    try:
        return datetime.strptime(ver, "%Y_%m_%d")
    except:
        return None

def mkdir_p(path):
    try:
        os.makedirs(path)
    except OSError as exc: # Python >2.5
        import errno
        if exc.errno == errno.EEXIST and os.path.isdir(path):
            pass
        else: raise

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


msvcp_re = re.compile("msvc[pr]1[2-9][0-9].pdb")

def get_dll_symbols(modname):
    # different for every version of windows...
    if modname.lower() in ("ntdll.dll", "kernelbase.dll"):
        return None, []
    pdb = modname.replace(".dll", ".pdb")
    lpdb = pdb.lower()
    if msvcp_re.match(lpdb):
        lpdb = lpdb.replace(".pdb", ".i386.pdb")

    pdb_path = [os.path.join(ROOT, "win32", pdb)]
    if sys.platform.startswith("darwin"):
        pdb_path.extend(os.path.join(w32, pdb) for w32 in OUTLAWS_WIN32)
    pdb_path.extend(os.path.join(sp, lpdb, "*", lpdb) for sp in PDB_SYMBOLS)
    paths = glob_files(pdb_path)
    if len(paths):
        pdb_path = paths[0]
        ext, flags = ("line", ("-l",)) if "win32" in pdb_path else ("globals", ("-g", "-p"))

        gpath = "%s.%s.gz" % (pdb_path, ext)
        if not os.path.exists(gpath) and os.path.exists(DIA2DUMP):
            fil = gzip.open(gpath, "w")
            if not fil:
                log("can't open", gpath)
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
    for base in OUTLAWS_LINUX:
        gzpath = os.path.join(ROOT, base, os.path.basename(modname) + version + ".elf.gz")
        if os.path.exists(gzpath):
            # log("using", gzpath)
            return "elf", gzip.open(gzpath)
        if os.path.exists(modname):
            fil = gzip.open(gzpath, "w")
            fil.write(subprocess.check_output(READELF % modname, shell=True))
            fil.close()
            return "elf", gzip.open(gzpath)
    return None, []

fuzzy_version = {}
def get_symbol_type_handle(modname, version):
    if modname.endswith(".dll"):
        return get_dll_symbols(modname)
    elif ".so." in modname:
        return get_so_symbols(modname)
    elif version:
        log("version is", version)
        basename = os.path.splitext(modname)[0]
        platform = "win32" if modname.endswith(".exe") else "linux"
        dirns = REL_SYM_PATH.get(basename, "")
        symbol_path = OUTLAWS_WIN32 if platform == "win32" else OUTLAWS_LINUX
        version_roots = [os.path.join(ROOT, p, dirn, version)
                         for p in symbol_path
                         for dirn in dirns]
        # print version_roots
        typ = "line" if platform == "win32" else "elf"
        symname = "%s.%s.gz" % (basename, typ)
        paths = glob_files(os.path.join(b, symname) for b in version_roots)
        if len(paths):
            # log("using", paths[0])
            return typ, gzip.open(paths[0])
        if platform == "linux" and sys.platform.startswith("linux"):
            ret = get_so_symbols(modname, version)
            if ret[0]:
                # log("using", ret[0])
                return ret
        # try earlier version (Only a few days tolerance)
        version_list = glob_files(os.path.join(os.path.dirname(x), "*_*_*/") for x in version_roots)
        vdate = parse_version(version)
        mn = timedelta(days=2)
        mnpath = None
        mnver = None
        for fil in version_list:
            ver = os.path.basename(fil[:-1])
            dat = parse_version(ver)
            if not dat:
                continue
            delta = abs(vdate - dat)
            # print ver, dat, delta
            if delta < mn:
                mn = delta
                mnpath = fil
                mnver = ver
        if mnpath:
            warning("using symbols from %s for %s" % (mnver, version))
            fuzzy_version[version] = mnver;
            return typ, gzip.open(os.path.join(mnpath, symname))
        # warning("no symbols for %s %s" % (modname, version))
    return None, []


parsed = {}
def parse_symbol_map(modname, version):
    key = (modname, version)
    if key in parsed:
        return parsed[key]
    # log("looking for", os.path.basename(modname), version)

    entries = parsed.setdefault(key, [])
    typ, handle = get_symbol_type_handle(modname, version)

    if typ == "line":
        # (Windows) output from Dia2Dump -l
        # Contains line number info
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
        # (Windows) output from Dia2Dump -g -p
        func_re = re.compile("(Function|PublicSymbol): \[([0-9A-F]+)\]\[([0-9A-F]+):([0-9A-F]+)\] (.*)")
        for line in handle:
            m = func_re.match(line)
            if m:
                entries.append(map_entry(m.group(5).strip(), int(m.group(2), 16)))
    elif typ == "map":
        # (Windows) Visual Studio generated map file
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
        # (Linux) Output from readelf -lsW -wL
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
        warning("No symbols for " + modname + (("@" + version) if version else ""))
        return entries

    # print "%d entries from %s@%s" % (len(entries), modname, version)
    if len(entries) == 0:
        warning("loaded 0 symbols from .%s %s" % (typ, handle))
    handle.close()
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
    # print module
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
    return ("%0#8x " % addr) + fmt


class extract_opts:
    def __init__(self):
        self.printall = False
        self.version = datetime.min;
        self.greps = None
        self.triage = None
        self.handlers = []
        self.matches = 0

version_re = re.compile("Build Version: ([a-zA-Z]+).*(Release|Debug|Develop|Builder|Steam)(32|64) ([^,]*),")
basere = re.compile("'([^']+)' base address is 0x([a-fA-F0-9]+), size is 0x([a-fA-F0-9]+)")
addrre = re.compile("[cC]alled from 0x([a-fA-F0-9]+)")
module_re = re.compile("In module: '([^']*)'")
mac_stack_re = re.compile("0x[a-fA-F0-9]+ (.+) [+] [0-9]+ [(]([^)]+)[)]")
hexre = re.compile("0x([a-fA-F0-9]{6,})")
sched_re = re.compile("Log upload scheduled: (.*)$")
sdl_win_re = re.compile("SDL_CreateWindow failed: (.*)$")
terminate_re = re.compile("ASSERT[(]Terminate Handler[)]: Exception: (.*)$")
memory_re = re.compile("Memory is ([0-9]+)% in use.")
vmemory_re = re.compile("([0-9. ]+)/([0-9. ]+) MB virtual memory free")

UNKNOWN_FUNC = "<unknown func>"

def close_inpt(data):
    if data != sys.stdin:
        data.close()

def extract_callstack(logf, opts):
    module_map = []
    version = ""
    lastfew = []
    exception_lines = []
    stacktrace = []
    trace_has_game = False
    is_triage = (opts.triage != None)
    is_full = (opts.triage is None)
    greps_match = (opts.greps == None)

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
        if not m:
            continue
        platform = m.group(1)
        build = m.group(2) + m.group(3)
        dat = datetime.strptime(m.group(4), "%b %d %Y")
        version = dat.strftime("%Y_%m_%d")
        if is_full:
            log("Reassembly version:", version)
        if dat < opts.version or build.startswith("Debug") or build.startswith("Develop"):
            close_inpt(data)
            return
        if is_full:
            print line.strip()
            print data.next().strip() # platform info
        break

    # analytic log handlers
    for line in data:
        line = line.strip()
        for h in opts.handlers:
            line = h.onLine(line)
        if opts.printall:
            print line
        elif is_full:
            lastfew.append(line)
            while (len(lastfew) > 5):
                lastfew.pop(0)
        for mre, prfx in [(sched_re, "CHECK"), (sdl_win_re, "SDL_CreateWindow"), (terminate_re, "TERMINATE")]:
            m = mre.search(line)
            if not m:
                continue
            reason = prfx + ": " + m.group(1)
            if is_triage:
                stacktrace = [reason]
                break
            if opts.printall and is_full:
                print line, '"' +  reason + '"'
        if is_triage and "Watchdog Thread detected hang! Crashing game" in line:
            stacktrace = ["Watchdog Thread detected hang!"]
            break
        if opts.greps and opts.greps.search(line):
            greps_match = True
        if ("Unhandled Top Level Exception" in line or \
            "Caught SIG" in line or \
            "Dumping stack for thread" in line or\
            "Terminate Handler" in line or\
            "Time is " in line or\
            "Dumping loaded shared objects" in line):
            if is_full:
                log("found crash:", line)
            break

    if not greps_match:
        close_inpt(data)
        return

    if is_full:
        for h in opts.handlers:
            h.onFinish()

    for ln in lastfew:
        print ln

    opts.matches += 1

    # crash dump
    stacklines = 0
    skipped = 0
    module = None
    for line in data:
        line = line.replace("\r", "")
        m = memory_re.search(line)
        if m:
            perc = int(m.group(1))
            # if memory load is above 95%, that probably caused the crash
            if perc > 95:
                if is_triage:
                    stacktrace = ["out of memory"]
                    break
                else:
                    log("out of memory (%d%% in use)" % perc)
        m = vmemory_re.search(line)
        if m:
            perc = 100.0 * (1.0 - float(m.group(1)) / float(m.group(2)))
            # if virtual memory load is above 95%, that probably caused the crash
            if perc > 95:
                if is_triage:
                    stacktrace = ["out of memory"]
                    break
                else:
                    log("out of virtual memory (%d%% in use)" % perc)
        m = basere.search(line)
        # log("module line", m, line)
        if m:
            mod = module_entry()
            mod.name = m.group(1)
            mod.base = int(m.group(2), 16)
            mod.size = int(m.group(3), 16)
            mod.version = version if (mod.name.endswith(".exe") or "Reassembly" in mod.name) else None
            mod.symbols = None # load lazily
            module_map.append(mod)
            # log("module:", "%#10x+%#09x" % (mod.base, mod.size), os.path.basename(mod.name), mod.version)
            continue
        elif module_map and is_full:
            for ln in exception_lines:
                func, lino, fil = lookup_address(module_map, ln[1])
                if func:
                    fmt = format_address(addr, func, lino, fil)
                    print ln[0] + fmt + ln[2],
            exception_lines = []

        if "Dumping stack" in line or "Spacetime Segfault" in line:
            if skipped and is_full:
                print "...skipped %d stack frames" % skipped
            stacklines = 0
            skipped = 0
            if stacktrace:
                if stacktrace[-1] == UNKNOWN_FUNC:
                    for line in data:
                        m = module_re.match(line)
                        if m:
                            module = m.group(1)
                            break
                break
            line = "\n" + line

        m = addrre.search(line)
        if m:
            stacklines += 1
            addr = int(m.group(1), 16)
            has_game = False
            # log("looking for %#x" % addr)
            n = mac_stack_re.search(line)
            if n:
                func, module = n.groups()
                has_game = ("Reassembly" in module)
                line = line.replace("[POSIX] ", "").replace("called from", "from")
            else:
                func, lino, fil = lookup_address(module_map, addr)
                if ignore_func(func):
                    continue
                has_game = lino > 0
                fmt = format_address(addr, func, lino, fil)
                line = line[:m.start()] + fmt + line[m.end():]
                line = line.replace("[win32] ", "").replace("called from", "from")
            if is_triage:
                if func:
                    if func.endswith (".DLL"):
                        func = func.lower()
                    func = remove_parens(func)
                else:
                    func = UNKNOWN_FUNC
                if not ignore_func(func) and (len(stacktrace) == 0 or stacktrace[-1] != func):
                    if has_game:
                        trace_has_game = True
                    stacktrace.append(func)
                    if len(stacktrace) >= TRIAGE_STACK_SIZE and trace_has_game:
                        break
        elif is_full:
            m = hexre.search(line)
            if m and "thread 0x" not in line:
                addr = int(m.group(1), 16)
                if module_map:
                    func, lino, fil = lookup_address(module_map, addr)
                    fmt = format_address(addr, func, lino, fil)
                    line = line[:m.start()] + fmt + line[m.end():]
                else:
                    exception_lines.append((line[:m.start()], addr, line[m.end():]))
                    
        if is_full:
            print line,
        elif stacklines > MAX_STACK_DUMP and trace_has_game:
            skipped += 1

    if skipped and is_full:
        print "...skipped %d stack frames" % skipped

    if is_triage:
        ver = version or "<unknown>"
        ver = fuzzy_version.get(ver, ver)
        if module:
            if not stacktrace:
                stacktrace = [module]
            elif stacktrace[-1] == UNKNOWN_FUNC:
                stacktrace[-1] = module
        if len(stacktrace) > 1 and all(x == stacktrace[0] for x in stacktrace):
            stacktrace = [stacktrace[0]]
        key = tuple(stacktrace)
        opts.triage.setdefault(ver, dict()).setdefault(key, []).append(logf)
    close_inpt(data)

    
TOP_FUNC_COUNT = 10
TOP_LOGS_PER_STACK_COUNT = 5
TRIAGE_NOISE_THRESHOLD = 1
TRIAGE_PRINT_ATLEAST = 80

crash_file_re = re.compile("_(([0-9]{1,3}[.]){4})txt.gz")
def count_ips(files):
    ips = set()
    for fil in files:
        m = crash_file_re.search(fil)
        if m:
            ips.add(m.group(1))
    return len(ips)

def do_triage(files, ops):
    triage = {}
    handlers = [AssertionHandler()]
    ops1 = copy(ops)
    ops1.printall = False
    ops1.triage = triage
    ops1.handlers = handlers
    for fil in files:
        fil = os.path.normpath(fil)
        extract_callstack(fil, ops1)
    tfiles = []
    log("Triage found %d matches" % ops1.matches)
    for version, data in sdict(triage, lambda x: x[0]):
        # data is dict(stacktrace -> list of logs)
        total = sum(len(x) for x in data.itervalues())
        total_ips = sum(count_ips(x) for x in data.itervalues())
        print "============= %s (%d total logs, %d total ips) ================" % (version, total, total_ips)
        func_counts = {}
        max_func_count = 0
        for stack, files in data.iteritems():
            for func in set(stack):
                func_counts[func] = func_counts.get(func, 0) + len(files)
                max_func_count = max(max_func_count, func_counts[func])
        if (max_func_count > 1 and len(func_counts) > 1):
            for func, count in sdict(func_counts, lambda x: x[1])[:TOP_FUNC_COUNT]:
                print "%d. %s (%.f%%)" % (count, func, 100.0 * count / total)
        print ""
        print_perc = 0
        for stack, files in sdict(data, lambda x: len(x[1])):
            pstack = "<no stack>"
            if stack:
                pstack = " <- ".join(stack)
                if len(pstack) > 100:
                    pstack = pstack.replace(" <- ", "\n   <- ")
            perc = 100.0 * len(files) / total
            ips_txt = ""
            if total_ips:
                ips = count_ips(files)
                ips_txt = " logs %d(%.1f%%) ips" % (ips, 100.0 * ips / total_ips)
            print "%d(%.1f%%)%s. %s" % (len(files), perc, ips_txt, pstack)
            print_perc += perc
            for fl in sorted(files, reverse=True)[:TOP_LOGS_PER_STACK_COUNT]:
                print "     ", fl.replace(os.path.expanduser("~"), "~")
                tfiles.append(fl)
            if len(files) > TOP_LOGS_PER_STACK_COUNT:
                print "    ..."
            if (perc < TRIAGE_NOISE_THRESHOLD and print_perc > TRIAGE_PRINT_ATLEAST):
                print "stopping - below noise threshold (%.1f%%) ignored" % (100.0 - print_perc)
                break
    for h in handlers:
        h.onFinish()
    # exit(0)

    print ""
    print ""
    print "printing %d logs" % len(tfiles)
    print ""
    for fl in tfiles:
        print "#" * (len(fl) + 4)
        print "#", fl
        print "#" * (len(fl) + 4)
        extract_callstack(fl, ops)
        print ""


def main():
    ops = extract_opts()

    triage = False
    copy = False
    latest = 1
    opts, args = getopt.getopt(sys.argv[1:], "av:tcnl:g:h")
    for (opt, val) in opts:
        if opt == "-a":
            ops.printall = True
        if opt == "-v":
            if len(val) <= 5:
                val = "2016_" + val
            ops.version = parse_version(val)
        if opt == "-t":
            triage = True
        if opt == "-c":
            copy = True
        if opt == "-n":
            ops.handlers = [TimestampHandler(), NotificationHandler(), AssertionHandler()]
        if opt == "-l":
            latest = int(val)
        if opt == "-g":
            ops.greps = re.compile(val)
        if opt == "-h":
            print """
%s [log files...]

Look up symbols in Reassembly crashlogs
Without any options, run over the newest log in ~/Downloads

Options:
 -a            print each line of every log analyzed
 -v <version>  filter by game version
 -g <regex>    filter by grep match
 -n            enable log analysis/summaries
 -t            triage over logs in server/sync/crash
 -l <count>    triage over newest <count> logs in ~/Downloads
 -c            copy symbol files to local computer from network
 -h            print this message
""" % sys.argv[0]
            exit(0)

    print "-*- mode: compilation -*-"
    log("ROOT=%s" % ROOT)
    log("version=%s" % str(ops.version))

    if copy:
        print "copying symbols from network"

        sys.path.append(os.path.join(ROOT, "server"))
        import feed
        feed.exec_command(["rsync", "-avr", "ani:anisopteragames.com/symbols/", ROOT + "/"],
                          feed.RSYNC_TIMEOUT)
        
        paths = glob_files([os.path.join(p, "steam/*/") for p in (OUTLAWS_WIN32 + OUTLAWS_LINUX) if p.startswith("/")])
        # print "checking", paths
        verignore = 0
        exists = 0
        updated = 0
        for p in paths:
            base, ver = os.path.split(p[:-1])
            base, typ = os.path.split(base)
            base, osd = os.path.split(base)
            # print ver, typ, osd

            if ops.version == datetime.min:
                ops.version = datetime.now() - timedelta(weeks=12)
            vtime = parse_version(ver)
            if vtime < ops.version:
                verignore += 1
                continue
            lpath = os.path.join(ROOT, osd, typ, ver)
            if os.path.exists(lpath):
                if os.path.getmtime(lpath) >= os.path.getmtime(p):
                    exists += 1
                    continue
                shutil.rmtree(lpath, True)
                updated += 1
            print "copy", p, "to", lpath
            mkdir_p(lpath)
            for fl in glob.glob(p + "/*"):
                shutil.copyfile(fl, os.path.join(lpath, os.path.basename(fl)))
            print "done"
        log("%d total builds, %d ignored based on date, %d already local, %d updated" % (len(paths), verignore, exists, updated))
    elif triage:
        files = []
        if len(args):
            files = args
            log("Triaging over %d crashlogs", len(files))
        else:
            base = os.path.join(ROOT, "server/sync/crash")
            all_files = safe_listdir(base)
            for fil in all_files:
                try:
                    date = datetime.strptime(fil.split("_")[0], "%Y%m%d")
                except ValueError:
                    warning("skipping '%s'", fil)
                    continue
                if date >= ops.version:
                    files.append(os.path.join(base, fil))
            log("Triaging over %d/%d server crashlogs" % (len(files), len(all_files)))
        do_triage(files, ops)
    elif len(args) == 0:
        logs = glob_files([os.path.join(os.path.expanduser("~/Downloads"), "Reassembly_*.txt"),
                           os.path.join(os.path.expanduser("~/Downloads"), "log_latest*.txt"),
                           os.path.join(ROOT, "data", "log_latest.txt")])
        if len(logs):
            print "picking %d most recently modified log(s)" % latest
            logs.sort(key=lambda x: os.path.getmtime(x))
            if latest > 1:
                files = logs[-latest:]
                print "Triaging over %d/%d crashlogs" % (len(files), len(logs))
                do_triage(files, ops)
            else:
                last = logs[-1]
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
                fils = glob.glob(os.path.join(ROOT, fil))
                files.extend(fils)
                print "globbing '%s' resulted in %d files" % (fil, len(fils))
            else:
                files.append(fil)
        if len(files) == 1:
            print "processing %s" % files[0]
        else:
            print "processing %d logs" % (len(files))
        do_triage(files, ops)


if __name__ == '__main__':
    ROOT = os.path.abspath(os.path.join(os.path.dirname(sys.argv[0]), ".."))
    ROOT = ROOT.replace("/cygdrive/c/", "c:/")
    main()
