
//
// Str.cpp - string utilities
// 

// Copyright (c) 2013 Arthur Danskin
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
#if 1
    va_list vl2;
    va_copy(vl2, vl);
    const int chars = vsnprintf(NULL, 0, format, vl2);
    va_end(vl2);
    std::string s(chars, ' ');
    int r = vsnprintf((char*) s.data(), chars+1, format, vl);
    ASSERT(r == chars);
#else
    std::string s;
    size_t mchars = 128;
    char* buf = (char*) malloc(mchars);
    int written = vsnprintf(buf, mchars+1, format, vl);
    s = buf;
    free(buf);
#endif
    return s;
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


std::string str_path_standardize(const std::string &str)
{
    string path(str.size(), ' ');
    int rdx = 0;
    int idx = 0;
    while (rdx < str.size()) 
    {
        while (str[rdx] == '/' && rdx > 0 && str[rdx-1] == '/' && rdx < str.size())
            rdx++;

        if (rdx > 0 && str[rdx] == '.' && str[rdx-1] == '.' &&
            idx > 2 && path[idx-3] != '.')
        {
            int i=idx-3;
            while (path[i--] != '/' && i >= 0);
            rdx++;
            idx = i+1;
            while (str[rdx] == '/')
                rdx++;
        }
        path[idx] = str[rdx];
        idx++;
        rdx++;
    }
    while (idx > 0 && strchr("/ ", path[idx-1]))
        idx--;
    path.resize(idx);
    return path;
}


std::string str_dirname(const std::string &str)
{
    size_t end = str.size()-1;
    while (end > 0 && str[end] == '/')
        end--;
    size_t pt = str.rfind("/", end);
    if (pt == std::string::npos)
        return ".";
    else if (pt == 0)
        return "/";
    else
        return str.substr(0, pt);
}

std::string str_basename(const std::string &str)
{
    size_t end = str.size()-1;
    while (end > 0 && str[end] == '/')
        end--;
    size_t pt = str.rfind("/", end);
    if (pt == std::string::npos)
        return str;
    else
        return str.substr(pt+1);
}

string str_tohex(const unsigned char* digest, int size)
{
    const char hexchars[] = "0123456789abcdef";

    string result;

    for (int i = 0; i < 16; i++)
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



#if DEBUG

#define assert_eql(A, B) ASSERTF(A == B, "'%s' != '%s'", str_tostr(A).c_str(), str_tostr(B).c_str())

static int strtests()
{
    assert_eql(str_path_standardize("~/Foo//Bar.lua"), "~/Foo/Bar.lua");
    assert_eql(str_path_standardize("../../bar.lua"), "../../bar.lua");
    assert_eql(str_path_standardize("foo/baz/../../bar.lua////"), "bar.lua");
    assert_eql(str_path_standardize("foo/baz/.."), "foo");
    assert_eql(str_path_standardize("foo/baz/../"), "foo");
    assert_eql(str_path_join("foo", "bar"), "foo/bar");
    assert_eql(str_path_join("foo/", "bar"), "foo/bar");
    assert_eql(str_path_join("foo/", "/bar"), "foo/bar");
    assert_eql(str_path_join("foo/", (const char*)NULL), "foo");
    assert_eql(str_path_join("foo/", ""), "foo");
    assert_eql(str_path_join("/home/foo", "bar"), "/home/foo/bar");
    assert_eql(str_path_join(str_path_join("/home/foo", "bar"), "/baz"), "/home/foo/bar/baz");
    assert_eql(str_dirname("foo/"), ".");
    assert_eql(str_dirname("/foo/"), "/");
    assert_eql(str_dirname("foo/baz/bar///"), "foo/baz");
    return 1;
}

static int _strtests = strtests();

#endif

#if _MSC_VER

std::string str_demangle(const char *str)
{
    string name = str;
    name = str_replace(name, "struct ", "");
    name = str_replace(name, "class ", "");
    return name;
}

#else

#include <cxxabi.h>

std::string str_demangle(const char *str)
{
    int status;
    char* result = abi::__cxa_demangle(str, NULL, NULL, &status);
    string name = result;
    free(result);
    name = str_replace(name, "std::__1::", "std::");
    return name;
}



#endif
