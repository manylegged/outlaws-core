
//
// Str.h - string utilities
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

#ifndef STR_H
#define STR_H

#include <string>
#include <sstream>
#include <utility>
#include <cctype>

// lexicon-ized string - basically a symbol
// very fast to copy around, compare
// can convert to const char* or std::string
struct lstring
{
private:
    struct Lexicon
    {
        std::set<std::string> strings;

        const char* intern(const std::string& str)
        {
            return strings.insert(str).first->c_str();
        }

        static Lexicon& instance()
        {
            static Lexicon l;
            return l;
        }
    };

    const char* m_ptr = NULL;

public:
    lstring() {}
    lstring(const std::string& str) : m_ptr(Lexicon::instance().intern(str)) { }
    lstring(const char* str)        : m_ptr(Lexicon::instance().intern(str ? str : "")) { }
    lstring(const lstring& o)       : m_ptr(o.m_ptr) {}

    std::string str()   const { return m_ptr ? std::string(m_ptr) : ""; }
    const char* c_str() const { return m_ptr; }
    bool empty()        const { return !m_ptr || m_ptr[0] == '\0'; }

    bool operator<(const lstring& o) const 
    {
        return !m_ptr ? true : !o.m_ptr ? false : strcmp(m_ptr, o.m_ptr) < 0;
    }

    bool operator==(const lstring &o) const { return m_ptr == o.m_ptr; }
    bool operator!=(const lstring &o) const { return m_ptr != o.m_ptr; }
};

namespace std {
    template <>
    struct hash< lstring > {
        std::size_t operator()(const lstring pt) const
        {
            // hashing the pointer, not the string
            return std::hash<const char*>()(pt.c_str());
        }
    };
}


template <typename T>
inline std::string str_replace(T&& s, const char* a, const char* b)
{
    std::string r(std::forward<T>(s));
    std::string::size_type n = strlen(a);
    std::string::size_type bn = strlen(b);
    for (std::string::size_type i = r.find(a); i != r.npos; i = r.find(a, i + bn))
    {
        r.replace(i, n, b);
    }
    return r;
}

template <typename T>
inline std::string str_indent(T&& s, int amount)
{
    std::string istr = std::string(' ', amount) + '\n';
    return str_replace(std::forward<T>(s), "\n", istr.c_str());
}

inline std::string str_strip(const std::string& s)
{
    int st=0;
    while (st < (int)s.size() && isspace(s[st])) {
        st++;
    }
    int ed=(int) s.size()-1;
    while (ed >= st && isspace(s[ed])) {
        ed--;
    }
    return s.substr(st, ed-st+1);
}

inline std::string str_chomp(const std::string &s)
{
    int ed=(int) s.size()-1;
    while (isspace(s[ed])) {
        ed--;
    }
    return s.substr(0, ed+1);
}

inline std::string str_chomp(std::string &&s)
{
    int ed=(int) s.size()-1;
    while (isspace(s[ed])) {
        ed--;
    }
    s.resize(ed+1);
    return s;
}

std::string str_vformat(const char *format, va_list vl) __printflike(1, 0);
std::string str_format(const char *format, ...)  __printflike(1, 2);

inline std::string str_addLineNumbers(const char* s, int start=1)
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

inline std::vector<std::string> &str_split(const std::string &s, char delim, std::vector<std::string> &elems) 
{
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, delim)) {
        elems.push_back(item);
    }
    return elems;
}


inline std::vector<std::string> str_split(const std::string &s, char delim) 
{
    std::vector<std::string> elems;
    str_split(s, delim, elems);
    return elems;
}

inline std::string str_tostr(float a) { return str_format("%f", a); }
inline std::string str_tostr(int a)   { return str_format("%d", a); }
inline std::string str_tostr(uint a)  { return str_format("0x%x", a); }
inline std::string str_tostr(const char *a) { return a ? a : ""; }
inline std::string str_tostr(const std::string &a) { return a; }
inline std::string str_tostr(lstring a) { return a.str(); }

template <class A>
std::string str_join(const A &cont, const std::string &sep)
{
    std::string result;
    for (typename A::const_iterator it=cont.begin(), end=cont.end(); it!=end; ++it)
    {
        if (!result.empty())
            result.append(sep);
        result.append(str_tostr(*it));
    }
    return result;
}

template <class A>
std::string str_join(const A &begin, const A &end, const std::string &sep)
{
    std::string result;
    for (A it=begin; it!=end; it++)
    {
        if (!result.empty())
            result.append(sep);
        result.append(str_tostr(*it));
    }
    return result;
}

template <class A>
std::string str_join_keys(const A &begin, const A &end, const std::string &sep)
{
    std::string result;
    for (A it=begin; it!=end; it++)
    {
        if (!result.empty())
            result.append(sep);
        result.append(str_tostr(it->first));
    }
    return result;
}

inline std::string str_word_wrap(std::string str, size_t width = 70)
{
    size_t curWidth = width;
    while( curWidth < str.length() ) {
        std::string::size_type spacePos = str.rfind( ' ', curWidth );
        if( spacePos == std::string::npos )
            spacePos = str.find( ' ', curWidth );
        if( spacePos != std::string::npos ) {
            str[ spacePos ] = '\n';
            curWidth = spacePos + width + 1;
        }
    }

    return str;
}

inline std::string str_capitalize(const char* str)
{
    string s = str;
    s[0] = toupper(s[0]);
    return s;
}

inline std::string str_toupper(const char* str)
{
    string s = str;
    for (uint i=0; i<s.size(); i++)
        s[i] = std::toupper(s[i]);
    return s;
}

inline std::string str_tolower(const char* str)
{
    string s = str;
    for (uint i=0; i<s.size(); i++)
        s[i] = std::tolower(s[i]);
    return s;
}


#endif
