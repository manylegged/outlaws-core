
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


#include "Str.h"

std::string str_format(const char *format, ...)
{
    va_list vl;
    va_start(vl, format);
    
    //std::string s(vsnprintf(NULL, 0, format, vl) , ' ');
    //vsnprintf((char*) s.c_str(), s.size()+1, format, vl);
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
