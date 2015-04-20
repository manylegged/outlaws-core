
//
// Str.cpp - string utilities
// 

// Copyright (c) 2013-2015 Arthur Danskin
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

#include "StdAfx.h"
#include "Str.h"

namespace std {
    template class basic_string<char>;
    template class vector<string>;
    template class unordered_set<string>;
    template class lock_guard<mutex>;
    template class lock_guard<recursive_mutex>;
}

std::string str_format(const char *format, ...)
{
    va_list vl;
    va_start(vl, format);
    std::string s = str_vformat(format, vl);
    va_end(vl);
    return s;
}


std::string str_vformat(const char *format, va_list vl) 
{
    va_list vl2;
    va_copy(vl2, vl);
    const int chars = vsnprintf(NULL, 0, format, vl2);
    va_end(vl2);
    std::string s(chars, ' ');
    int r = vsnprintf(&s[0], chars+1, format, vl);
    ASSERT(r == chars);
    return s;
}

void str_append_vformat(std::string &str, const char *format, va_list vl)
{
    va_list vl2;
    va_copy(vl2, vl);
    const int chars = vsnprintf(NULL, 0, format, vl2);
    va_end(vl2);
    const int start = str.size();
    str.resize(start + chars);
    vsnprintf(&str[start], chars+1, format, vl);
}
    
void str_append_format(std::string &str, const char *format, ...)
{
    va_list vl;
    va_start(vl, format);
    str_append_vformat(str, format, vl);
    va_end(vl);   
}

std::string str_add_line_numbers(const char* s, int start)
{
    std::string r;
    int i=start;
    r += str_format("%d: ", i++);
    for (const char* p = s; *p != '\0'; ++p) {
        r += *p;
        if (*p == '\n') {
            r += str_format("%d: ", i++);
        }
    }
    return r;
}


vector<string> str_split_quoted(char token, const string& line)
{
    vector<string> vec;
    bool quoted = false;
    int instr = 0;
    string last;
    foreach (char c, line)
    {
        if (instr) {
            if (quoted) {
                quoted = false;
            } else if (c == '\\') {
                quoted = true;
            } else if (c == instr) {
                instr = false;
            }
        } else if (c == '\'' || c == '"') {
            instr = c;
        } else if (c == token) {
            vec.push_back(last);
            last = "";
            continue;
        }
        last += c;
    }
    if (last.size())
        vec.push_back(last);
    return vec;
}


long chr_unshift(long chr)
{
    chr = std::tolower(chr);
    switch (chr) {
    case '!': return '1';
    case '@': return '2';
    case '#': return '3';
    case '$': return '4';
    case '%': return '5';
    case '^': return '6';
    case '&': return '7';
    case '*': return '8';
    case '(': return '9';
    case ')': return '0';
    }
    return chr;
}


std::string str_word_wrap(std::string str, size_t width, const char* newline, const char* wrap)
{
    size_t i = 0;
    size_t lineStart = 0;
    size_t lastSpace = 0;
    const size_t nlsize = strlen(newline);

    while (str.size() - lineStart > width && i<str.size())
    {
        if (str[i] == '\n') {
            lineStart = i;
        } else if (strchr(wrap, str[i])) {
            lastSpace = i;
        }
        if (i - lineStart > width) {
            str.replace(lastSpace, 1, newline);
            lineStart = lastSpace + nlsize;
            i += nlsize-1;
        }
        i++;
    }

    return str;
}

std::string str_align(const std::string& input, char token)
{
    int alignColumn = 0;
    int lineStart = 0;
    int tokensInLine = 0;
    for (int i=0; i<input.size(); i++) {
        if (input[i] == '\n') {
            lineStart = i;
            tokensInLine = 0;
        } else if (input[i] == token &&
                   input.size() != i+1 && input[i+1] != '\n' &&
                   tokensInLine == 0) {
            alignColumn = max(alignColumn, i - lineStart + 1);
            tokensInLine++;
        }
    }

    if (alignColumn == 0)
        return input;

    std::string str;
    lineStart = 0;
    tokensInLine = 0;
    
    for (int i=0; i<input.size(); i++) {
        str += input[i];
        if (input[i] == '\n') {
            lineStart = i;
            tokensInLine = 0;
        } else if (input[i] == token && tokensInLine == 0) {
            const int spaces = alignColumn - (i - lineStart);
            for (; (i+1)<input.size() && input[i+1] == ' '; i++);
            if (input[i+1] != '\n')
                str.append(spaces, ' ');
            tokensInLine++;
        }
    }
    return str;
}

// http://stackoverflow.com/questions/154536/encode-decode-urls-in-c
std::string str_urlencode(const std::string &value) 
{
    std::string ret;

    for (string::const_iterator i = value.begin(), n = value.end(); i != n; ++i) 
    {
        string::value_type c = (*i);

        // Keep alphanumeric and other accepted characters intact
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            ret += c;
            continue;
        }

        // Any other characters are percent-encoded
        ret += str_format("%%%2x", int((unsigned char) c));
    }

    return ret;
}

std::string str_urldecode(const std::string &value) 
{
    std::string ret;

    char seq[3] = {};
    int in_seq = 0;
    
    for (int i=0; i<value.size(); i++)
    {
        const char c = value[i];
        if (in_seq && std::isxdigit(c)) {
            seq[in_seq++ - 1] = c;
            if (in_seq == 3) {
                in_seq = 0;
                long num = strtol(seq, NULL, 16);
                if (num && num < 0xF7)
                    ret += num;
            }
        } else if (in_seq) {
            // abort, put it back
            ret += '%';
            for (int j=0; j<in_seq; j++)
                ret += seq[j];
            ret += c;
        } else if (c == '%') {
            in_seq = 1;
        } else {
            ret += c;
        }
    }
 
    return ret;
}

std::string str_time_format(float seconds)
{
    seconds = max(epsilon, seconds);
    std::string ret;
    const int minutes = floor(seconds / 60.f);
    const int hours   = floor(seconds / 3600.f);
    if (!minutes)
        return str_format("%02.1f", seconds);
    else if (!hours)
        return str_format("%3d:%02d", minutes, modulo((int)floor(seconds), 60));
    else
        return str_format("%3d:%02d:%.02d", hours, modulo(minutes, 60),
                          modulo((int)floor(seconds), 60));
}

std::string str_reltime_format(float seconds)
{
    if (seconds > 0)
        return "in " + str_time_format(seconds);
    else
        return str_time_format(-seconds) + " ago";
}

std::string str_numeral_format(int num)
{
    static const char* numerals[] = { "zero", "one", "two", "three", "four", "five", "six",
                                    "seven", "eight", "nine", "ten", "eleven", "twelve",
                                    "thirteen", "fourteen", "fifteen", "sixteen",
                                    "seventeen", "eighteen", "nineteen" };
    static const char* tens[] = { "twenty", "thirty", "forty", "fifty", "sixty", "seventy", "eighty", "ninety" };
    if (abs(num) >= 100)
        return str_format("%d", num);
    std::string ret;
    if (num < 0)
        ret += "negative ";
    num = abs(num);
    if (num < 20) {
        ret += numerals[num];
    } else {
        ret += tens[num / 10 - 2];
        ret += " ";
        ret += numerals[num % 10];
    }
    return ret;
}


std::string str_bytes_format(int bytes)
{
    static const double kilo = 1000.0; // 1024.0;
    if (bytes < kilo)
        return str_format("%d B", bytes);
    else if (bytes < kilo * kilo)
        return str_format("%.1f KB", bytes / kilo);
    else if (bytes < kilo * kilo * kilo)
        return str_format("%.1f MB", bytes / (kilo * kilo));
    else
        return str_format("%.1f GB", bytes / (kilo * kilo * kilo));
}

template <typename T>
static std::basic_string<T> str_path_standardize1(const std::basic_string<T> &str, T sep)
{
    const int root_size = (str.size() && str[0] == '/') ? 1 :
                          (str.size() > 1 && str[1] == ':') ? 3 : 0;
    if (root_size == 3)
        sep = T('\\');
    const T wsep = (sep == T('/')) ? T('\\') : T('/');
    std::basic_string<T> path;
    if (root_size) {
        for (int i=0; i<root_size-1; i++)
            path += str[i];
        path += sep;
    }
    for (int sidx=root_size; sidx < str.size(); sidx++)
    {
        const T cur = str[sidx] == wsep ? sep : str[sidx];
        const T lst = sidx > root_size ? (str[sidx-1] == wsep ? sep : str[sidx-1]) : T(0);
        
        if (cur == sep && lst == sep) // foo// -> foo/
        {
        }
        else if (cur == '.' && lst == '.') // foo/bar/.. -> foo/ .. -> .. or /.. -> /
        {
            if (path.size() > 2+root_size && path[path.size()-3] != '.')
            {
                int i;
                for (i=path.size()-3; i > root_size && path[i] != sep; i--);
                path.resize(max(i, root_size));
            }
            else if (root_size)
            {
                path.pop_back();
            }
            else
            {
                path += cur;
            }
        }
        else if (cur == sep && lst == '.' && // foo/./ -> foo/
                 (sidx == 1 || str[sidx-2] != '.'))
        {
            path.pop_back();
        }
        else if (path.empty() && cur == sep)
        {
        }
        else
        {
            path += cur;
        }
    }
    while (path.size() > root_size && path.back() == sep)
        path.pop_back();
    if (!path.size())
        path += (T)'.';
    return path;
    // return *new std::basic_string<T>(path);
}

std::string str_path_standardize(std::string str)
{
    return str_path_standardize1(str, '/');
}

std::wstring str_w32path_standardize(const std::wstring &str)
{
    return str_path_standardize1(str, L'\\');
}

static std::string str_w32path_standardize(const std::string &str)
{
    return str_path_standardize1(str, '\\');
}


std::string str_dirname(const std::string &str)
{
    size_t end = str.size()-1;
    while (end > 0 && strchr("/\\", str[end]))
        end--;
    size_t pt = str.find_last_of("/\\", end);
    if (pt == std::string::npos)
        return ".";
    else
        return str.substr(0, max((size_t)1, pt));
}

std::string str_basename(const std::string &str)
{
    size_t end = str.size()-1;
    while (end > 0 && strchr("/\\", str[end]))
        end--;
    size_t pt = str.find_last_of("/\\", end);
    if (pt == std::string::npos)
        return str;
    else
        return str.substr(pt+1);
}

string str_tohex(const char* digest, int size)
{
    const char *hexchars = "0123456789abcdef";

    string result;

    for (int i = 0; i < size; i++)
    {
        unsigned char b = digest[i];
        char hex[3];

        hex[0] = hexchars[b >> 4];
        hex[1] = hexchars[b & 0xF];
        hex[2] = 0;

        result.append(hex);
    }
    return result;
}

std::string str_capitalize(const char* str)
{
    std::string s = str;
    s[0] = toupper(s[0]);
    for (int i=1; i<s.size(); i++) {
        if (str_contains(" \n\t_-", s[i-1]))
            s[i] = toupper(s[i]);
        else
            s[i] = tolower(s[i]);
    }
        
    return s;
}

bool str_runtests()
{
#if IS_DEVEL
    // str_w32path_standardize(L"C:/foo/bar");
    assert_eql(str_path_standardize("/foo/../.."), "/");
    assert_eql(str_path_standardize("~/Foo//Bar.lua"), "~/Foo/Bar.lua");
    assert_eql(str_path_standardize("../../bar.lua"), "../../bar.lua");
    assert_eql(str_path_standardize("foo/baz/../../bar.lua////"), "bar.lua");
    assert_eql(str_path_standardize("foo/baz/.."), "foo");
    assert_eql(str_path_standardize("foo/../"), ".");
    assert_eql(str_path_standardize("foo/baz/../"), "foo");
    assert_eql(str_path_standardize("foo/baz/./"), "foo/baz");
    assert_eql(str_path_standardize("foo//baz"), "foo/baz");
    assert_eql(str_path_standardize("foo/baz/./.."), "foo");
    assert_eql(str_path_standardize("./foo"), "foo");
    assert_eql(str_path_standardize("foo/../.."), "..");
    assert_eql(str_path_standardize("/../../../../../../.."), "/");
    assert_eql(str_path_standardize("c:/foo/../.."), "c:\\");
    assert_eql(str_path_standardize("/foo подпис/공전baz/../"), "/foo подпис");
    assert_eql(str_path_standardize("foo/../正文如下：///"), "正文如下：");
    assert_eql(str_path_standardize("C:\\foo\\bar\\..\\"), "C:\\foo");
    assert_eql(str_path_standardize("C:\\foo\\..\\..\\..\\.."), "C:\\");
    assert_eql(str_path_standardize("foo\\..\\.."), "..");
    assert_eql(str_w32path_standardize("C:/foo/bar/../"), "C:\\foo");
    assert_eql(str_w32path_standardize("foo/bar/../baz"), "foo\\baz");
    assert_eql(str_path_join("foo", "bar"), "foo/bar");
    assert_eql(str_path_join("foo/", "bar"), "foo/bar");
    assert_eql(str_path_join("foo/", "/bar"), "/bar");
    assert_eql(str_path_join("foo/", (const char*)NULL), "foo/");
    assert_eql(str_path_join("foo/", ""), "foo/");
    assert_eql(str_path_join("/home/foo", "bar"), "/home/foo/bar");
    assert_eql(str_path_join("/home/foo", "bar", "/baz"), "/baz");
    assert_eql(str_path_join("/foo/bar", "c:/thing"), "c:/thing");
    assert_eql(str_path_join("c:/foo/bar", "thing"), "c:/foo/bar/thing");
    assert_eql(str_path_join("c:\\", "thing"), "c:\\thing");
    assert_eql(str_path_join("/foo/bar", "/thing"), "/thing");
    assert_eql(str_dirname("foo/"), ".");
    assert_eql(str_dirname("/foo/"), "/");
    assert_eql(str_dirname("foo/baz/bar///"), "foo/baz");
    const char *url = "http://www.anisopteragames.com/forum/viewtopic.php?f=4&t=1136#$@#TW$#^$%*^({}[";
    assert_eql(str_urldecode(str_urlencode(url)), url);
    assert_eql(str_find(url, "?f"), str_find(std::string(url), "?f"));
    assert_eql(str_find(url, "?f"), str_find(url, std::string("?f")));
    assert_eql(str_rfind(url, "?f"), str_rfind(std::string(url), "?f"));
    assert_eql(str_rfind(url, "?f"), str_rfind(url, std::string("?f")));
    assert_eql(str_substr(url, 10, 5), str_substr(std::string(url), 10, 5));
    assert_eql(str_numeral_format(57), "fifty seven");
    assert_eql(str_numeral_format(-1), "negative one");
#endif
    return 1;
}

#if _MSC_VER

#include <intrin.h>
#define cpuid(OUT, LEVEL) __cpuid(OUT, LEVEL)
typedef int regtype_t;

std::string str_demangle(const char *str)
{
    string name = str;
    name = str_replace(name, "struct ", "");
    name = str_replace(name, "class ", "");
    name = str_replace(name, "std::basic_string<char,std::char_traits<char>,std::allocator<char> >", "std::string");
    return name;
}

#else

#include <cxxabi.h>
#include <cpuid.h>
typedef unsigned int regtype_t;
#define cpuid(OUT, LEVEL) __get_cpuid(LEVEL, &(OUT)[0], &(OUT)[1], &(OUT)[2], &(OUT)[3])

std::string str_demangle(const char *str)
{
    int status;
    char* result = abi::__cxa_demangle(str, NULL, NULL, &status);
    ASSERTF(status == 0, "__cxa_demangle:%d: %s", status,
            ((status == -1) ? "memory allocation failed" :
             (status == -2) ? "input not valid name under C++ ABI mangling rules" :
             (status == -3) ? "invalid argument" : "unexpected error code"));
    if (status != 0 || !result)
        return str;
    string name = result;
    free(result);
    name = str_replace(name, "std::__1::", "std::");
    name = str_replace(name, "unsigned long long", "uint64");
    name = str_replace(name, "basic_string<char, std::char_traits<char>, std::allocator<char> >", "string");
#if __APPLE__
    name = str_replace(name, "glm::detail::tvec2<float, (glm::precision)0>", "float2");
    name = str_replace(name, "glm::detail::tvec3<float, (glm::precision)0>", "float3");
#else
    name = str_replace(name, "glm::detail::tvec2<float,0>", "float2");
    name = str_replace(name, "glm::detail::tvec3<float,0>", "float3");
#endif
    return name;
}

#endif

std::string str_cpuid()
{
    regtype_t CPUInfo[4] = {};
    char CPUBrandString[0x40] = {};
    // Get the information associated with each extended ID.
    cpuid(CPUInfo, 0x80000000);
    unsigned nExIds = CPUInfo[0];
    for (unsigned i=0x80000000; i<=nExIds; ++i)
    {
        cpuid(CPUInfo, i);
        // Interpret CPU brand string
        if  (i == 0x80000002)
            memcpy(CPUBrandString, CPUInfo, sizeof(CPUInfo));
        else if  (i == 0x80000003)
            memcpy(CPUBrandString + 16, CPUInfo, sizeof(CPUInfo));
        else if  (i == 0x80000004)
            memcpy(CPUBrandString + 32, CPUInfo, sizeof(CPUInfo));
    }
    return str_strip(CPUBrandString);
}

// for mac OS layer...
extern "C" const char* str_cpuid_(void);

const char* str_cpuid_(void)
{
    static std::string s = str_cpuid();
    return s.c_str();
}

