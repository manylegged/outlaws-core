
// Unicode.h - utf8 / utf16 conversion routines

// Some routines borrowd from SDL_ttf
// everything else (c) 2014 Arthur Danskin

#ifndef UNICODE_H
#define UNICODE_H

typedef std::uint8_t Uint8;
typedef std::uint16_t Uint16;
typedef std::uint32_t Uint32;

// modified from SDL2 SDL_ttf.c
/* Convert a UCS-2 string to a UTF-8 string */
inline void UCS2_to_UTF8(const Uint16 *src, Uint8 *dst)
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
inline string UCS2_to_UTF8(int ucs2)
{
    char outp[5] = {};
    const Uint16 chrs[] = { (Uint16)ucs2, 0 };
    UCS2_to_UTF8(chrs, (Uint8*)outp);
    return outp;
}

// modified from SDL2 SDL_ttf.c
// return a single ucs2 character from a utf8 stream
#define UNKNOWN_UNICODE 0xFFFD
inline Uint32 UTF8_getch(const char **src, size_t *srclen)
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

// utf8 multi-byte sequences start with something that has two or more 11s,
// continuation bytes all have 10 in highest position
// ascii always has 0 in highest pos
inline bool utf8_iscont(char byte)
{
    return (byte&0xc0) == 0x80;
}


// return byte index after advancing LEN characters in STR, starting at START
inline int utf8_advance(const string& str, int start, int len=0)
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
inline string utf8_substr(const string &utf8, int start, int len=-1)
{
    if (len == 0)
        return "";
    start = utf8_advance(utf8, start);
    return utf8.substr(start, utf8_advance(utf8, start, len) - start);
}

// return STR with LEN characters erased starting from byte START
inline string utf8_erase(const string &str, int start, int len=-1)
{
    if (len == 0)
        return str;
    string ret = str;
    start = utf8_advance(str, start);
    ret.erase(start, utf8_advance(str, start, len) - start);
    return ret;
}

// return first byte of last character
inline size_t utf8_last(const string& str)
{
    size_t last = str.size()-1;
    while (last > 0 && utf8_iscont(str[last]))
        last--;
    return last;
}

#endif // UNICODE_H
