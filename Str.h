
//
// Str.h - string utilities
//
// API style is largely copied from python
// most functions work on cstrings or std::string
// 
// lstring is intended to be used as a "symbol" rather than a byte array

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
        std::mutex                      mutex;
        std::unordered_set<std::string> strings;

        const char* intern(const std::string& str)
        {
            std::lock_guard<std::mutex> l(mutex);
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
    lstring(const char* str)        : m_ptr(str ? Lexicon::instance().intern(str) : NULL) { }
    lstring(const lstring& o)       : m_ptr(o.m_ptr) {}

    std::string str()   const { return m_ptr ? std::string(m_ptr) : ""; }
    const char* c_str() const { return m_ptr; }
    bool empty()        const { return !m_ptr || m_ptr[0] == '\0'; }
    
    void clear() { m_ptr = NULL; }

    bool operator<(const lstring& o) const 
    {
        // could just do pointer compare here for speed, but sometimes nice to be alphabetical
        return (m_ptr && o.m_ptr) ? (strcmp(m_ptr, o.m_ptr) < 0) : (m_ptr < o.m_ptr);
    }
    
    explicit operator bool() const
    {
        return !empty();
    }
    
    bool operator==(const lstring &o) const { return m_ptr == o.m_ptr; }
    bool operator!=(const lstring &o) const { return m_ptr != o.m_ptr; }

    static size_t lexicon_size() { return Lexicon::instance().strings.size(); }
    
    static size_t lexicon_bytes()
    {
        size_t sz = sizeof(Lexicon);
        Lexicon &lex = Lexicon::instance();
        std::lock_guard<std::mutex> l(lex.mutex);
        sz += lex.strings.bucket_count() * lex.strings.load_factor() * sizeof(std::string);
        for (const std::string &str : lex.strings)
            sz += str.size();
        return sz;
    }
    
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

inline size_t str_len(std::nullptr_t null)    { return 0; }
inline size_t str_len(long null)              { return 0; }
inline size_t str_len(const char* str)        { return str ? strlen(str) : 0; }
inline size_t str_len(const std::string& str) { return str.size(); }
inline size_t str_len(char chr)               { return 1; }

template <typename T>
inline size_t str_find(const std::string &s, const T& v, size_t pos=0) { return s.find(v, pos); }

inline size_t str_find(const char* s, char v, size_t pos=0)
{
    if (pos > strlen(s))
        return std::string::npos;
    const char *p = strchr(s+pos, v);
    return p ? (size_t) (p - s) : std::string::npos;
}

inline size_t str_find(const char* s, const char* v, size_t pos=0)
{
    if (pos > strlen(s))
        return std::string::npos;
    const char *p = strstr(s+pos, v);
    return p ? (size_t) (p - s) : std::string::npos;
}

template <typename T>
inline size_t str_find(const char* &s, const std::string& v, size_t pos=0)
{
    return str_find(s, v.c_str(), pos);
}

inline std::string str_substr(const std::string &st, size_t idx, size_t len=std::string::npos) { return st.substr(idx, len); }

inline std::string str_substr(const char* st, size_t idx, size_t len=std::string::npos)
{
    return std::string(st + idx, len != std::string::npos ? len : strlen(st) - idx);
}

template <typename T, typename S1, typename S2>
inline std::string str_replace(T&& s, const S1 &a, const S2 &b)
{
    std::string r(std::forward<T>(s));
    std::string::size_type n = str_len(a);
    std::string::size_type bn = str_len(b);
    for (std::string::size_type i = r.find(a); i != r.npos; i = r.find(a, i + bn))
    {
        r.replace(i, n, b);
    }
    return r;
}

template <typename T>
inline std::string str_indent(T&& s, int amount)
{
    std::string istr = '\n' + std::string(amount, ' ');
    return std::string(amount, ' ') + str_replace(std::forward<T>(s), "\n", istr.c_str());
}

std::string str_align(const std::string& input, char token);

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

inline bool str_startswith(const char* str, const char* prefix)
{
    while (*prefix)
    {
        if (*str != *prefix)    // will catch *str == NULL
            return false;
        str++;
        prefix++;
    }
    return true;
}

inline const char* str_tocstr(const char *v) { return v; }
inline const char* str_tocstr(const std::string &v) { return v.c_str(); }
inline const char* str_tocstr(const lstring &v) { return v.c_str(); }

template <typename S1, typename S2>
inline bool str_startswith(const S1& s1, const S2& s2)
{
    return str_startswith(str_tocstr(s1), str_tocstr(s2));
}

inline bool str_endswith(const char* str_, const char* prefix_)
{
    std::string str = str_;
    std::string pref = prefix_;
    return str.size() >= pref.size() && str.substr(str.size()-pref.size()) == pref;
}

std::string str_vformat(const char *format, va_list vl) __printflike(1, 0);
std::string str_format(const char *format, ...)  __printflike(1, 2);

inline std::string str_add_line_numbers(const char* s, int start=1)
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

template <typename T, typename U>
inline std::vector<std::string> str_split(const T &s, const U &delim)
{
    std::vector<std::string> elems;
    size_t i = 0;
    const size_t dlen = str_len(delim);
    do {
        size_t n = str_find(s, delim, i);
        if (n == std::string::npos) {
            elems.push_back(str_substr(s, i));
            return elems;
        }
        elems.push_back(str_substr(s, i, n-i));
        i = n + dlen;
    }
    while (i != std::string::npos);
    
    return elems;
}

inline std::string str_tostr(float a) { return str_format("%f", a); }
inline std::string str_tostr(int a)   { return str_format("%d", a); }
inline std::string str_tostr(uint a)  { return a < 0xfffff ? str_format("%d", a) : str_format("%#x", a); }
inline std::string str_tostr(char a)  { return str_format((a >= 0 && std::isprint(a)) ? "%c" : "\\%x", a); }
inline std::string str_tostr(const char *a) { return a ? a : ""; }
inline std::string str_tostr(lstring a) { return a.str(); }
inline const std::string &str_tostr(const std::string &a) { return a; }
inline std::string str_tostr(std::string &&a) { return std::move(a); }

template <class A>
std::string str_join( const std::string &sep, const A &cont)
{
    std::string result;
    for (auto &it : cont)
    {
        if (!result.empty())
            result.append(sep);
        result.append(str_tostr(it));
    }
    return result;
}

template <class A>
std::string str_join( const std::string &sep, const std::string &lsep, const A &cont)
{
    std::string result;
    for (typename A::const_iterator it=cont.begin(), end=cont.end(); it!=end; ++it)
    {
        if (!result.empty())
            result.append((it+1) == end ? lsep : sep);
        result.append(str_tostr(*it));
    }
    return result;
}

template <class A>
std::string str_join(const std::string &sep, const A &begin, const A &end)
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
std::string str_join_keys(const std::string &sep, const A &begin, const A &end)
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

std::string str_word_wrap(std::string str,
                          size_t width = 70,
                          const char* newline="\n",
                          const char* wrap=" ");

inline std::string str_word_rewrap(const std::string &str, size_t width)
{
    return str_word_wrap(str_replace(str, "\n", " "), width);
}


std::string str_capitalize(const char* str);

inline std::string str_capitalize(const std::string &s)
{
    return str_capitalize(s.c_str());
}


template <typename T>
std::string str_toupper(const T &str)
{
    std::string s = str_tostr(str);
    for (uint i=0; i<s.size(); i++)
        s[i] = std::toupper(s[i]);
    return s;
}

template <typename T>
std::string str_tolower(const T &str)
{
    std::string s = str_tostr(str);
    for (uint i=0; i<s.size(); i++)
        s[i] = std::tolower(s[i]);
    return s;
}

inline bool str_contains(const std::string& str, const std::string& substr)
{
    return str.find(substr) != std::string::npos;
}

inline bool str_contains(const std::string& str, const char *substr)
{
    return str.find(substr) != std::string::npos;
}

inline bool str_contains(const char* str, const char* substr)
{
    return strstr(str, substr) != NULL;
}

inline bool str_contains(const char* str, char chr)
{
    return strchr(str, chr) != NULL;
}

inline bool str_contains(const std::string &str, char chr)
{
    return str.find(chr) != std::string::npos;
}

long chr_unshift(long chr);

std::string str_demangle(const char* str);

inline bool issym(const char v)
{
    return v != '\0' && (isalpha(v) || v == '_');
}

inline std::string str_get_extension(const std::string &str)
{
    size_t pt = str.rfind(".");
    if (pt == std::string::npos)
        return "";
    else
        return str.substr(pt+1);
}

inline std::string str_no_extension(const std::string &str)
{
    size_t pt = str.rfind(".");
    if (pt == std::string::npos)
        return str;
    else
        return str.substr(0, pt);
}

std::string str_basename(const std::string &str);
std::string str_dirname(const std::string &str);
std::string str_path_standardize(const std::string &str);

template <typename T, typename S>
inline std::string str_path_join(T &&a, const S &b)
{
    std::string r(std::forward<T>(a));
    if (r.back() == '/')
        r.pop_back();
    if (str_len(b)) {
        if (b[0] != '/')
            r += '/';
        r += b;
    }
    return r;
}

template <typename T, typename S1, typename S2>
inline std::string str_path_join(T &&a, const S1 &b, const S2 &c)
{
    return str_path_join(str_path_join(std::forward<T>(a), b), c);
}


template <typename Fun>
inline std::string str_interpolate_variables(const std::string &str_, const Fun& fun)
{
    std::string str = str_;
    for (size_t idx = str.find("$"); idx != std::string::npos; idx=str.find("$", idx))
    {
        int len=1;
        for (; idx+len<str.size() && issym(str[idx+len]); len++);
        const std::string fname = str.substr(idx+1, len-1);
        str.replace(idx, len, fun(fname));
    }
    return str;
}


std::string str_urlencode(const std::string &value);
std::string str_urldecode(const std::string &value);

std::string str_time_format(float seconds);

std::string str_tohex(const char* digest, int size);

#endif
