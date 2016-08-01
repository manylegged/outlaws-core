
#include "StdAfx.h"
#include "Steam.h"

#include "Save.h"

static DEFINE_CVAR(bool, kSteamCloudEnable, true);
static DEFINE_CVAR(int, kParserMaxWarnings, 50);

static vector<string> s_early_msgs;

void ReportEarly(const string &msg)
{
    if (OL_IsLogOpen())
    {
        ReportFlush();
        Report(msg);
    }
    else
    {
        s_early_msgs.push_back(msg);
    }
}

void ReportEarlyf(const char* fmt, ...)
{
    va_list vl;
    va_start(vl, fmt);
    ReportEarly(str_vformat(fmt, vl));
    va_end(vl);
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

bool SaveParser::loadFile(const string& fname_)
{
    if (!str_endswith(fname_, ".lua") && !str_endswith(fname_, ".lua.gz") && !str_endswith(fname_, ".txt"))
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
    if (globals.isUpdateThread()) {
        static SaveSerializer ss;
        it = &ss;
    } else {
        static SaveSerializer ss;
        it = &ss;
    }
    it->clear();
    return *it;
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
        str_append_format(o, (i > 100000 ? "%#x" : "%d"), i);
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
    if (flags&FORMAT_DISPLAY) {
        str_append_format(o, "%.2f", f);
    } else {
        str_append_format(o, "%.3f", prepare(f));
    }
    while (o.back() == '0')
        o.pop_back();
    if (o.back() == '.')
        o.pop_back();
}


bool SaveSerializer::isIdent(const char* s)
{
    if (!str_issym(*s)) {
        return false;
    }
    while (str_isdigit(*s) || str_issym(*s)) {
        s++;
    }
    return *s == '\0';
}

void SaveSerializer::chompForToken(int token)
{
    if (flags&(BINARY|FORMAT_NONE)) {
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
        if (flags&FORMAT_DISPLAY)
            o += ": ";
        else
            o += '=';
        break;
    case '{':
        indent++;
        if (flags&FORMAT_DISPLAY) {
            insertToken('\n');
        } else {
            o += token;
        }
        break;
    case '[':
        chompForToken('[');
        break;
    case ']':
    case '}':{
        // eat trailing whitespace and commas
        indent--;
        chompForToken(!(flags&FORMAT_DISPLAY) ? token : -1);
        break;
    }
    case ',':
        if (flags&FORMAT_DISPLAY) {
            insertToken('\n');
        } else if (flags&(BINARY|FORMAT_NONE)) {
            o += ',';
        } else {
            chompForToken(',');
            if (o.size() - lastnewline > columnWidth)
                insertToken('\n');
            else
                o += ' ';
        }
        break;
    case '\n':
        if (flags&FORMAT_NONE)
            break;
        if (o.size() != lastnewlineindentend) {
            chompForToken('\n');
            lastnewline = o.size();
            indent1();
            lastnewlineindentend = o.size();
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


void SaveSerializer::insertName(const char* name)
{
    if (flags&FORMAT_DISPLAY)
        o += str_capitalize(name);
    else
        o += name;
}

void SaveSerializer::serializeE(uint64 v, const EnumType &e)
{
    if (flags&BINARY) {
        serialize(v);
        return;
    }
    
    // try exact match first
    lstring name = e.getName(v);
    if (name) {
        o += name.c_str();
        return;
    }

    // then try to build up bits
    if (v != 0 && v != ~0 && e.isBitset())
    {
        foreach (const EnumType::Pair &se, e.elems)
        {
            if ((v&se.second) != se.second)
                continue;
            o += se.first.c_str();
            v &= ~se.second;
            if (v == 0)
                return;
            o += "|";
        }
    }
    // finally resort to numbers!!!
    serialize(v);
}

void SaveSerializer::serializeEAll(const EnumType &e)
{
    insertToken('{');
    int i = 0;
    foreach (const EnumType::Pair &se, e.elems)
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
    while (lineBegin >= dataLastLine && *(lineBegin-1) != '\n' && *(lineBegin-1) != '\0' &&
           (data - lineBegin) < 200)
    {
        lineBegin--;
    }
    for (const char *ptr=lineBegin; *ptr != '\0' && *ptr != '\n' && (ptr - lineBegin) < 300; ptr++)
        loc.currentLine += *ptr;
    loc.currentLine = str_chomp(loc.currentLine);
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
    if (currentLineColumn >= 0 && currentLine.size())
    {
        int col = currentLineColumn;
        string cline = currentLine;
        if (col > 100 && cline.size() >= col) {
            cline = "..." + cline.substr(col - 40 - 3, 100);
            col = 40;
        }
        string marker(col, ' ');
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
    DPRINT(INFO, ("%s", it.c_str()));
    if (logger)
        logger->Report(it);
}

string SaveParser::errmsg(const char* type, const string& msg) const
{
    return getCurrentLoc().format(type, msg, (error_count == 0));
}

string SaveParser::errfmt(const char* type, const char* errformat, ...) const
{
    va_list vl;
    va_start(vl, errformat);
    string msg = errmsg(type, str_vformat(errformat, vl));
    va_end(vl);
    return msg;
}

static void SaveReport(LogRecorder *logger, const string &msg)
{
    if (logger)
        logger->Report(msg);
    ReportEarly(msg);
}

void SaveParser::warn(const char* errformat, ...) const
{
    if (fail_ok || warn_count >= kParserMaxWarnings)
        return;
    va_list vl;
    va_start(vl, errformat);
    string msg = str_vformat(errformat, vl);
    va_end(vl);

    SaveReport(logger, errmsg("warning", msg));

    warn_count++;
    if (warn_count == kParserMaxWarnings)
    {
        SaveReport(logger, str_format("%s: warning: ignoring future warnings (kParserMaxWarnings=%d)",
                                      fname.c_str(), kParserMaxWarnings));
    }
}

bool SaveParser::error(const char* errformat, ...) const
{
    if (fail_ok)
        return false;
    va_list vl;
    va_start(vl, errformat);
    string msg = str_vformat(errformat, vl);
    va_end(vl);

    SaveReport(logger, errmsg("error", msg));
    
    error_count++;
    return false;
}

bool SaveParser::fail1(const char* func, uint linenum, const char* errformat, ...) const
{
    if (fail_ok)
        return false;
    va_list vl;
    va_start(vl, errformat);
    string msg = str_vformat(errformat, vl);
    va_end(vl);

    SaveReport(logger, errmsg("error", msg));
        
#if DEBUG
    if (!logger && error_count == 0)
        OLG_OnAssertFailed(__FILE__, linenum, func, "PARSE_FAIL", "");
#endif

    error_count++;
    return false;
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


bool SaveParser::skipSpace()
{
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
        else if (!cliMode && chr == '#')
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


bool SaveParser::parseIntegral(uint64* v)
{
    skipSpace();
    if (parseBinaryIntegral(v))
        return true;
    char* end = 0;
    const uint64 lval = strtoull(data, &end, 0);
    if (end <= data)
        return false;
    *v = lval;
    data = end;
    return true;
}

bool SaveParser::parseToken(const char* token)
{
    skipSpace();
    const size_t len = strlen(token);
    bool found = strncmp(data, token, len) == 0;
    if (found)
        data += len;
    return found;
}

bool SaveParser::parseToken(char token)
{
    skipSpace();
    bool found = (*data == token);
    if (found)
        data++;
    return found;
}

bool SaveParser::atToken(char token)
{
    skipSpace();
    return (*data == token);
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

bool SaveParser::parseIdent(std::string* s)
{
    skipSpace();
    if (!str_issym(*data))
        return false;

    s->clear();
    while (str_isdigit(*data) || str_issym(*data) || *data == '.')
    {
        *s += *data;
        data++;
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

bool SaveParser::parseName(std::string* s)
{
    skipSpace();
    s->clear();
    while (isprint(*data) && !str_isspace(*data) && *data != ',')
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

bool SaveParser::parseQuotedString(string *s, char quote)
{
    if (!parseToken(quote))
        return false;

    s->clear();
    while (*data != quote && *data != '\0')
    {
        if (*data == '\\')
        {
            data++;
            switch (*data) {
            case 'n':  *s += '\n'; break;
            case '\\': *s += '\\'; break;
            case '\n': break;
            default:
                if (*data != quote)
                    *s += '\\';
                *s += *data;
                break;
            }
        }
        else
        {
            *s += *data;
        }

        nextChar();
    }

    PARSE_FAIL_UNLESS(parseToken(quote), "expected closing '%c' while parsing string, got EOF", quote);
    return true;
}

bool SaveParser::parse(string* s)
{
    const bool lcl = parseToken('_');
    if (lcl) {
        PARSE_FAIL_UNLESS(parseToken('('), "Expected '(' after gettext _");
    }
    bool success = false;
    if (parseQuotedString(s, '"') || parseQuotedString(s, '\'')) {
        success = true;
    } else if (str_isalnum(*data)) {
        success = parseName(s);
    }
    if (lcl) {
        PARSE_FAIL_UNLESS(parseToken(')'), "Expected closing ')' after gettext _");
        if (success)
            *s = gettext_(s->c_str());
    }
    return success;
}

bool SaveParser::parse(lstring* s)
{
    string t;
    bool r = parse(&t);
    if (r)
        *s = lstring(t);
    return r;
}

bool SaveParser::parse(bool* v)
{
    skipSpace();
    if (*data == '1')
    {
        *v = true;
        data++;
    }
    else if (*data == '0')
    {
        *v = false;
        data++;
    }
    else
    {
        std::string s;
        PARSE_FAIL_UNLESS(parseIdent(&s), "expected 'true' or 'false'");
        s = str_tolower(s);     // ignore case
        if (s == "true")
            *v = true;
        else if (s == "false")
            *v = false;
        else PARSE_FAIL("expected 'true' or 'false' parsing bool, got %s", s.c_str());
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
    skipSpace();
    char* end = 0;
    const double val = strtod(data, &end);
    // eat msvc special nans
    // #IO #INF #SNAN #QNAN #IND
    while (*end && str_contains("#IONFSAQD", *end))
        end++;
    PARSE_FAIL_UNLESS(end > data && !fpu_error(val), "expected float");
    *v = val;
    data = end;
    return true;
}

bool SaveParser::parseE(uint64 *h, const EnumType &e)
{
    std::string s;
    if (parseIdent(&s))
    {
        const uint64 val = e.getVal(str_toupper(s));
        if (val != ~0) {
            *h |= val;
        } else {
            warn("No enum field '%s'", s.c_str());
        }
    }
    else
    {
        uint64 v = 0;
        PARSE_FAIL_UNLESS(parse(&v), "expected uint64 while parsing enum");
        *h |= v;
    }

    return parseToken('|') ? parseE(h, e) : true;
}

template <typename T>        
static bool skipDeprecated(SaveParser &sp, const char* name)
{
    T val{};
    if (!sp.parse(&val))
        return false;

    DPRINT(DEPRECATED, ("%s", sp.errmsg("warning", str_format("Ignored deprecated field '%s' : %s", name, PRETTY_TYPE(T))).c_str()));
    return true;
}


std::map<lstring, CVarBase*>& CVarBase::index()
{
    static std::map<lstring, CVarBase*> s_index;
    return s_index;
}

 std::map<lstring, pair<string, string> >& CVarBase::incomplete_index()
{
    static std::map<lstring, pair<string, string> > s_incomplete_index;
    return s_incomplete_index;
}

 std::mutex &CVarBase::mutex()
{
    static std::mutex m;
    return m;
}

CVarBase::CVarBase(lstring name, const char* comment) : m_name(name), m_comment(comment)
{
    ASSERT(!index()[name]);
    index()[name] = this;
}

CVarBase::~CVarBase()
{
    ASSERT(index()[m_name] == this);
    index().erase(m_name);
}

std::string CVarBase::get(lstring key)
{
    std::lock_guard<std::mutex> l(mutex());
    CVarBase *cv = index()[key];
    return cv ? cv->toString() : "";
}

static bool s_cvar_file_loaded = false;

void CVarBase::reportIncomplete()
{
    std::lock_guard<std::mutex> l(mutex());
    
    foreach (auto &x, incomplete_index())
    {
        Report(x.second.second.c_str());
    }

    // also write cvar file if none exists
    
    if (s_cvar_file_loaded)
    {
        Report("cvars.txt changed - not dumping");
        return;
    }

    writeToFile();
}

bool CVarBase::writeToFile()
{
    SaveSerializer ss;
    ss.o += "# -*- mode: default-generic -*-\n";
    ss.o += "# This file contains development game variables\n";
    ss.o += "# It is automatically regenerated at game startup unless overwriting would loose settings\n";
    ss.o += "# Lines beginning with a '#' are comments - remove the '#' to modify the value\n";
    ss.o += string("# Generated by Reassembly version: ") + getBuildDate() + "\n";
    ss.o += string("# ") + OL_GetPlatformDateInfo() + "\n\n";
    ss.setColumnWidth(-1);
    foreach (auto &it, index())
    {
        if (!it.second)
            continue;
        const CVarBase &cv = *it.second;
        if (cv.getComment() || cv.hasMinMax()) {
            ss.o += "### ";
            ss.o += it.first.c_str();
            ss.o += ": ";
            ss.o += cv.getTypeString();
            if (cv.getComment())
            {
                ss.o += ": ";
                ss.o += cv.getComment();
            }
            ss.o += '\n';
        }
        if (cv.isDefault())
            ss.o += "# ";
        ss.o += it.first.c_str();
        ss.insertTokens(" = ");
        cv.serialize(ss);
        ss.o += '\n';
    }

    if (ZF_SaveFileRaw("data/cvars.txt", ss.str()))
    {
        Reportf("Wrote default cvars to cvars.txt");
    }
    else
    {
        Reportf("Failed to write cvars.txt");
    }
    return true;
}

bool CVarBase::parseCVars(SaveParser &sp)
{
    std::lock_guard<std::mutex> l(mutex());
    while (!sp.isEof())
    {
        std::string s;
        if (sp.parseArg(&s))
        {
            if (sp.getFileName().size() && sp.getFileName() != "-")
                s_cvar_file_loaded = true;
            
            if (str_startswith(s, "--"))
                s = str_replace(s, "--", "k");

            const lstring name(s);
            CVarBase *cv = index()[name];

            const bool isor = sp.parseToken('|');
            const bool isandnot = !isor && sp.parseToken('&');
            sp.parseToken('='); // optional

            if (isandnot && !sp.parseToken('~'))
                sp.error("expected ~ after &=");

            if (cv == NULL) {
                string val;
                sp.parseToTokens(&val, sp.getCliMode() ? " -\n" : ";\n");
                if (!val.size())
                    return sp.error("no value for uninstantiated cvar '%s'", name.c_str());
                const string msg = sp.errfmt("warning", "uninstantiated cvar '%s'", name.c_str());
                incomplete_index()[name] = make_pair(val, msg);
            } else if (isor) {
                if (!cv->parseValueOr(sp))
                    return sp.error("failed to parse |= for cvar '%s'", name.c_str());
            } else if (isandnot) {
                if (!cv->parseValueAndNot(sp))
                    return sp.error("failed to parse &= for cvar '%s'", name.c_str());
            } else if (!cv->parseValue(sp))
                return sp.error("failed to parse cvar '%s'", name.c_str());
        }
        else
        {
            sp.error("parsing cvars from '%s'", sp.getFileName().c_str());
            ASSERT_FAILED("", "Cvar parse failed");
            break;
        }
        sp.parseToken(';'); // optional
    }
    return true;
}


std::string CVarBase::getAll()
{
    std::lock_guard<std::mutex> l(mutex());
    SaveSerializer ss;
    foreach (auto &it, index())
    {
        if (it.second)
        {
            ss.o += it.first.c_str();
            ss.insertTokens(" = ");
            ss.o += it.second->toString();
            ss.insertToken('\n');
        }
    }
    return ss.str();
}

vector<std::string> CVarBase::getAllNames()
{
    std::lock_guard<std::mutex> l(mutex());
    vector<std::string> all;
    foreach (auto &it, index())
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
    std::lock_guard<std::mutex> l(mutex());

    if (incomplete_index().count(m_name))
    {
        SaveParser sp(incomplete_index()[m_name].first);
        parseValue(sp);
        incomplete_index().erase(m_name);
        return true;
    }
    return false;
}

void loadCVars(const char* txt)
{
    if (txt) {
        SaveParser sp;
        sp.loadData(txt);
        sp.setCliMode(true);
        CVarBase::parseCVars(sp);
    }

    SaveParser sp;
    if (sp.loadFile("data/cvars.txt")) {
        CVarBase::parseCVars(sp);
    } else {
        ReportEarlyf("cvars file not found");
    }
}


#define DEF_CCV(TYPE)                                                   \
    TYPE CreateCVar_ ## TYPE(const char* name, TYPE* vptr, TYPE value, const char* comment) { \
        return (new CVar<TYPE>(name, vptr, value, comment))->getDefault();      \
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
        std::string s;
        if (CVarBase::parseCVars(sp))
        {
            SaveParser(args, true).parseIdent(&s);
            return CVarBase::get(lstring(s));
        }
    }

    // cvar query
    string query;
    if (!SaveParser::parse(args, &query, true))
        return "specify a search query";
    vector<string> names = str_split('\n', CVarBase::getAll());
    vector<string> matching;
    foreach (const string &name, names)
    {
        if (str_tolower(name).find(str_tolower(query)) != -1)
            matching.push_back(name);
    }
    return str_join('\n', matching);
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
