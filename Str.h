
//
// Str.h - string utilities
//
// API style is largely copied from python
// most functions work on cstrings or std::string
// 
// lstring is intended to be used as a "symbol" rather than a byte array

// Copyright (c) 2013-2016 Arthur Danskin
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
#include <utility>
#include <cctype>

namespace std {
    // extern template class basic_string<char>;
    // extern template class vector<string>;
    // extern template class unordered_set<string>;
    // extern template class lock_guard<mutex>;
    // extern template class lock_guard<recursive_mutex>;
}

typedef unsigned long long uint64;
typedef std::basic_string<uint32_t> ustring;

#define UNKNOWN_UNICODE 0xFFFD

// convert a single ucs2 character to a utf8 string
std::string utf8_encode(int ucs2);

// utf8 multi-byte sequences start with something that has two or more 11s,
// continuation bytes all have 10 in highest position
// ascii always has 0 in highest pos
inline bool utf8_iscont(char byte) { return (byte&0xc0) == 0x80; }

// return a single ucs2 character from a utf8 stream
uint utf8_getch(const char **src, size_t *srclen);
uint utf8_getch(const std::string &str);

// return byte index after advancing LEN characters in STR, starting at START
int utf8_advance(const std::string& str, int start, int len=0);

// return substring of UTF8 starting at byte START of LEN characters (not bytes)
std::string utf8_substr(const std::string &utf8, int start, int len=-1);

// return STR with LEN characters erased starting from byte START
std::string utf8_erase(const std::string &str, int start, int len=-1);

// return length of substring in _characters_
// POS and LEN are byte indexes
size_t utf8_len(const std::string &str, size_t pos=0, size_t len=~0);

// return length of substring in _character width_
// (中文/한국어/日本語 characters are double width)
// POS and LEN are byte indexes
size_t utf8_width(const std::string &str, size_t pos=0, size_t len=~0);

size_t str_hash(const char* str);
inline bool str_equals(const char* a, const char* b);

// lexicon-ized string - basically a symbol
// very fast to copy around, compare
// can convert to const char* or std::string
struct lstring {
private:
    struct Lexicon
    {
        struct cstring_hash
        {
            size_t operator()(const char *p) const
            {
                return str_hash(p);
            }
        };

        struct cstring_eq {
            bool operator()(const char* a, const char *b) const
            {
                return str_equals(a, b);
            }
        };
        
        std::mutex                                                mutex;
        std::unordered_set<const char*, cstring_hash, cstring_eq> strings;

        static const char* intern(const char* ptr);
        static Lexicon &instance();
    };

    const char* m_ptr = NULL;

public:
    lstring() NOEXCEPT {}
    lstring(const std::string& str) NOEXCEPT : m_ptr(Lexicon::intern(str.c_str())) { }
    lstring(const char* str) NOEXCEPT : m_ptr(str ? Lexicon::intern(str) : NULL) { }
    lstring(const lstring &o) NOEXCEPT : m_ptr(o.m_ptr) {}

    // trick ctags into continue to read this file from here (why does it do that!?!?!?)    
#if 1 && 0
#else
    std::string str()   const { return m_ptr ? std::string(m_ptr) : std::string(); }
    const char* c_str() const { return m_ptr; }
    const char* c_str_nonnull() const { return m_ptr ? m_ptr : ""; }
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

    static void lexicon_destroy();
    static size_t lexicon_bytes();
    static size_t lexicon_size() { return Lexicon::instance().strings.size(); }
    
#endif
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
inline size_t str_len(const std::wstring& str) { return str.size(); }
inline size_t str_len(char chr)               { return 1; }
inline size_t str_len(lstring str)            { return str_len(str.c_str()); }
template <size_t N>
size_t str_len(const char (&buf)[N])          { return strnlen(buf, N); }

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
    if (pos >= strlen(s))
        return std::string::npos;
    const char *p = strstr(s+pos, v);
    return p ? (size_t) (p - s) : std::string::npos;
}

template <typename T>
inline size_t str_rfind(const std::string &s, const T& v, size_t pos=std::string::npos)
{
    return s.rfind(v, pos);
}

inline std::string str_substr(const std::string &st, size_t idx, size_t len=std::string::npos)
{
    return st.substr(idx, len);
}

inline std::string str_substr(const char* st, size_t idx, size_t len=std::string::npos)
{
    return std::string(st + idx, len != std::string::npos ? len : strlen(st) - idx);
}

std::string str_align(const std::string& input, char token=':');

template <typename T, typename Fun>
inline std::string str_strip(T &&s, Fun func)
{
    const int sz = str_len(s);
    int st=0;
    while (st < sz && s[st] < 128 && func(s[st])) {
        st++;
    }
    int ed=sz-1;
    while (ed >= st && s[st] < 128 && func(s[ed])) {
        ed--;
    }
    return str_substr(s, st, ed-st+1);
}

inline bool str_isspace(int c) { return 0 < c && c < 128 && isspace(c); }
inline bool str_isalnum(int c) { return 0 < c && c < 128 && isalnum(c); }
inline bool str_isalpha(int c) { return 0 < c && c < 128 && isalpha(c); }
inline bool str_isdigit(int c) { return 0 < c && c < 128 && isdigit(c); }
inline bool str_ispunct(int c) { return 0 < c && c < 128 && ispunct(c); }

template <typename T>
inline std::string str_strip(T &&s)
{
    return str_strip(std::forward<T>(s), str_isspace);
}

template <typename T>
inline std::string str_strip(const std::string &s)
{
    return str_strip(s, str_isspace);
}

inline std::string str_chomp(const std::string &s)
{
    int ed=(int) s.size()-1;
    while (ed < s.size() && str_isspace(s[ed])) {
        ed--;
    }
    return s.substr(0, ed+1);
}

inline std::string str_chomp(std::string &&s)
{
    int ed=(int) s.size()-1;
    while (ed < s.size() && str_isspace(s[ed])) {
        ed--;
    }
    s.resize(ed+1);
    return std::move(s);
}

std::string str_vformat(const char *format, va_list vl) __printflike(1, 0);
std::string str_format(const char *format, ...)  __printflike(1, 2);

void str_append_vformat(std::string &str, const char *format, va_list vl) __printflike(2, 0);
void str_append_format(std::string &str, const char *format, ...)  __printflike(2, 3);

inline std::string str_tostr(float a) { return str_format("%f", a); }
inline std::string str_tostr(int a)   { return str_format("%d", a); }
inline std::string str_tostr(uint a)  { return a < 0xfffff ? str_format("%d", a) : str_format("%#x", a); }
inline std::string str_tostr(uint64 a) { return a < 0xfffff ? str_format("%lld", a) : str_format("%#llx", a); }
inline std::string str_tostr(unsigned long a) { return str_tostr((uint)a); }
inline std::string str_tostr(char a)  { return str_format((a >= 0 && std::isprint(a)) ? "%c" : "\\%x", a); }
inline std::string str_tostr(const char *a) { return a ? a : ""; }
inline std::string str_tostr(lstring a) { return a.str(); }
inline const std::string &str_tostr(const std::string &a) { return a; }
inline std::string str_tostr(std::string &&a) { return std::move(a); }
inline const std::wstring &str_tostr(const std::wstring &a) { return a; }
inline std::wstring str_tostr(std::wstring &&a) { return std::move(a); }

template <typename K, typename V>
inline std::string str_tostr(std::pair<K, V> &p) {
    return str_tostr(p.first) + "=" + str_tostr(p.second);
}

inline bool str_equals(const char* a, const char* b)
{
    return (!a && !b) || (a && b && strcmp(a, b) == 0);
}

inline bool str_equals(const std::string &a, const std::string &b)
{
    return a == b;
}

inline bool str_startswith(const char* str, const char* prefix)
{
    if (!str || !prefix)
        return false;
    while (*prefix)
    {
        if (*str != *prefix)    // will catch *str == NULL
            return false;
        str++;
        prefix++;
    }
    return true;
}

inline bool str_startswith(const char* str, char prefix)
{
    return str && *str == prefix;
}

#define DEF_CSTR2(N)                                                    \
    template <typename S1, typename S2>                                 \
    inline bool N(const S1& s1, const S2& s2) { return N(s1.c_str(), s2.c_str()); } \
    template <typename S2>                                              \
    inline bool N(const char *s1, const S2& s2) { return N(s1, s2.c_str()); } \
    template <typename S1>                                              \
    inline bool N(const S1& s1, const char *s2) { return N(s1.c_str(), s2); }

DEF_CSTR2(str_startswith);

template <typename T, typename S1, typename S2>
inline std::string str_replace(const T &s, const S1 &a, const S2 &b)
{
    std::string r;
    const std::string::size_type sn = str_len(s);
    const std::string::size_type n = str_len(a);
    r.reserve(sn);
    for (std::string::size_type i = 0; i < sn; )
    {
        if (str_startswith((const char*) (&s[i]), a)) {
            r += b;
            i += n;
        } else {
            r += s[i];
            i++;
        }
    }
    return r;
}

template <typename T>
inline std::string str_indent(T&& s, int amount)
{
    std::string istr = '\n' + std::string(amount, ' ');
    return std::string(amount, ' ') + str_replace(std::forward<T>(s), "\n", istr.c_str());
}

inline bool str_endswith(const char* str, const char* pfx)
{
    const size_t strlen = str_len(str);
    const size_t pfxlen = str_len(pfx);
    return strlen >= pfxlen && str_equals(str+(strlen-pfxlen), pfx);
}

DEF_CSTR2(str_endswith);

std::string str_add_line_numbers(const char* s, int start=1);

template <typename T, typename U>
inline std::vector<std::string> str_split(const U &delim, const T &s)
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

std::vector<std::string> str_split_quoted(char token, const std::string& line);

template <class S, class S1, class A>
std::string str_join(const S &sep, const S1 &lsep, const A &cont)
{
    std::string result;
    for (typename A::const_iterator it=cont.begin(), end=cont.end(); it!=end; ++it)
    {
        if (!result.empty())
            ((it+1) == end) ? result += lsep : result += sep;
        result.append(str_tostr(*it));
    }
    return result;
}

template <class S, class A>
std::string str_join(const S &sep, const A &begin, const A &end)
{
    std::string result;
    for (A it=begin; it!=end; ++it)
    {
        if (!result.empty())
            result += sep;
        result.append(str_tostr(*it));
    }
    return result;
}

template <class S, class A>
std::string str_join(const S &sep, A &&cont)
{
    std::string result;
    for (auto &&it : cont)
    {
        if (!result.empty())
            result += sep;
        result.append(str_tostr(it));
    }
    return result;
}

template <class S, class A>
std::string str_join_keys(const S &sep, const A &begin, const A &end)
{
    std::string result;
    for (A it=begin; it!=end; ++it)
    {
        if (!result.empty())
            result += sep;
        result.append(str_tostr(it->first));
    }
    return result;
}

template <class S, class A>
std::string str_join_keys(const S &sep, const A &vec)
{
    return str_join_keys(sep, std::begin(vec), std::end(vec));
}

struct str_wrap_options_t {
    const char *newline = "\n";
    const char *wrap    = NULL;
    int         width   = 70;
    bool        rewrap  = false;

    str_wrap_options_t(int w) : width(w) {}
    str_wrap_options_t() {}
};

std::string str_word_wrap(const std::string &str, const str_wrap_options_t &ops=str_wrap_options_t());

std::string str_capitalize(std::string s);
std::string str_capitalize_first(std::string s);

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

template <typename C, typename T>
bool str_contains(const std::basic_string<C>& str, const T &substr)
{
    return str.find(substr) != std::basic_string<C>::npos;
}

inline bool str_contains(const char* str, lstring b)
{
    return str && strstr(str, b.c_str()) != NULL;
}

inline bool str_contains(const char* str, const char *b)
{
    return str && strstr(str, b) != NULL;
}

template <typename T>
inline bool str_contains(const char* str, const std::basic_string<T> &b)
{
    return str && strstr(str, b.c_str()) != NULL;
}

inline bool str_contains(const char* str, char chr)
{
    return str && strchr(str, chr) != NULL;
}

template <typename T>
bool str_contains(lstring str, const T& substr)
{
    return str_contains(str.c_str(), substr);
}

template <typename T>
std::string str_concat(T&& first)
{
    return str_tostr(std::forward<T>(first));
}

template <typename T, typename... Args>
std::string str_concat(T&& first, Args... args)
{
    return str_tostr(std::forward<T>(first)) + str_concat(args...);
}

long chr_unshift(long chr);

std::string str_demangle(std::string str);
std::string str_demangle(const char* str);

inline bool str_issym(int c)
{
    return c == '_' || str_isalpha(c);
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
std::string str_path_standardize(std::string str);
std::wstring str_w32path_standardize(const std::wstring &inpt);

template <typename T, typename S>
inline std::basic_string<T> str_path_join_(std::basic_string<T> r, const S &b, T sep=T('/'))
{
    const size_t bsz = str_len(b);
    if (!bsz)
        return r;
    if (r.empty() || b[0] == sep || (bsz > 1 && b[1] == ':'))
        return b;
    if (!strchr("/\\", r.back()))
        r += sep;
    r += b;
    return r;
}

template <typename S>
inline std::string str_path_join(std::string r, const S &b)
{
    return str_path_join_(std::move(r), b, '/');
}

template <typename T>
inline std::string str_path_join(T s)
{
    return str_tostr(s);
}

template <typename T, typename S, typename R, typename... Args>
std::string str_path_join(T &&first, S &&second, R &&third, Args... args)
{
    return str_path_join(str_path_join(std::forward<T>(first), std::forward<S>(second)),
                         str_path_join(std::forward<R>(third), args...));
}

inline std::wstring str_win32path_join(const std::wstring &a, const std::wstring &b)
{
    return str_path_join_(a, b, L'\\');
}

// remove characters not allowed in file names from path (including / and \!)
std::string str_path_sanitize(std::string path);

template <typename Fun>
inline std::string str_interpolate_variables(std::string str, const Fun& fun)
{
    for (size_t idx = str.find("$"); idx != std::string::npos; idx=str.find("$", idx))
    {
        int len=1;
        for (; idx+len<str.size() && str_issym(str[idx+len]); len++);
        std::string fname = str.substr(idx+1, len-1);
        str.replace(idx, len, fun(fname));
    }
    return str;
}

inline std::string str_untabify_fixnewlines(const std::string &str)
{
    std::string r;
    r.reserve(str.size());
    for (char x : str)
    {
        switch (x) {
        case '\r': break;
        case '\t': r += "    "; break;
        default:   r += x; break;
        }
    }
    return r;
}

std::string str_urlencode(const std::string &value);
std::string str_urldecode(const std::string &value);

#define STR_TIMESTAMP_FORMAT "%Y%m%d_%I.%M.%S.%p"
std::string str_time_format(float seconds);
std::string str_time_format_long(float seconds);
std::string str_reltime_format(float seconds);
std::string str_timestamp();
std::string str_strftime(const char* fmt, const std::tm *time);
std::string str_strftime(const char* fmt);
bool str_strptime(const char* str, const char* fmt, std::tm *tm);

// return one, two, three, etc
std::string str_numeral_format(int num);

// apply adjective, using word order based on language
std::string lang_concat_adj(const std::string &adj, const char* noun);

// language sensitive plural
std::string lang_plural(const std::string &noun);

const char* lang_space();
void lang_append_space(std::string &str);

// join two strings with a colon
std::string lang_colon(const std::string &a, const std::string &b);
std::string lang_colon(const char *a, const std::string &b);
std::string lang_colon(const std::string &a, const char* b);
std::string lang_colon(const char *a, const char* b);

// return data byte size in MB, KB, etc
std::string str_bytes_format(int bytes);
#define FMT_BYTES(X) (str_bytes_format(X).c_str())

std::string str_tohex(const char* digest, int size);

std::string str_b64encode(const char* digest, int size);
std::string str_b64decode(const char* digest, int size);

inline std::string str_b64encode(const std::string &str) { return str_b64encode(str.c_str(), str.size()); }
inline std::string str_b64decode(const std::string &str) { return str_b64decode(str.c_str(), str.size()); }

// cpuid cpu brand string
std::string str_cpuid();

// append binary data to a string
template <typename T>
int str_append_bytes(std::string& str, const T &bytes)
{
    str.append((const char*)&bytes, sizeof(T));
    return sizeof(T);
}

template <typename T>
int str_write_bytes(std::string& str, int idx, const T &bytes)
{
    if (size_t(idx + sizeof(T)) > str.size())
        return 0;
    memcpy(&str[idx], (void*)&bytes, sizeof(T));
    return sizeof(T);
}

inline int str_write_bytes(std::string& str, int idx, const std::string &bytes)
{
    if (size_t(idx + bytes.size()) > str.size())
        return 0;
    memcpy(&str[idx], &bytes[0], bytes.size());
    return bytes.size();
}

// read binary data from string at index
// return number of bytes read
template <typename T>
int str_read_bytes(std::string& str, int idx, T *bytes)
{
    if (size_t(idx + sizeof(T)) > str.size())
        return 0;
    memcpy((void*)bytes, &str[idx], sizeof(T));
    return sizeof(T);
}

template <typename T>
int str_read_bytes(const char* str, int idx, T *bytes)
{
    memcpy((void*)bytes, &str[idx], sizeof(T));
    return sizeof(T);
}

bool str_runtests();

#endif
