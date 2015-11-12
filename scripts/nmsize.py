#!/usr/bin/python

# this is a script for optimizing binary size and compile time it outputs information about which
# symbols (functions) are contributing most to the binary size, which are duplicated the most
# times, which data sections are the largest, etc.

# nmsize.py <binary> [regex]

import sys, re, subprocess

template_re = re.compile("([a-zA-Z0-9:_]+)<")
pairs = {"<":">", "(":")"}

def sdict(dic, lmba=lambda x: x[1]):
    return sorted(list(dic.items()), key=lmba, reverse=True)

def skip_parens(ss):
    if len(ss) < 3:
        return "", ss
    start = ss[0]
    if start not in pairs:
        return "", ss
    end = pairs[start]
    count = 0
    index = 0
    for ch in ss:
        index += 1
        if ch == start:
            count += 1
        elif ch == end:
            count -= 1
            if count == 0:
                break
    return (start + "..." + end), ss[index:]


def erase_tmpl_args(ss):
    m = template_re.search(ss)
    if m:
        a, b = skip_parens(ss[m.end()-1:]) # skip template <> args
        ss = ss[:m.end()-1] + a
        a, b = skip_parens(b)   # skip arguments of templated function
        ss += a + erase_tmpl_args(b)
    return ss.replace("::__1::", "::")

# print erase_tmpl_args("unsigned int std::__sort4<>(std::pair<>*, std::pair<>*, std::pair<>*, std::pair<>*, AI::getAttackCaps()::$_0&)")
# print erase_tmpl_args("std::__1::vector<std::__1::basic_string<char, std::__1::char_traits<char>, std::__1::allocator<char> >, std::__1::allocator<std::__1::basic_string<char, std::__1::char_traits<char>, std::__1::allocator<char> > > >::push_back(std::__1::basic_string<char, std::__1::char_traits<char>, std::__1::allocator<char> >&&)")
# exit(0)

NM = "gnm" if sys.platform.startswith("darwin") else "nm"
pipe = subprocess.Popen([NM, "--print-size", "--demangle", "--size-sort", sys.argv[1]], stdout=subprocess.PIPE)

pat = len(sys.argv) > 2 and re.compile(sys.argv[2])
ignore_re = re.compile("(%s)" % "|".join(["re2", "EH_frame", "GCC_except", "__clang", "L_.str"]))

totalSize = 0
patSize = 0
patCount = 0
totalCount = 0

syms = {}
data = {}
funcs = {}

for line in pipe.stdout:
    try:
        fields = line.strip().split(" ")
        addr, size, typ, symbol = fields[0], fields[1], fields[2], " ".join(fields[3:])
    except:
        print "ERROR: ", line
        continue
    size = int(size, 16)
    totalSize += size
    totalCount += 1
    # if ignore_re.search(symbol):
        # continue
    symbol = erase_tmpl_args(symbol)
    if typ in "Dd":
        data[symbol] = size
    else:
        cnt, tsize = funcs.get(symbol, (0, 0))
        funcs[symbol] = (cnt + 1, tsize + size)
    syms[symbol] = syms.get(symbol, 0) + 1
    if pat:
        m = pat.search(symbol)
        if m:
            patSize += size
            patCount += 1

namespace_re = re.compile("([a-zA-Z0-9_]+::)[a-zA-Z0-9_]+")
namespaces = {}
for sym, dat in funcs.iteritems():
    m = namespace_re.search(sym)
    if m:
        nm = m.group(1)
        cnt, tsize = namespaces.get(nm, (0, 0))
        namespaces[nm] = (cnt + dat[0], tsize + dat[1])
            
def fmt_size(size):
    return "%5.1fkb (%4.1f%%)" % (size / 1024.0, 100.0 * size / totalSize)

def fmt_count(cnt):
    return "%4d (%4.1f%%)" % (cnt, 100.0 * cnt / totalCount)

def fmt_index(i):
    # return "%2d." % (i+1)
    return ""

print "total: %d syms, %.1f kb" % (totalCount, totalSize / 1024.0)
if pat:
    print "%s: %d syms (%.1f%%), %.1f kb (%.1f%%)" % (pat.pattern, patCount, 100.0 * patCount / totalCount,
                                                      patSize / 1024.0, 100.0 * patSize / totalSize)

# print "################# duplicate symbols ######################"
# dups = sdict(syms)
# for i in xrange(10):
#     cnt = dups[i][1]
#     if cnt > 1:
#         print fmt_index(i), dups[i][1], dups[i][0] 

print "################# large data sections ######################"
i = 0
for sym, size in sdict(data):
    print fmt_index(i), fmt_size(size), sym
    i += 1
    if i > 10 or size < 1024:
        break;

print "################# large functions ######################"
i = 0
for sym, dat in sdict(funcs, lambda x:x[1][1]):
    count, size = dat
    print fmt_index(i), fmt_size(size), fmt_count(count), sym
    i += 1
    if i > 20:
        break;

print "################# large namespaces ######################"
i = 0
for sym, dat in sdict(namespaces, lambda x:x[1][1]):
    count, size = dat
    print fmt_index(i), fmt_size(size), fmt_count(count), sym
    i += 1
    if i > 10:
        break;

