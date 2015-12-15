
//
// Str.cpp - string / unicode utilities
// 

// Some routines borrowed from SDL_ttf
// everything else
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
#include <cstdint>

namespace std {
    // template class basic_string<char>;
    // template class vector<string>;
    // template class unordered_set<string>;
    // template class lock_guard<mutex>;
    // template class lock_guard<recursive_mutex>;
}

typedef std::uint8_t Uint8;
typedef std::uint16_t Uint16;
typedef std::uint32_t Uint32;

// Copyright (c) 2008-2009 Bjoern Hoehrmann <bjoern@hoehrmann.de>
// See http://bjoern.hoehrmann.de/utf-8/decoder/dfa/ for details.

#define UTF8_ACCEPT 0
#define UTF8_REJECT 1

static const uint8_t utf8d[] = {
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 00..1f
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 20..3f
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 40..5f
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 60..7f
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9, // 80..9f
    7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7, // a0..bf
    8,8,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2, // c0..df
    0xa,0x3,0x3,0x3,0x3,0x3,0x3,0x3,0x3,0x3,0x3,0x3,0x3,0x4,0x3,0x3, // e0..ef
    0xb,0x6,0x6,0x6,0x5,0x8,0x8,0x8,0x8,0x8,0x8,0x8,0x8,0x8,0x8,0x8, // f0..ff
    0x0,0x1,0x2,0x3,0x5,0x8,0x7,0x1,0x1,0x1,0x4,0x6,0x1,0x1,0x1,0x1, // s0..s0
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,1,1,1,1,1,0,1,0,1,1,1,1,1,1, // s1..s2
    1,2,1,1,1,1,1,2,1,2,1,1,1,1,1,1,1,1,1,1,1,1,1,2,1,1,1,1,1,1,1,1, // s3..s4
    1,2,1,1,1,1,1,1,1,2,1,1,1,1,1,1,1,1,1,1,1,1,1,3,1,3,1,1,1,1,1,1, // s5..s6
    1,3,1,1,1,1,1,3,1,3,1,1,1,1,1,1,1,3,1,1,1,1,1,1,1,1,1,1,1,1,1,1, // s7..s8
};

static uint32_t decode(uint32_t* state, uint32_t* codep, uint32_t byte)
{
    uint32_t type = utf8d[byte];

    *codep = (*state != UTF8_ACCEPT) ?
             (byte & 0x3fu) | (*codep << 6) :
             (0xff >> type) & (byte);

    *state = utf8d[256 + *state*16 + type];
    return *state;
}
// end Copyright(c) 2008-2009 Bjoern Hoehrmann <bjoern@hoehrmann.de>

// utf8 -> utf32
static ustring utf8_decode(const string &utf8)
{
    ustring ret;
    uint32_t state = 0;
    uint32_t codepoint = 0;
    for (int i=0; i<utf8.size(); i++)
    {
        if (decode(&state, &codepoint, (unsigned char)utf8[i]))
            continue;
        ret.push_back(codepoint);
    }
    return ret;
}


// modified from SDL2 SDL_ttf.c
static void UCS2_to_UTF8(const Uint16 *src, Uint8 *dst)
{
    while (*src) {
        Uint16 ch = *(Uint16*)src++;
        if (ch <= 0x7F) {
            *dst++ = (Uint8) ch;
        } else if (ch <= 0x7FF) {
            *dst++ = 0xC0 | (Uint8) ((ch >> 6) & 0x1F);
            *dst++ = 0x80 | (Uint8) (ch & 0x3F);
        } else {
            *dst++ = 0xE0 | (Uint8) ((ch >> 12) & 0x0F);
            *dst++ = 0x80 | (Uint8) ((ch >> 6) & 0x3F);
            *dst++ = 0x80 | (Uint8) (ch & 0x3F);
        }
    }
    *dst = '\0';
}

// convert a single ucs2 character to a utf8 string
string utf8_encode(int ucs2)
{
    char outp[5] = {};
    const Uint16 chrs[] = { (Uint16)ucs2, 0 };
    UCS2_to_UTF8(chrs, (Uint8*)outp);
    return outp;
}

static void utf8_encode_append(string &str, int ucs2)
{
    char outp[5] = {};
    const Uint16 chrs[] = { (Uint16)ucs2, 0 };
    UCS2_to_UTF8(chrs, (Uint8*)outp);
    str += outp;
}


// modified from SDL2 SDL_ttf.c
// return a single ucs2 character from a utf8 stream
Uint32 utf8_getch(const char **src, size_t *srclen)
{
    const Uint8 *p = *(const Uint8**)src;
    size_t left = 0;
    bool overlong = false;
    bool underflow = false;
    Uint32 ch = UNKNOWN_UNICODE;

    if (*srclen == 0) {
        return UNKNOWN_UNICODE;
    }
    if (p[0] >= 0xFC) {
        if ((p[0] & 0xFE) == 0xFC) {
            if (p[0] == 0xFC && (p[1] & 0xFC) == 0x80) {
                overlong = true;
            }
            ch = (Uint32) (p[0] & 0x01);
            left = 5;
        }
    } else if (p[0] >= 0xF8) {
        if ((p[0] & 0xFC) == 0xF8) {
            if (p[0] == 0xF8 && (p[1] & 0xF8) == 0x80) {
                overlong = true;
            }
            ch = (Uint32) (p[0] & 0x03);
            left = 4;
        }
    } else if (p[0] >= 0xF0) {
        if ((p[0] & 0xF8) == 0xF0) {
            if (p[0] == 0xF0 && (p[1] & 0xF0) == 0x80) {
                overlong = true;
            }
            ch = (Uint32) (p[0] & 0x07);
            left = 3;
        }
    } else if (p[0] >= 0xE0) {
        if ((p[0] & 0xF0) == 0xE0) {
            if (p[0] == 0xE0 && (p[1] & 0xE0) == 0x80) {
                overlong = true;
            }
            ch = (Uint32) (p[0] & 0x0F);
            left = 2;
        }
    } else if (p[0] >= 0xC0) {
        if ((p[0] & 0xE0) == 0xC0) {
            if ((p[0] & 0xDE) == 0xC0) {
                overlong = true;
            }
            ch = (Uint32) (p[0] & 0x1F);
            left = 1;
        }
    } else {
        if ((p[0] & 0x80) == 0x00) {
            ch = (Uint32) p[0];
        }
    }
    ++*src;
    --*srclen;
    while (left > 0 && *srclen > 0) {
        ++p;
        if ((p[0] & 0xC0) != 0x80) {
            ch = UNKNOWN_UNICODE;
            break;
        }
        ch <<= 6;
        ch |= (p[0] & 0x3F);
        ++*src;
        --*srclen;
        --left;
    }
    if (left > 0) {
        underflow = true;
    }
    /* Technically overlong sequences are invalid and should not be interpreted.
       However, it doesn't cause a security risk here and I don't see any harm in
       displaying them. The application is responsible for any other side effects
       of allowing overlong sequences (e.g. string compares failing, etc.)
       See bug 1931 for sample input that triggers this.
    */
    UNUSED(overlong);
    /*if (overlong) return UNKNOWN_UNICODE;*/
    if (underflow ||
        (ch >= 0xD800 && ch <= 0xDFFF) ||
        (ch == 0xFFFE || ch == 0xFFFF) || ch > 0x10FFFF) {
        ch = UNKNOWN_UNICODE;
    }
    return ch;
}

uint utf8_getch(const string &str)
{
    const char *ptr = &str[0];
    size_t len = str.size();
    return utf8_getch(&ptr, &len);
}

int utf8_advance(const string& str, int start, int len)
{
    while (utf8_iscont(str[start]) && start > 0)
        start--;
    if (len == 0)
        return start;
    else if (len < 0)
        return str.size();
    int end = start;
    for (; end<str.size(); end++)
    {
        if (!utf8_iscont(str[end])) {
            if (len == 0)
                break;
            len--;
        }
    }
    return end;
}

// return substring of UTF8 starting at byte START of LEN characters (not bytes)
string utf8_substr(const string &utf8, int start, int len)
{
    if (len == 0)
        return "";
    start = utf8_advance(utf8, start);
    return utf8.substr(start, utf8_advance(utf8, start, len) - start);
}

// return STR with LEN characters erased starting from byte START
string utf8_erase(const string &str, int start, int len)
{
    if (len == 0)
        return str;
    string ret = str;
    start = utf8_advance(str, start);
    ret.erase(start, utf8_advance(str, start, len) - start);
    return ret;
}

// return length of substring in _characters_
// POS and LEN are byte indexes
size_t utf8_len(const string &str, size_t pos, size_t len)
{
    while (utf8_iscont(str[pos]) && pos > 0)
        pos--;
    const size_t lenc = min(str.size() - pos, len);
    size_t chars = 0;
    for (int i=pos; i<lenc; i++)
        if (!utf8_iscont(str[i]))
            chars++;
    ASSERT(chars <= lenc);
    return chars;
}

static int utf8_charwidth(const char* utf8)
{
    size_t size = 4;
    const uint chr = utf8_getch(&utf8, &size);
    return ((0x1100 <= chr && chr <= 0x11FF) || // hangul
            (0xAC00 <= chr && chr <= 0xD7FF) || // hangul extended
            (0x2E00 <= chr && chr <= 0x9FFF) || // hirigana, katakana, kanji, etc.
            (0xFF00 < chr && chr <= 0xFF60))    // fullwidth latin
            ? 2 : 1;
}

// return length of substring in _character width_
// (中文/한국어/日本語 characters are double width)
// POS and LEN are byte indexes
size_t utf8_width(const string &str, size_t pos, size_t len)
{
    while (utf8_iscont(str[pos]) && pos > 0)
        pos--;
    const size_t end = pos + min(str.size() - pos, len);
    size_t chars = 0;
    for (int i = pos; i < end; i++)
    {
        if (utf8_iscont(str[i]))
            continue;
        // quake3 style color escapes
        if ((i+1 < end && str[i] == '^' && isdigit(str[i+1])) ||
            (i > 0 && str[i-1] == '^' && isdigit(str[i])))
            continue;
        chars += utf8_charwidth(&str[i]);
    }
    return chars;
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

static bool is_punct(uint32_t it)
{
    switch (it)
    {
    case '|': case '_': case '.': case '?': case '!':
    case 0x3001: case 0x3002: case 0xff1f: // 、。？
        return true;
    }
    return false;
}

std::string str_word_wrap(const std::string &utf8, const str_wrap_options_t &ops)
{
    const size_t nlsize = strlen(ops.newline)-1;

    const ustring str = utf8_decode(utf8);
    std::string ret;
    int line_length = 0;
    std::string word;
    for (int i=0; i<=str.size(); i++)
    {
        uint32_t chr = (i == str.size()) ? '\0' : str[i];
        if (chr == ' ' || is_punct(chr) || chr == '\n' || chr == '\0')
        {
            const int word_len = utf8_width(word);
            if (line_length + word_len >= ops.width && line_length > nlsize)
            {
                while (ret.size() && ret.back() == ' ')
                    ret.pop_back();
                ret += ops.newline;
                line_length = nlsize;
            }
            ret += word;
            // replace newline with space if rewrapping
            // FIXME japanese doesn't have spaces
            if (ops.rewrap && chr == '\n' && 0 < i && i < str.size()-1 &&
                str[i-1] != '\n' && str[i+1] != '\n')
            {
                chr = ' ';
            }
            utf8_encode_append(ret, chr);
            if (chr == '\n')
                line_length = 0;
            else
                line_length += word_len + 1;
            word = string();
        }
        else
        {
            utf8_encode_append(word, chr);
        }
    }
    if (ret.size() && ret.back() == '\0')
        ret.pop_back();
    return ret;
}

std::string str_align(const std::string& input, char token)
{
    int alignColumn = 0;
    int lineStart = 0;
    int tokensInLine = 0;
    for (int i=0; i<input.size(); i++) {
        if (input[i] == '\n') {
            lineStart = i+1;
            tokensInLine = 0;
        } else if (input[i] == token &&
                   input.size() != i+1 && input[i+1] != '\n' &&
                   tokensInLine == 0) {
            alignColumn = max(alignColumn, (int)utf8_width(input, lineStart, i - lineStart + 1));
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
            lineStart = i+1;
            tokensInLine = 0;
        } else if (input[i] == token && tokensInLine == 0) {
            const int spaces = alignColumn - utf8_width(input, lineStart, (i - lineStart));
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

std::string str_time_format_long(float seconds)
{
    seconds = max(epsilon, seconds);
    std::string ret;
    const int minutes = floor(seconds / 60.f);
    const int hours   = floor(seconds / 3600.f);
    if (!minutes)
        return str_format("%d seconds", floor_int(seconds));
    else if (!hours)
        return str_format("%d minutes, %d seconds", minutes, modulo((int)floor(seconds), 60));
    else
        return str_format("%d hours, %d minutes, %d seconds", hours, modulo(minutes, 60),
                          modulo((int)floor(seconds), 60));
}

std::string str_reltime_format(float seconds)
{
    if (seconds > 0)
        return "in " + str_time_format(seconds);
    else
        return str_time_format(-seconds) + " ago";
}

std::string str_timestamp()
{
    std::chrono::time_point<std::chrono::system_clock> start = std::chrono::system_clock::now();
    const std::time_t cstart = std::chrono::system_clock::to_time_t(start);
    char mbstr[100];
    std::strftime(mbstr, sizeof(mbstr), "%Y%m%d_%I.%M.%S.%p", std::localtime(&cstart));
    return mbstr;    
}


std::string str_numeral_format(int num)
{
    if (!str_equals(OLG_GetLanguage(), "en"))
        return str_format("%d", num);
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


std::string lang_concat_adj(const string &adj, const string &noun)
{
    if (adj.empty())
        return noun;
    const char* lang = OLG_GetLanguage();
    if (str_startswith(lang, "pt") ||
        str_startswith(lang, "es"))
    {
        int suffix = noun.size();
        while (suffix > 0 && (str_isspace(noun[suffix-1]) || str_ispunct(noun[suffix-1])))
            suffix--;
        return noun.substr(0, suffix) + " " + str_strip(adj) + noun.substr(suffix);
    }
    else
    {
        return str_strip(adj) + " " + noun;
    }
}

std::string lang_plural(const string &noun)
{
    const char* lang = OLG_GetLanguage();
    if (str_startswith(lang, "en"))
    {
        return noun + "s";
    }
    // just use singular
    return noun;
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

string str_path_sanitize(string path)
{
    const char* kReserved = "<>:\"/\\|?*";

    for (size_t i=path.find_first_of(kReserved); i != std::string::npos; i = path.find_first_of(kReserved, i))
    {
        path.erase(i, 1);
    }
    return str_strip(path);
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

std::string str_capitalize(std::string s)
{
    s[0] = toupper(s[0]);
    for (int i=1; i<s.size(); i++) {
        if (str_contains(" \n\t_-", s[i-1]))
            s[i] = toupper(s[i]);
        // don't lowercase, might be camelCase or something
    }
        
    return s;
}

std::string str_capitalize_first(std::string s)
{
    s[0] = toupper(s[0]);
    return s;
}


#define TEST(A, B) ASSERTF(A == B, "\n%s\n!=\n%s", str_tocstr(A), str_tocstr(B))

bool str_runtests()
{
#if IS_DEVEL
    Report("Beginning String Tests");
    // str_w32path_standardize(L"C:/foo/bar");
    TEST(str_path_standardize("/foo/../.."), "/");
    TEST(str_path_standardize("~/Foo//Bar.lua"), "~/Foo/Bar.lua");
    TEST(str_path_standardize("../../bar.lua"), "../../bar.lua");
    TEST(str_path_standardize("foo/baz/../../bar.lua////"), "bar.lua");
    TEST(str_path_standardize("foo/baz/.."), "foo");
    TEST(str_path_standardize("foo/../"), ".");
    TEST(str_path_standardize("foo/baz/../"), "foo");
    TEST(str_path_standardize("foo/baz/./"), "foo/baz");
    TEST(str_path_standardize("foo//baz"), "foo/baz");
    TEST(str_path_standardize("foo/baz/./.."), "foo");
    TEST(str_path_standardize("./foo"), "foo");
    TEST(str_path_standardize("foo/../.."), "..");
    TEST(str_path_standardize("/../../../../../../.."), "/");
    TEST(str_path_standardize("c:/foo/../.."), "c:\\");
    TEST(str_path_standardize("/foo подпис/공전baz/../"), "/foo подпис");
    TEST(str_path_standardize("foo/../正文如下：///"), "正文如下：");
    TEST(str_path_standardize("C:\\foo\\bar\\..\\"), "C:\\foo");
    TEST(str_path_standardize("C:\\foo\\..\\..\\..\\.."), "C:\\");
    TEST(str_path_standardize("foo\\..\\.."), "..");
    TEST(str_w32path_standardize("C:/foo/bar/../"), "C:\\foo");
    TEST(str_w32path_standardize("foo/bar/../baz"), "foo\\baz");
    TEST(str_path_join("foo", "bar"), "foo/bar");
    TEST(str_path_join("foo/", "bar"), "foo/bar");
    TEST(str_path_join("foo/", "/bar"), "/bar");
    TEST(str_path_join("foo/", (const char*)NULL), "foo/");
    TEST(str_path_join("foo/", ""), "foo/");
    TEST(str_path_join("/home/foo", "bar"), "/home/foo/bar");
    TEST(str_path_join("/home/foo", "bar", "/baz"), "/baz");
    TEST(str_path_join("/foo/bar", "c:/thing"), "c:/thing");
    TEST(str_path_join("c:/foo/bar", "thing"), "c:/foo/bar/thing");
    TEST(str_path_join("c:\\", "thing"), "c:\\thing");
    TEST(str_path_join("/foo/bar", "/thing"), "/thing");
    TEST(str_path_sanitize("/foo/bar"), "foobar");
    TEST(str_path_sanitize("foo \"bar\""), "foo bar");
    TEST(str_path_sanitize("*foo \":<>*bar\"?"), "foo bar");
    TEST(str_dirname("foo/"), ".");
    TEST(str_dirname("/foo/"), "/");
    TEST(str_dirname("foo/baz/bar///"), "foo/baz");
    const char *url = "http://www.anisopteragames.com/forum/viewtopic.php?f=4&t=1136#$@#TW$#^$%*^({}[";
    TEST(str_urldecode(str_urlencode(url)), url);
    TEST(str_find(url, "?f"), str_find(std::string(url), "?f"));
    TEST(str_find(url, "?f"), str_find(url, std::string("?f")));
    TEST(str_rfind(url, "?f"), str_rfind(std::string(url), "?f"));
    TEST(str_rfind(url, "?f"), str_rfind(url, std::string("?f")));
    TEST(str_substr(url, 10, 5), str_substr(std::string(url), 10, 5));
    // TEST(str_numeral_format(57), "fifty seven");
    // TEST(str_numeral_format(-1), "negative one");

    TEST(utf8_width("foo"), 3);
    TEST(utf8_width("NS-윤지"), 7);
    TEST(utf8_width("これか"), 6);
    TEST(utf8_width("чтобы"), 5);
    TEST(str_align("foo: 5\n"
                   "bazbar: 6"),
         "foo:    5\n"
         "bazbar: 6");
    TEST(str_align("Пролетите: 5\n"
                   "на: 6"),
         "Пролетите: 5\n"
         "на:        6");
    TEST(str_align("NS-윤지: 5\n"
                   "これか: 6"),
         "NS-윤지: 5\n"
         "これか:  6");
    TEST(str_word_wrap("чтобы применить дополнительное оружие", 16),
         "чтобы применить\n"
         "дополнительное\n"
         "оружие");
    TEST(str_word_wrap("чтобы применить дополнительное оружие", 22),
         "чтобы применить\n"
         "дополнительное оружие");
    TEST(str_word_wrap("스텔라 - 마리오네트", 16), "스텔라 -\n마리오네트");
    
    str_wrap_options_t ops;
    ops.rewrap = true;
    TEST(str_word_wrap("foo\nbar", ops), "foo bar");
    TEST(str_word_wrap("foo\n\nbar", ops), "foo\n\nbar");
    ops.width = 4;
    TEST(str_word_wrap("foo\nbar", ops), "foo\nbar");
    ops.width = 16;
    TEST(str_word_wrap("で入手します。敵艦を破壊、仲間のスポ", ops),
         "で入手します。\n敵艦を破壊、\n仲間のスポ");

    TEST(str_chomp("스텔라 "), "스텔라");
    TEST(str_strip(" применить\n"), "применить");

    TEST(str_replace("12aa345aa67aa89", "aa", "b"), "12b345b67b89");
    TEST(str_replace("12a345a67a", "a", "bb"), "12bb345bb67bb");
    TEST(str_join(" ", vector<string>({"abc", "d", "ef"})), "abc d ef");
    TEST(str_chomp(""), "");
    TEST(str_chomp(" foo  "), " foo");
    string foo = " foo  ";
    TEST(str_chomp(std::move(foo)), " foo");
    
    Report("Ending String Tests");
#endif
    return 1;
}

#if _MSC_VER

#include <intrin.h>
#define cpuid(OUT, LEVEL) __cpuid(OUT, LEVEL)
typedef int regtype_t;

std::string str_demangle(string name)
{
    name = str_replace(name, "struct ", "");
    name = str_replace(name, "class ", "");
    name = str_replace(name, "std::basic_string<char,std::char_traits<char>,std::allocator<char> >", "std::string");
    return name;
}

std::string str_demangle(const char *str)
{
    return str_demangle(string(str));
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
    ASSERTF(status == 0 || status == -2, "__cxa_demangle:%d: %s", status,
            ((status == -1) ? "memory allocation failed" :
             (status == -2) ? "input not valid name under C++ ABI mangling rules" :
             (status == -3) ? "invalid argument" : "unexpected error code"));
    if (status != 0 || !result)
        return str;
    string name = result;
    free(result);
    name = str_replace(name, "std::__1::", "std::");
    name = str_replace(name, "unsigned long long", "uint64");
    name = str_replace(name, "unsigned int", "uint");
    name = str_replace(name, "basic_string<char, std::char_traits<char>, std::allocator<char> >", "string");
#if __APPLE__
    name = str_replace(name, "glm::detail::tvec2<float, (glm::precision)0>", "float2");
    name = str_replace(name, "glm::detail::tvec3<float, (glm::precision)0>", "float3");
    name = str_replace(name, "glm::detail::tvec2<int, (glm::precision)0>", "int2");
    name = str_replace(name, "glm::detail::tvec3<int, (glm::precision)0>", "int3");
#else
    name = str_replace(name, "glm::detail::tvec2<float,0>", "float2");
    name = str_replace(name, "glm::detail::tvec3<float,0>", "float3");
#endif
    return name;
}

std::string str_demangle(string name)
{
    return str_demangle(name.c_str());
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

