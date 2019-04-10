
#include "StdAfx.h"
#include "Steam.h"

#include "Save.h"
#include "Symbol.h"

static DEFINE_CVAR(bool, kSteamCloudEnable, true);
DEFINE_CVAR(int, kParserMaxWarnings, 50);

static vector<string> s_early_msgs;

void ReportEarly(string msg)
{
    if (OL_IsLogOpen())
    {
        ReportFlush();
        Report(std::move(msg));
    }
    else
    {
        s_early_msgs.push_back(std::move(msg));
    }
}

void ReportFlush()
{
    if (s_early_msgs.empty())
        return;
    foreach (const string &x, s_early_msgs)
        Report(x);
    s_early_msgs.clear();
}

string TypeSerializer::cleanupType(string type)
{
    type = str_demangle(type);
    type = str_replace(type, "Serial", "");
    type = str_replace(type, " const", "");
    return type;
}


SaveParser::SaveParser() { }
SaveParser::~SaveParser() { }

SaveParser::SaveParser(const char* d, bool fok)
{
    loadData(d);
    fail_ok = fok;
}

SaveParser::SaveParser(const string &d, bool fok)
{
    loadData(d.c_str());
    fail_ok = fok;
}

bool SaveParser::loadData(const char* dta, const string &fn)
{
    fname        = fn;
    data         = dta;
    dataLastLine = data;
    dataline     = 0;
    start        = data;
    dend         = NULL;
    error_count  = 0;
    
    return data != NULL;
}

static const char* kExtensions[] = {".lua", ".lua.gz", ".lisp", ".lisp.gz", ".txt", ".json", ".json.gz"};

bool SaveParser::loadFile(const string& fname_)
{
    if (!vec_any(kExtensions, [&](const char* ex) { return str_endswith(fname_, ex); }))
        return false;
    buffer = LoadFile(fname_);
    fname = fname_;
    resetFile();
    return buffer.size();
}

void SaveParser::resetFile()
{
    loadData(buffer.c_str(), fname);
    dend = data + buffer.size();
}

SaveSerializer& SaveSerializer::instance()
{
    SaveSerializer *it = NULL;
    if (::globals.isUpdateThread()) {
        static SaveSerializer ss;
        it = &ss;
    } else {
        static SaveSerializer ss;
        it = &ss;
    }
    it->clear();
    return *it;
}

SaveSerializer& SaveSerializer::setFlag(Flags flag, bool val)
{
    setBits(flags, (ushort)flag, val);
    if (flags&HUMAN)
        indent = -1;
    return *this;
}

SaveSerializer& SaveSerializer::setColumnWidth(int v)
{
    if (v < 0)
        setFlag(COMPACT, true);
    else
        columnWidth = v;
    return *this;
}

float SaveSerializer::prepare(float f) const
{
    ASSERT(!fpu_error(f));
    if (fabsf(f) < 0.0005f || fpu_error(f))
        return 0.f;
    return f;
}

#define BINARY_UINT_MARKER '%'
#define BINARY_UINT64_MARKER '$'

bool SaveParser::parseBinaryIntegral(uint64 *v)
{
    if (parseToken(BINARY_UINT_MARKER))
    {
        uint val = 0;
        data += str_read_bytes(data, 0, &val);
        *v = val;
        return true;
    }
    else if (parseToken(BINARY_UINT64_MARKER))
    {
        data += str_read_bytes(data, 0, v);
        return true;
    }
    return false;
}

void SaveSerializer::serialize(uint i)
{
    if ((flags&BINARY) && (i > 0xfff))
    {
        o += BINARY_UINT_MARKER;
        str_append_bytes(o, i);
    }
    else
    {
        str_append_format(o, (i > 100000 ? ((flags&JSON) ? "\"#%x\"" : "%#x") : "%d"), i);
    }
}

void SaveSerializer::serialize(uint64 i)
{
    if ((flags&BINARY) && (i > 0xfff))
    {
        if (i < uint64(0xffffffff)) {
            o += BINARY_UINT_MARKER;
            str_append_bytes(o, (uint)i);
        } else {
            o += BINARY_UINT64_MARKER;
            str_append_bytes(o, i);
        }
    }
    else
    {
        str_append_format(o, (i > 100000 ? "%#llx" : "%lld"), i);
    }
}

void SaveSerializer::serialize(float f)
{
    if (flags&HUMAN) {
        str_append_format(o, "%.2f", f);
    } else {
        str_append_format(o, "%.3f", prepare(f));
        while (o.back() == '0')
            o.pop_back();
        if (o.back() == '.')
            o.pop_back();
    }
}

void SaveSerializer::indent1()
{
    const int c = (flags&COMPACT) ? 0 :
                  (flags&LISP) ? 1 : 2;

    lastnewline = o.size();
	o.append(c * indent, ' ');
    lastnewlineindentend = o.size();
}

void SaveSerializer::chompForToken(int token)
{
    if (flags&(BINARY|COMPACT)) {
        if (o.back() == ',')
            o.pop_back();
        o += (char) token;
        return;
    }
    bool needsnewline = false;
    int size = o.size();
    const int chomp = (token == '\n') ? '\n' : ',';
    while (size && (str_isspace(o[size-1]) || o[size-1] == chomp)) {
        size--;
        if (o[size] == '\n')
            needsnewline = true;
    }
    o.resize(size);
    if (token > 0)
        o += (char)token;
    if (needsnewline && token != '\n')
        insertToken('\n');
}

void SaveSerializer::insertToken(char token)
{
    switch (token) {
    case '=':
        if (flags&HUMAN)
            o += ": ";
        else if ((flags&JSON) || (flags&LISP))
            o += ':';
        else
            o += '=';
        break;
    case '{':
    case '(':
        indent++;
        if (flags&LISP) {
            o += '(';
        } else if (flags&HUMAN) {
            insertToken('\n');
        } else {
            o += token;
        }
        break;
    case '[':
        chompForToken('[');
        break;
    case ']':
    case ')':
    case '}':{
        // eat trailing whitespace and commas
        indent--;
        chompForToken((flags&LISP) ? ')' :
                      (flags&HUMAN) ? -1 : token);
        break;
    }
    case ',':
        if (flags&HUMAN) {
            insertToken('\n');
            // insertToken(' ');
            // insertToken((o.size() - lastnewline > columnWidth) ? '\n' : ' ');
        } else if (flags&LISP) {
            insertToken(' ');
        } else if (flags&(BINARY|COMPACT)) {
            o += ',';
        } else {
            chompForToken(',');
            insertToken((o.size() - lastnewline > columnWidth) ? '\n' : ' ');
        }
        break;
    case ' ':
        if ((flags&LISP) && (o.size() - lastnewline > columnWidth))
            insertToken('\n');
        else
            o += token;
        break;
    case '\n':
        if (flags&COMPACT)
            break;
        if (o.size() != lastnewlineindentend) {
            chompForToken('\n');
            indent1();
        }
        break;
    default:
        o += token;
        break;
    }
}

void SaveSerializer::insertTokens(const char* tkns)
{
    for (; *tkns; tkns++)
        insertToken(*tkns);
}


void SaveSerializer::forceInsertToken(char token)
{
    o += token;
    
    if (token == '\n')
        indent1();
}

static bool is_ident(char c)
{
    return str_isdigit(c) || str_issym(c) || c == '.';
}

static bool is_ident(const char* s)
{
    if (!str_issym(*s))
        return false;
    while (is_ident(*s))
        s++;
    return *s == '\0';
}

void SaveSerializer::insertName(const char* name)
{
    if (flags&JSON)
        serialize(name);
    else if (flags&HUMAN)
        o += str_capitalize(name);
    else if (is_ident(name))
        o += name;
    else
        serialize(name);
}

void SaveSerializer::serializeKey(const char* name)
{
    if ((flags&LISP) && is_ident(name)) {
        insertToken(':');
        insertName(name);
        insertToken(' ');
    } else {
        insertName(name);
        insertToken('=');
    }
}

void SaveSerializer::serializeKey(const Symbol &name)
{
    serializeKey(name.str().c_str()); 
}

void SaveSerializer::serialize(const Symbol &st)
{
    // serialize(st.str());
    insertName(st.str());
}

void SaveSerializer::padColumn(int width)
{
    int c = (o.size() - lastnewline);
    o.append(max(1, width - c), ' ');
}

void SaveSerializer::serializeE(uint64 v, const EnumType &e)
{
    if (flags&BINARY)
    {
        serialize(v);
        return;
    }
    
    // try exact match first
    lstring name = e.getName(v);
    if (name)
    {
        insertName(name.c_str());
        return;
    }

    if ((flags&JSON) && e.isBitset())
    {
        insertToken('[');
        foreach (const EnumType::Elem &se, e.elems)
        {
            if ((v&se.second) != se.second)
                continue;
            insertName(se.first.c_str());
            v &= ~se.second;
            if (v == 0)
                break;
            insertToken(',');
        }
        insertToken(']');
        return;
    }

    // then try to build up bits
    if (v != 0 && v != ~0 && e.isBitset())
    {
        foreach (const EnumType::Elem &se, e.elems)
        {
            if ((v&se.second) != se.second)
                continue;
            o += se.first.c_str();
            v &= ~se.second;
            if (v == 0)
                return;
            o += (flags&LISP) ? '/' : '|';
        }
    }
    // finally resort to numbers!!!
    serialize(v);
}

void SaveSerializer::serializeEAll(const EnumType &e)
{
    insertToken('{');
    int i = 0;
    foreach (const EnumType::Elem &se, e.elems)
    {
        if (i != 0) {
            insertToken(',');
        }

        o += se.first.c_str();
        i++;
    }
    insertToken('}');
}

static bool scanEof(const char* data)
{
    while (str_isspace(*data))
        data++;
    return (*data == '\0');
}

ParserLocation SaveParser::getCurrentLoc() const
{
    ParserLocation loc;
    loc.fname = fname;
    loc.line = dataline;
    loc.logger = logger;

    const char* lineBegin = std::max(data, start + 1);
    while (lineBegin > dataLastLine && *(lineBegin-1) != '\n' && *(lineBegin-1) != '\0' &&
           (data - lineBegin) < 200)
    {
        lineBegin--;
    }
    for (const char *ptr=lineBegin; *ptr != '\0' && *ptr != '\n' && (ptr - lineBegin) < 300; ptr++)
        loc.currentLine += *ptr;
    loc.currentLine = str_chomp(loc.currentLine);
    for (ParseContext *s=stack; s && lineBegin <= s->begin && (data - s->begin) > 1; s=s->next)
        loc.context.push_back(make_pair(s->begin - lineBegin + 1, s->desc()));
    loc.currentLineColumn = data - lineBegin + 1;
    loc.isEof = scanEof(data);
    return loc;
}

string ParserLocation::format(const char* type, const string &msg, bool first) const
{
    if (!first)
        type = "note";
    string ret = str_format("%s:%d: %s: ", fname.c_str(), line, type) + msg;
    if (!first)
        return ret;
    if (context.size() && currentLineColumn >= 0 && currentLine.size())
    {
        string cline = currentLine;

        if (context[0].first > 100)
        {
            int2 col = i2(context[0].first, currentLineColumn);
            cline = "..." + cline.substr(col.x - 40 - 3, 100);
            col.y = col.y - col.x + 40;
            col.x = 40;
            string marker(max(1, col.x-1), ' ');
            marker.append(col.y - col.x, '~');
            if (context[0].second.size())
                marker += str_format(" (%s)", context[0].second);
            if (cline.size() > 100) {
                cline.resize(100);
                cline += "...";
            }
            ret += "\n" + cline + "\n" + marker;
        }
        else
        {
            ret += "\n" + cline;
            for_ (ctx, context)
            {
                int2 col = i2(ctx.first, currentLineColumn);
                string marker(max(1, col.x-1), ' ');
                marker.append(col.y - col.x, '~');
                if (ctx.second.size())
                    marker += str_format(" (%s)", ctx.second);
                if (cline.size() > 100) {
                    cline.resize(100);
                    cline += "...";
                }
                ret += "\n" + marker;
            }
        }
    }
    else if (currentLineColumn >= 0 && currentLine.size())
    {
        int col = currentLineColumn;
        string cline = currentLine;
        if (col > 100 && cline.size() >= col) {
            cline = "..." + cline.substr(col - 40 - 3, 100);
            col = 40;
        }
        string marker(max(1, col), ' ');
        marker.back() = '^';
        if (cline.size() > 100) {
            cline.resize(100);
            cline += "...";
        }
        ret += "\n" + cline + "\n" + marker;
    }
    else if (currentLine.size())
    {
        ret += "\n" + currentLine;
    }
    if (isEof)
    {
        ret += "\n(hit end of file, check for unbalanced '{' or '}')";
    }

    return ret;
}

string ParserLocation::shortLoc() const
{
    return fname + str_format(":%d", line); 
}

void ParserLocation::Report(const char* type, const string &msg) const
{
    string it = format(type, msg);
    DPRINT(INFO, "%s", it.c_str());
    if (logger)
        logger->Report(it);
}

string SaveParser::errmsg(const char* type, const string& msg) const
{
    return getCurrentLoc().format(type, msg, (error_count == 0));
}


char SaveParser::nextChar()
{
    const char chr = *data;
    if (chr == '\n') {
        dataLastLine = data + 1;
        dataline++;
        if (progress)
        {
            if (!dend)
                dend = start + strlen(start);
            *progress = (double) (data - start) / (dend - start);
        }
    }
    data++;
    return chr;
}


bool SaveParser::skipSpace(const char** after)
{
    if (after)
    {
        const char* &ptr = *after;
        while (str_isspace(*ptr))
            ptr++;
        return true;
    }
    
    if (!data)
        return false;
    
    while (1)
    {
        const char chr = *data;
        if (chr == '\0')
        {
            break;
        }
        else if (str_isspace(chr))
        {
            nextChar();
        }
        else if (!cliMode && (chr == '#' || (chr == ';' &&
                                             (data[1] == ';' || data[1] == ' '))))
        {
            while (*data != '\n' && *data != '\0')
            {
                data++;
            }
        }
        else if (!cliMode && 
                 ((chr == '-' && data[1] == '-') ||
                  (chr == '/' && data[1] == '/')))
        {
            data += 2;
            if (data[0] == '[' && data[1] == '[')
            {
                while (data[0] != '\0' && !(data[0] == ']' && data[1] == ']'))
                {
                    nextChar();
                }
            }
            else
            {
                while (*data != '\n' && *data != '\0')
                {
                    data++;
                }
            }
        }
        else
        {
            break;
        }
    }
    return true;
}

template <typename T>
static uint parse_exponent(const char* data, T *v)
{
    if (!(*data == 'e' || *data == 'E'))
        return 0;
    data++;
    char* end = 0;
    const double exp = strtod(data, &end);
    if (end <= data)
        return 1;
    *v = round(*v * pow(10.0, exp));
    return (end - data) + 1;
}

bool SaveParser::parseIntegral(uint64* v)
{
    skipSpace();
    if (parseBinaryIntegral(v))
        return true;
    char* end = 0;
    uint64 lval = strtoull(data, &end, 0);
    if (end <= data)
        return false;
    data = end;
    data += parse_exponent(data, &lval);
    *v = lval;
    return true;
}

bool SaveParser::parseIntegral(long long *v)
{
    skipSpace();
    uint64 bv;
    if (parseBinaryIntegral(&bv))
    {
        *v = bv;
        return true;
    }
    // parse "#453453" style colors for JSON/etc.
    const bool quoted = (data[0] == '"' && data[1] == '#');
    if (quoted)
        data += 2;
    char* end = 0;
    long long lval = strtoll(data, &end, quoted ? 16 : 0);
    if (end <= data)
        return false;
    data = end;
    data += parse_exponent(data, &lval);
    *v = lval;
    if (quoted)
        parseToken('"');
    return true;
}

bool SaveParser::parseToken(const char* token, const char** after)
{
    const size_t len = strlen(token);
    if (len == 1)
        return parseToken(token[0], after);
    skipSpace(after);
    const char* &ptr = (after ? *after : data);
    bool found = strncmp(ptr, token, len) == 0;
    if (found)
        ptr += len;
    return found;
}

static bool is_token(char chr, char token)
{
    if (chr == '\0')
        return false;
    switch (token)
    {
    case '{': return strchr("{([", chr);
    case '}': return strchr("})]", chr);
    case '|': return strchr("|/", chr);
    case '=': return strchr("=:", chr);
    default:  return chr == token;
    }
}

bool SaveParser::parseToken(char token, const char** after)
{
    skipSpace(after);
    const char* &ptr = (after ? *after : data);
    if (!is_token(*ptr, token))
        return false;
    ptr++;
    return true;
}

bool SaveParser::atToken(char token)
{
    skipSpace();
    return is_token(*data, token);
}

bool SaveParser::atToken(const char* token)
{
    skipSpace();
    return strncmp(data, token, strlen(token)) == 0;
}


bool SaveParser::parseToTokens(string *s, const char *tokens)
{
    skipSpace();
    while (*data != '\0' && !strchr(tokens, *data))
    {
        if (*data == '\0')
            return false;
        *s += nextChar();
    }
    return *data != '\0';
}

bool SaveParser::parseIdent(std::string* s, const char** after)
{
    skipSpace(after);
    const char* &ptr = (after ? *after : data);
    if (!str_issym(*ptr))
        return parseQuotedString(s, '"', after); // json
    s->clear();
	ParseContext pc(this, "ident");
    while (is_ident(*ptr))
    {
        *s += *ptr;
        ptr++;
    }
    return true;
}

static bool isargs(char c)
{
    return c && (str_issym(c) || str_isdigit(c) || c == '-' || c == '.');
}

bool SaveParser::parseArg(string* s)
{
    s->clear();
    skipSpace();
    if (!isargs(*data))
        return false;
    while (isargs(*data))
    {
        *s += *data;
        data++;
    }
    return true;
}

inline bool str_isname(char c)
{
    return (str_isprint(c) || utf8_isutf8(c)) &&
        !str_contains(" \t\n,{}()|", c);
}

bool SaveParser::parseName(std::string* s)
{
    skipSpace();
    s->clear();
    ParseContext pc(this, "name");
    if (*data == '\0')
        return false;
    while (str_isname(*data))
    {
        *s += *data;
        data++;
    }
    return true;
}


void SaveSerializer::serialize(const char* s)
{
    if (!s) {
        o += "\"\"";
        return;
    }

    o += '"';
    for (; *s; ++s)
    {
        switch (*s) {
        case '\n': o += "\\n"; break;
        case '"':  o += "\\\""; break;
        case '\\': o += "\\\\"; break;
        default:   o += *s; break;
        }
    }
    o += '"';
}

bool SaveParser::parseQuotedString(string *s, char quote, const char** after)
{
    if (!parseToken(quote, after))
        return false;

    s->clear();
    const char* &ptr = (after ? *after : data);
    while (*ptr != quote && *ptr != '\0')
    {
        if (*ptr == '\\')
        {
            ptr++;
            switch (*ptr) {
            case 'n':  *s += '\n'; break;
            case '\\': *s += '\\'; break;
            case '\n': break;
            default:
                if (*ptr != quote)
                    *s += '\\';
                *s += *ptr;
                break;
            }
        }
        else
        {
            *s += *ptr;
        }

        if (after)
            ptr++;
        else
            nextChar();
    }

    if (parseToken(quote, after))
        return true;
    if (after)
        return false;
    PARSE_FAIL("expected closing '%c' while parsing string, got EOF", quote);
}

static bool spaces_only(const char* v)
{
    for (; str_isspace(*v); v++)
        if (*v == '\n')
            return false;
    return true;
}

bool SaveParser::parseKey(string *v)
{
    const char* after = data;
            
    // lisp :key val
    if (parseToken(':', &after) && spaces_only(after) && parseIdent(v, &after))
    {
        data = after;
        return true;
    }
    // lua key = val OR JSON "key" : val
    if (parseIdent(v, &after) && spaces_only(after) && parseToken('=', &after) &&
        !(*(after-1) == ':' && str_isalpha(*after)))     // NOTE avoid (data :key val) misparse as data : key
    {
        data = after;
        return true;
    }
    return false;
}

bool SaveParser::parseKey(Symbol *s)
{
    string v;
    if (!parseKey(&v))
        return false;
    *s = Symbol(v);
    return true;
}


bool SaveParser::parse(string* s)
{
    const bool lcl = parseToken('_');
    if (lcl) {
        PARSE_FAIL_UNLESS(parseToken('('), "Expected '(' after gettext _");
    }
    ParseContext pc(this, "string");
    bool success = false;
    if (parseQuotedString(s, '"') || parseQuotedString(s, '\'')) {
        success = true;
    } else if (str_isname(*data)) {
        success = parseName(s);
    }
    if (lcl) {
        PARSE_FAIL_UNLESS(parseToken(')'), "Expected closing ')' after gettext _");
        if (success)
            *s = gettext_(s->c_str());
    }
    return success;
}

bool SaveParser::parse(Symbol* s)
{
    string t;
    if (!parse(&t))
        return false;
    if (t.size() > kSymbolMaxChars)
    {
        t.resize(kSymbolMaxChars);
        warn("Truncating to '%s', symbol length %d>%d chars", t, (int)t.size(), kSymbolMaxChars);
    }
    *s = Symbol(t);
    return true;
}

bool SaveParser::parse(lstring* s)
{
    string t;
    bool r = parse(&t);
    if (r)
        *s = lstring(std::move(t));
    return r;
}

bool SaveParser::parse(bool* v)
{
    if (parseToken('1')) {
        *v = true;
    } else if (parseToken('0')) {
        *v = false;
    } else {
        std::string s;
        PARSE_FAIL_UNLESS(parseIdent(&s), "expected 'true' or 'false' while parsing bool");
        s = str_tolower(s);     // ignore case
        if (s == "t" || s == "true")
            *v = true;
        else if (s == "nil" || s == "false")
            *v = false;
        else PARSE_FAIL("expected 'true' or 'false' parsing bool, got '%s'", s);
    }
    return true;
}

bool SaveParser::parse(float* v)
{
    double val = 0;
    if (!parse(&val))
        return false;
    *v = val;
    return true;
}

bool SaveParser::parse(double* v)
{
    ParseContext pc(this, "float");
    
    double val = 0;
    if (parseToken("pi"))
    {
        val = M_PI;
    }
    else if (parseToken("tau"))
    {
        val = M_TAU;
    }
    else
    {
        char* end = 0;
        val = strtod(data, &end);
        // eat msvc special nans
        // #IO #INF #SNAN #QNAN #IND
        // while (*end && str_contains("#IONFSAQD", *end))
            // end++;
        if (end == data)
            return false;
        // PARSE_FAIL_UNLESS(end > data && !fpu_error(val), "expected float");
        data = end;
    }

    if (parseToken('*'))
    {
        double m = 1;
        PARSE_FAIL_UNLESS(parse(&m), "expected float while parsing float expression");
        val *= m;
    }
    else if (parseToken('/'))
    {
        double div = 1.0;
        PARSE_FAIL_UNLESS(parse(&div), "expected float while parsing float expression");
        val /= div;
    }
    
    *v = val;
    return true;
}

bool SaveParser::parseE(uint64 *h, const EnumType &e)
{
    ParseContext pc(this, e.name.c_str());
    std::string s;
    
    if (parseToken('['))        // JSON
    {
        while (!parseToken(']'))
        {
            PARSE_FAIL_UNLESS(parse(&s), "expected enum field");
            const uint64 val = e.getVal(str_toupper(s));
            PARSE_FAIL_UNLESS(val != ~0, "enum '%s' has no member '%s'", e.name, s);
            *h |= val;
            parseToken(',');
        }

        return true;
    }
    
    do {
        if (parseIdent(&s))
        {
            const uint64 val = e.getVal(str_toupper(s));
            PARSE_FAIL_UNLESS(val != ~0, "enum '%s' has no member '%s'", e.name, s);
            *h |= val;
        }
        else
        {
            uint64 v = 0;
            PARSE_FAIL_UNLESS(parse(&v), "expected uint64 or member while parsing enum '%s'", s);
            *h |= v;
        }
    } while (parseToken('|'));
    return true;
}

DynamicObj::DynamicObj() : type(&ReflectionLayout::instance<ReflectionNull>()) { }
DynamicObj DynamicObj::operator[](const char* s) const { return (*type)(self, s); }
DynamicObj DynamicObj::operator[](const uint i) const { return (*type)(self, i); }
size_t DynamicObj::size() const { return type->size(); }
DynamicObj DynamicObj::operator()() const { return (*type)(self); }
bool DynamicObj::operator==(const DynamicObj &o) const { return (type == o.type) && type->equal(self, o.self); }
DynamicObj DynamicObj::clone() const { return DynamicObj(*type, type->clone(self)); }

LispVal DynamicObj::read() const { return type->read(self); }
bool DynamicObj::write(LispVal v, const LispEnv *e) const { return type->write(self, v, e); }
void DynamicObj::serialize(SaveSerializer &s) const { type->serialize(s, self); }
int DynamicObj::parse(SaveParser &p) const { return type->parse(p, self); }
int DynamicObj::skip(SaveParser &p) const { return type->skip(p); }
const string &DynamicObj::name() const { return type->name(); }
void DynamicObj::clear() const { type->clear(self); }

template <typename T>        
static bool skipDeprecated(SaveParser &sp, const char* name)
{
    T val{};
    if (!sp.parse(&val))
        return false;

    DPRINT(DEPRECATED, "%s", sp.errmsg("warning", str_format("Ignored deprecated field '%s' : %s", name, PRETTY_TYPE(T))));
    return true;
}

struct CVarIndex {
    std::map<lstring, CVarBase*>                        index;
    std::unordered_map<const void*, CVarBase*>          value_index;
    std::unordered_map<lstring, pair<string, string> >  incomplete_index;
    std::mutex                                          mutex;
};

static CVarIndex &cvar()
{
    static CVarIndex i;
    return i;
}

inline string file_name_onedir(const char* file)
{
    size_t slash = str_rfind(file, OL_PATH_SEP);
    if (slash == ~0)
        return file;
    slash = str_rfind(file, OL_PATH_SEP, slash-1);
    if (slash == ~0)
        return file;
    file += slash + 1;
#if OL_WINDOWS
    return str_replace(file, '\\', '/');
#else
    return file;
#endif
}
// MSVC uses lower case file names to do the same on other platforms
CVarBase::CVarBase(lstring name, const char* comment, const char *file)
    : m_name(name), m_comment(comment), m_file(str_tolower(file_name_onedir(file)))
{
    ASSERT(!cvar().index[name]);
    cvar().index[name] = this;
}

CVarBase::~CVarBase()
{
    ASSERT(cvar().index[m_name] == this);
    cvar().index.erase(m_name);
}

std::string CVarBase::get(lstring key)
{
    std::lock_guard<std::mutex> l(cvar().mutex);
    CVarBase *cv = cvar().index[key];
    return cv ? cv->toString() : "";
}

vector<std::string> CVarBase::comp(lstring key)
{
    std::lock_guard<std::mutex> l(cvar().mutex);
    CVarBase *cv = cvar().index[key];
    return cv ? cv->comp() : vector<string>();
}

static bool s_cvar_file_loaded = false;

void CVarBase::onClose()
{
    std::lock_guard<std::mutex> l(cvar().mutex);
    
    foreach (auto &x, cvar().incomplete_index)
    {
        Report(x.second.second.c_str());
    }

    // also write cvar file if none exists
    // if (s_cvar_file_loaded)
    // {
        // Report("cvars.txt changed - not dumping");
        // return;
    // }

    writeToFile();
}

static void writeCvar(SaveSerializer &ss, const char* name, const CVarBase &cv)
{
    if (cv.isDefault() || !cv.save())
        ss.o += "# ";
	ss.o += name;
    ss.insertTokens(" = ");
    cv.serialize(ss);
    // if (cv.getComment() || cv.hasMinMax())
    {
        ss.padColumn(50);
        ss.o += "# (";
        ss.o += cv.getTypeString();
        ss.o += ") ";
        ss.o.append(max(0, 6 - (int)str_len(cv.getTypeString())), ' ');
        ss.o += cv.getFile();
        if (cv.getComment().size())
        {
            ss.o += " // ";
            ss.o += cv.getComment();
        }
    }
    ss.forceInsertToken('\n');
}

bool CVarBase::writeToFile()
{
    SaveSerializer ss;
    ss.o += "# -*- mode: default-generic -*-\n";
    ss.o += "# NOTE: this file is read on startup and rewritten on shutdown\n";
    if (!OLG_UseDevSavePath())
    {
        ss.o += str_format("# %s version: %s\n", OLG_GetName(), getBuildDate());
        str_wrap_options_t w(90);
        w.newline = "\n#    ";
        ss.o += string("# ") + str_word_wrap(OL_GetPlatformDateInfo(), w) + "\n";
    }
    ss.forceInsertToken('\n');
    ss.setColumnWidth(-1);

    vector<const CVarBase*> vec;
    for_ (it, cvar().index)
    {
        if (it.second)
            vec.push_back(it.second);
    }
    vec_sort(vec, [](const CVarBase *a, const CVarBase *b) {
                      return (a->getFile() != b->getFile()) ? a->getFile() < b->getFile() :
                          a->getName() < b->getName(); });
    for_ (cv, vec)
        if (!cv->isDefault())
            writeCvar(ss, cv->getName().c_str(), *cv);
    for_ (cv, vec)
        if (cv->isDefault())
            writeCvar(ss, cv->getName().c_str(), *cv);

    bool ok = ZF_SaveFileRaw("data/cvars.txt", ss.str());
    Reportf("Wrote default cvars to cvars.txt: %s", ok ? "OK" : "FAILED");
    return ok;
}

bool CVarBase::parseCVars(SaveParser &sp, bool save_value)
{
    std::lock_guard<std::mutex> l(cvar().mutex);
    std::string s;
    while (!sp.isEof())
    {
        if (!sp.parseArg(&s))
        {
            sp.error("parsing cvars from '%s'", sp.getFileName());
            ASSERT_FAILED("", "Cvar parse failed");
            break;
        }
        if (sp.getFileName().size() && sp.getFileName() != "-")
            s_cvar_file_loaded = true;
            
        if (str_startswith(s, "--"))
            s = str_replace(s, "--", "k");

        size_t dot = str_find(s, '.');
        
        const lstring name = str_substr(s, 0, dot);
        CVarBase *cv = cvar().index[name];

        if (cv == NULL) {
            string val;
            sp.parseToTokens(&val, sp.getCliMode() ? " -\n" : ";\n");
            if (!val.size())
                return sp.error("no value for uninstantiated cvar '%s'", name);
            const string msg = sp.errfmt("warning", "uninstantiated cvar '%s'", name);
            cvar().incomplete_index[name] = make_pair(val, msg);
            continue;
        }

        cv->save(save_value);
        
        const bool isor = sp.parseToken('|');
        const bool isandnot = !isor && sp.parseToken('&');
        sp.parseToken('='); // optional

		if (dot != -1) {
			if (!cv->parseValue(sp, str_substr(s, dot + 1)))
				return sp.error("failed to parse cvar '%s'.%s", name, str_substr(s, dot + 1));
		} else if (isor) {
            if (!cv->parseValueOr(sp))
                return sp.error("failed to parse |= for cvar '%s'", name);
        } else if (isandnot) {
            if (!sp.parseToken('~'))
                return sp.error("expected ~ after &=");
            if (!cv->parseValueAndNot(sp))
                return sp.error("failed to parse &= for cvar '%s'", name);
        } else if (!cv->parseValue(sp))
            return sp.error("failed to parse cvar '%s'", name);

        sp.parseToken(';'); // optional
    }
    return true;
}

static void writeCvarNameVal(SaveSerializer &ss, const CVarBase *cvar)
{
    if (cvar)
    {
        ss.o += cvar->getName().c_str();
        ss.insertTokens(" = ");
        ss.o += cvar->toString();
        ss.insertToken('\n');
    }
}

std::string CVarBase::getAll()
{
    SaveSerializer ss;
    std::lock_guard<std::mutex> l(cvar().mutex);
    foreach (auto &it, cvar().index)
        writeCvarNameVal(ss, it.second);
    return ss.str();
}

std::string CVarBase::getMatching(const std::string &query)
{
	const string q = str_tolower(query);
    SaveSerializer ss;
    std::lock_guard<std::mutex> l(cvar().mutex);
    foreach (auto &it, cvar().index)
    {
        if (it.second && (str_tolower(it.first.str()).find(q) != -1 ||
                          str_tolower(it.second->getComment()).find(q) != -1 ||
                          str_tolower(it.second->getFile().str()).find(q) != -1))
        {
            writeCvarNameVal(ss, it.second);
        }
    }
    return ss.str();
}


vector<std::string> CVarBase::getAllNames()
{
    std::lock_guard<std::mutex> l(cvar().mutex);
    vector<std::string> all;
    foreach (auto &it, cvar().index)
    {
        if (it.second)
        {
            all.push_back(it.first.str());
        }
    }
    return all;
}

bool CVarBase::initialize()
{
    std::lock_guard<std::mutex> l(cvar().mutex);

    cvar().value_index[getVptr()] = this;
    
    if (cvar().incomplete_index.count(m_name))
    {
        SaveParser sp(cvar().incomplete_index[m_name].first);
        parseValue(sp);
        cvar().incomplete_index.erase(m_name);
        return true;
    }
    return false;
}

void loadCVars(const char* txt)
{
    SaveParser sp;
    if (sp.loadFile("data/cvars.txt")) {
        CVarBase::parseCVars(sp, true);
    } else {
        ReportEarlyf("cvars file not found");
    }

    if (sp.loadFile("data/platform_cvars.txt")) {
        CVarBase::parseCVars(sp, false);
    } else {
        ReportEarlyf("platform cvars file not found");
    }

    if (txt) {
        SaveParser sp;
#if __APPLE__
        sp.loadData(str_replace(txt, "-NSDocumentRevisionsDebugMode YES", "").c_str());
#else
        sp.loadData(txt);
#endif
        sp.setCliMode(true);
        CVarBase::parseCVars(sp, false);
    }
}

pair<CVarBase*, DynamicObj> CVarBase::get_cvar(const char* key)
{
    size_t dot = str_find(key, '.');
    CVarBase *cv = map_get(cvar().index, str_substr(key, 0, dot));
    if (!cv)
        return make_pair(cv, DynamicObj());
    return make_pair(cv, (dot == -1) ? cv->dyobj() : cv->dyobj()[key + dot+1]);
}

CVarBase* CVarBase::get_cvar(void *value)
{
    return cvar().value_index[value];
}


#define DEF_CCV(TYPE)                                                   \
    TYPE CreateCVar_ ## TYPE(const char* name, TYPE* vptr, TYPE value, const char* file) { \
        return (new CVar<TYPE>(name, vptr, value, file, ""))->getDefault(); \
    }

DEF_CCV(float);
DEF_CCV(float2);
DEF_CCV(float3);
DEF_CCV(double);
DEF_CCV(double2);
DEF_CCV(double3);
DEF_CCV(int);
DEF_CCV(uint);
DEF_CCV(int2);
DEF_CCV(bool);



template <typename T>
bool checkMinMax1(CVar<T> *cv)
{
    if (!cv->hasMinMax())
        return true;
    T cl = clamp(*cv->m_vptr, cv->m_min, cv->m_max);
    if (cl == *cv->m_vptr)
        return true;
    *cv->m_vptr = cl;
    return false;
}

template<> bool CVar<float>::checkMinMax() { return checkMinMax1(this); }
template<> bool CVar<int>::checkMinMax() { return checkMinMax1(this); }
template<> bool CVar<float2>::checkMinMax() { return checkMinMax1(this); }

// editor console callback
string cmd_cvar(void* data, const char* cmdname, const char *args)
{
    // cvar
    if (strlen(args) == 0)
    {
        return CVarBase::getAll();
    }

    // cvar kName = val
    {
        SaveParser sp(args, true);
        if (CVarBase::parseCVars(sp))
        {
            std::string s;
            SaveParser(args, true).parseIdent(&s);
            return CVarBase::get(lstring(s));
        }
    }

    // cvar query
    string query;
    SaveParser sp(args, true);
    if (sp.parse(&query))
        return CVarBase::getMatching(query);
    else
        return "specify a search query";
}

string cmd_save_cvars(void* data, const char* name, const char *args)
{
    CVarBase::writeToFile();
    return "";
}

bool isSteamCloudEnabled()
{
    return kSteamCloudEnable && SteamRemoteStorage() && !OLG_UseDevSavePath();
}

bool SaveFile(const char* fname, const char* data, int size)
{
    if (isSteamCloudEnabled() && !str_startswith(fname, "~"))
    {
        if (steamFileWrite(fname, data, size, size))
            return true;
    }

    return ZF_SaveFileRaw(fname, data, size);
}

bool SaveCompressedFile(const char* fname, const char* data, int size)
{
    if (isSteamCloudEnabled() && !str_startswith(fname, "~"))
    {
        const string bytes = ZF_Compress(data, size);
        const string gzname = str_format("%s.gz", fname);
        if (steamFileWrite(gzname.c_str(), &bytes[0], bytes.size(), size))
            return true;
    }

    return ZF_SaveFile(fname, data, size);
}


string LoadFile(const char* fname)
{
    if (isSteamCloudEnabled())
    {
        const string gzname = str_format("%s.gz", fname);
        if (SteamFileExists(gzname.c_str()))
        {
            // 1. steam gzipped file
            string data = steamFileRead(SteamRemoteStorage(), gzname.c_str());
            if (data.size())
                return ZF_Decompress(data.data(), data.size());
        }
        else if (SteamFileExists(fname))
        {
            // 2. steam uncompressed file
            return steamFileRead(SteamRemoteStorage(), fname);
        }
    }

    // 3. system files
    return ZF_LoadFile(fname);
}

string LoadFileRaw(const char* fname)
{
    if (isSteamCloudEnabled() && SteamFileExists(fname))
    {
        return steamFileRead(SteamRemoteStorage(), fname);
    }

    return ZF_LoadFileRaw(fname);
}


int RemoveFileOrDirectory(const char* path)
{
    int deleted = 0;
    if (isSteamCloudEnabled())
    {
        deleted = steamDeleteRecursive(path);
        // fallthrough to also delete main save directory
    }
    return OL_RemoveFileOrDirectory(path) + deleted;
}

bool RemoveFile(const char *path)
{
    int deleted = 0;
    if (isSteamCloudEnabled())
    {
        deleted = SteamFileDelete(path);
        // fallthrough to also delete main save directory
    }
    return OL_RemoveFile(path) + deleted;
}

int FileForget(const char *path)
{
    return !isSteamCloudEnabled() || SteamFileForget(path);
}


bool FileExists(const char* fname)
{
    const string gzname = str_format("%s.gz", fname);
    if (isSteamCloudEnabled() && (SteamFileExists(gzname.c_str()) || SteamFileExists(fname)))
        return true;
    return OL_FileDirectoryPathExists(gzname.c_str()) || OL_FileDirectoryPathExists(fname);
}

vector<string> ListDirectory(const char* dname)
{
    vector<string> files;
    
    for (const char **ptr=OL_ListDirectory(dname); ptr && *ptr; ++ptr)
        files.push_back(*ptr);

    const string base = str_path_standardize(dname) + "/";
    SteamForEachFile([&](const string &name, int size) {
            if (str_startswith(name, base))
                files.push_back(name);
        });

    return files;
}
