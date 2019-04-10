
#pragma once

#include <typeindex>
#include "ZipFile.h"

struct SaveSerializer;
struct SaveParser;
struct LispVal;
struct LispEnv;
struct Symbol;
extern int kParserMaxWarnings;

// basic io routines - abstract steam, save path, etc.

bool isSteamCloudEnabled();

// write a save file - possibly to steam cloud
bool SaveFile(const char* fname, const char* data, int size);
inline bool SaveFile(const string &fname, const string &data) {
    return SaveFile(fname.c_str(), &data[0], data.size());
}

// write a compressed save file to FNAME.gz, possibly to steam cloud
bool SaveCompressedFile(const char* fname, const char* data, int size);
inline bool SaveCompressedFile(const string &fname, const string &data) {
    return SaveCompressedFile(fname.c_str(), &data[0], data.size());
}

// load a saved file, possibly from steam cloud
string LoadFile(const char* fname);
inline string LoadFile(const string &str) { return LoadFile(str.c_str()); }

// load file without decompression
string LoadFileRaw(const char* fname);
inline string LoadFileRaw(const string &str) { return LoadFileRaw(str.c_str()); }

// recursively delete stuff
int RemoveFileOrDirectory(const char* path);
bool RemoveFile(const char *path);
int FileForget(const char *path);

// is there a file?
bool FileExists(const char* fname);
inline bool FileExists(const string &fname) { return FileExists(fname.c_str()); }

// return list of files in directory
vector<string> ListDirectory(const char* fname);
inline vector<string> ListDirectory(const string &fname) { return ListDirectory(fname.c_str()); }

// __DATE__
const char *getBuildDate();

void ReportEarly(string x);

template <typename... Args>
void ReportEarlyf(const char* fmt, const Args & ... args)
{
    ReportEarly(str_format(fmt, args...));
}

void ReportFlush();

#define PARSE_FAIL(F, ...) return fail1(__func__, __LINE__, F, ##__VA_ARGS__)
#define PARSE_FAIL_UNLESS(X, F, ...) if (!(X)) PARSE_FAIL(F, ##__VA_ARGS__)


// prevent things with implicit bool conversions from matching
struct ReallyBool {
    bool val;
    explicit ReallyBool(bool v) : val(v) {}
};

// type traits


struct ParseValidateDefault {
    struct general_ {};
    struct special_ : general_ {};
    template<typename> struct int_ { typedef int type; };

    // template <typename U>
    // static void validate1(const SaveParser &sp, U &obj, special_,
                          // typename int_<decltype(U::parseValidate)>::type = 0)
    // {
        // obj.parseValidate1();
    // }

    template <typename U>
    static void validate1(const SaveParser &sp, U &obj, special_,
                          typename int_<decltype(obj.parseValidate())>::type = 0)
    {
        obj.parseValidate();
    }

    template <typename U>
    static void validate1(const SaveParser &sp, U &obj, general_)
    {
       // do nothing
    }
};

// define a function that is called after the object finishes parsing
// the function can generate contextualized error messages
template <typename T>
struct ParseValidate {

    bool validate(const SaveParser &sp, T &obj)
    {
        // call obj.parseValidate() if it exists
        ParseValidateDefault::validate1(sp, obj, ParseValidateDefault::special_());
        return true;
    }
};

template <typename T>
struct VisitVirtualEnabled {
    // typedef int type;
    enum { value = 0 };
};

template <typename> struct void_ { typedef void type; };

template <typename T, typename = void>
struct VisitProxyEnabled { enum { value = false }; };
template <typename T>
struct VisitProxyEnabled<T, typename void_<typename T::VisitProxy>::type> { enum { value = true }; };


template <typename T, typename = void>
struct VisitAnyEnabled { enum { value = false }; };
template <typename T>
struct VisitAnyEnabled<T, typename void_<typename T::VisitEnabled>::type> { enum { value = true }; };
template <typename T>
struct VisitAnyEnabled<T, typename void_<typename T::VisitIndexedEnabled>::type> { enum { value = true }; };


template <typename T, typename = void>
struct is_default {
    bool operator()(const T &v) { return v == T(); }
};
template <typename T>
struct is_default<T, typename void_<decltype(&T::empty)>::type> {
    bool operator()(const T &v) { return v.empty(); }
};


template <typename K>
struct sref_traits { static K default_key() { return K(); } };

template <>
struct sref_traits<int> { static int default_key() { return -1; } };

// sref allows serialization of graphs by writing a "key" value instead of the whole object
// kind of a weak pointer
// sref<object, key_type>
template <typename T, typename K=decltype(T().sref_key())>
struct sref {

    typedef T ref_type;
    typedef K key_type;
    typedef std::function<T*(K)> lookup_type;

    T*                 ptr = NULL;
    K                  key = sref_traits<K>::default_key();
    const lookup_type* func = NULL;

    static K get_key(T* p) { return p ? p->sref_key() : sref_traits<K>::default_key(); }
    T* get_ref(K k) const
    {
        if (!func || k == sref_traits<K>::default_key())
            return NULL;
        T* v = (*func)(k);
        ASSERTF(v, "%s", k);
        return v;
    }
    
    sref() NOEXCEPT {}
    sref(T *t, const lookup_type *f) NOEXCEPT : ptr(t), key(get_key(t)), func(f) {}
    sref(K k, const lookup_type *f) NOEXCEPT : key(k), func(f) { if (f) ptr = get_ref(k); }
    
    sref &operator=(T* p) { ptr = p; key = get_key(p); return *this; }
    sref &operator=(K k) { key = k; ptr = get_ref(k); return *this; }

    T *resolve(const key_type &k, const lookup_type *f) { key = k; func = f; return ptr = get_ref(key); }
    T *resolve(const lookup_type *f) { func = f; return ptr = get_ref(key); }
    T *resolve() { return ptr = get_ref(key); }

    T& operator*()                     const { return *ptr; }
    T* operator->()                     const { return ptr; }
    bool empty()                       const { return ptr == NULL; }
    explicit operator bool()           const { return ptr != NULL; }
    
    bool operator==(const sref &o) const { return ptr == o.ptr; }
    bool operator==(const T* o) const { return ptr == o; }
    bool operator==(const K &o) const { return key == o; }

    bool operator!=(const sref &o) const { return ptr != o.ptr; }
    bool operator!=(const T* o) const { return ptr != o; }
    bool operator!=(const K &o) const { return key != o; }

    T*  get() const { return ptr; }
};

template <typename T, typename K=decltype(T().sref_key())>
inline sref<T, K> make_sref(T *v, const typename sref<T, K>::lookup_type* f)
{
    sref<T, K> s(v, f);
    return s;
}


enum LoadStatus {
    LS_UNKNOWN = -2,
    LS_ERROR = -1,
    LS_MISSING = 0,
    LS_OK = 1,
};



struct SaveSerializer {

    string o;

    enum Flags : uchar {
        BINARY=1<<1,
        COMPACT=1<<2,
        HUMAN=1<<3,
        LISP=1<<4,
        JSON=1<<5,
    };

protected:
    int    indent               = 0;
    int    lastnewline          = 0;
    int    lastnewlineindentend = -1;
    ushort flags                = 0;
    ushort columnWidth          = 80;

public:

    void clear()
    {
        o.clear();
        indent = 0;
        lastnewline = 0;
        lastnewlineindentend = -1;
        flags = 0;
        columnWidth = 80;
    }

    SaveSerializer() {}
    SaveSerializer(uint f) : flags(f) {}
    SaveSerializer(SaveSerializer &&ss) :
        o(std::move(ss.o)),
        lastnewline(ss.lastnewline),
        lastnewlineindentend(ss.lastnewlineindentend),
        flags(ss.flags),
        columnWidth(ss.columnWidth) {}

    static SaveSerializer& instance();
    
    // options
    SaveSerializer& setFlag(Flags flag, bool val=true);
    SaveSerializer& setColumnWidth(int v);

    template <typename T>
    static string toString(const T& v, uint flags=0)
    {
        SaveSerializer s;
        s.flags = flags;
        s.serialize(v);
        return s.str();
    }

    ~SaveSerializer(){}

    string&       str()        { return o; }
    const string& str()   const { return o; }
    const char*   c_str() const { return o.c_str(); }
    bool          empty() const { return o.empty(); }
    int           size()  const { return o.size(); }

    float prepare(float f) const;

    void comment(const string &s)
    {
        if (flags&JSON)
            return;
        o += (flags&LISP) ? ";; " : "# ";
        o += s;
        o += '\n';
    }

    template <typename T> void serialize(const copy_ptr<T>& ptr) { serialize(ptr.get()); }
    template <typename T> void serialize(const unique_ptr<T>& ptr) { serialize(ptr.get()); }
    template <typename T> void serialize(const ref_ptr<T>& ptr) { serialize(ptr.get()); }
    template <typename T, typename K>
    void serialize(const sref<T, K>& ptr) { serialize(ptr.key); }

    template <typename T>
    SaveSerializer &serialize(const SerialEnum<T> &enm)
    {
        serializeE(enm.value, T::getType());
        return *this;
    }
        
    void serializeEAll(const EnumType &e);

    template <typename T>
    void serializeKey(const T& name)
    {
        serialize(name);
        insertToken('=');
    }

    void serializeKey(const string &name) { serializeKey(name.c_str()); }
    void serializeKey(const Symbol &name);
    void serializeKey(const char* name);

    template <typename K, typename T>
    void serializeField(const K &name, const T& val, char delimit=',')
    {
        serializeKey(name);
        serialize(val);
        insertToken(delimit);
    }

    template <typename K, typename V>
    void serialize(const std::pair<K, V> &v)
    {
        insertToken('{');
        serialize(v.first);
        insertToken(',');
        serialize(v.second);
        insertToken('}');
    }

    void insertToken(char token);
    void insertTokens(const char* tksn);
    void forceInsertToken(char token);
    void insertName(const char* name);
    void insertName(const string &name) { return insertName(name.c_str()); }
    template <typename T>
    void insertName(const T& v) { serialize(v); }
    void padColumn(int width);

protected:

    void serializeE(uint64 v, const EnumType &e);

    void indent1();
    void chompForToken(int token);

    template <typename T>
    void serializeList(const T &v)
    {
        insertToken(list_paren(0));
        bool first = true;
        bool newline = false;
        foreach (const auto &x, v)
        {
            // can parse vectors of structs without commas
            if (!first)
                insertToken(',');
            if (newline)
                insertToken('\n');

            first = false;

            const int start = o.size();
            serialize(x);
            newline = !(flags&COMPACT) && columnWidth > 0 && (o.size() - start) > columnWidth/3;
        }
        insertToken(list_paren(1));
    }

public:

    template <typename T> void serialize(const vector<T> &v)   { serializeList(v); }
    template <typename T> void serialize(const copy_vec<T> &v) { serializeList(v); }
    template <typename T> void serialize(const deque<T> &v)    { serializeList(v); }
    template <typename T> void serialize(const std::set<T> &v) { serializeList(v); }
    template <typename T> void serialize(const std::unordered_set<T> &v) { serializeList(v); }
    template <typename T, size_t S> void serialize(const std::array<T, S> &v) { serializeList(v); }
    template <typename T, size_t S> void serialize(const static_vector<T, S> &v) { serializeList(v); }

    template <typename M>
    void serializeMap(const M& v)
    {
        insertToken('{');
        for (typename M::const_iterator it=v.begin(), end=v.end(); it != end; ++it)
            serializeField(it->first, it->second);
        insertToken('}');
    }
    
    template <typename K, typename V> void serialize(const std::map<K, V> &v) { serializeMap(v); }
    
    template <typename K, typename V> void serialize(const std::unordered_map<K, V> &v)
    {
        // convert to regular map so the elements don't print in random order
        std::map<K, V> m(v.begin(), v.end());
        serializeMap(m);
    }

    void serialize(float f);

    // faster than insertToken(',') and never inserts newline
    void insertComma()
    {
        o += (flags&LISP) ? " " : (flags&COMPACT) ? "," : ", ";
    }

    char list_paren(int i) const
    {
        return ((flags&LISP) ? "()" : (flags&JSON) ? "[]" : "{}")[i];
    }

    template <typename T>
    void serialize(const glm::tvec2<T> &v)
    {
        o += list_paren(0); serialize(v.x); insertComma(); serialize(v.y); o += list_paren(1);
    }

    template <typename T>
    void serialize(const glm::tvec3<T> &v)
    {
        o += list_paren(0); serialize(v.x); insertComma();
        serialize(v.y); insertComma();
        serialize(v.z); o += list_paren(1);
    }

    template <typename T>
    void serialize(const glm::tvec4<T> &v)
    {
        o += list_paren(0); serialize(v.x); insertComma();
        serialize(v.y); insertComma();
        serialize(v.z); insertComma();
        serialize(v.w); o += list_paren(1);
    }

    void serialize(const char* s);
    void serialize(const string& s) { serialize(s.c_str()); }
    void serialize(lstring st)      { serialize(st.c_str()); }
    void serialize(const Symbol &st);
    void serialize(double f) { str_append_format(o, "%.12g", f); }
    void serialize(uint i);
    void serialize(uint64 i);
    void serialize(int i)    { str_append_format(o, "%d", i); }
    void serialize(char i)   { str_append_format(o, "%d", (int)i); }
    void serialize(uchar i)  { str_append_format(o, "%d", (int)i); }
    void serialize(short i)  { str_append_format(o, "%d", (int)i); }
    void serialize(ushort i) { str_append_format(o, "%d", (int)i); }
    void serialize(ReallyBool v)   { o += (v.val ? "1" : "0"); }
    void serialize(LispVal cons, const LispEnv *heap=NULL);

    template <typename T>
    bool visitSkip(const char *name)
    {
        return true; // keep going
    }

    int visitIndex = -1;

    template <typename T>
    bool visit_isdef(const char* name, const T &val, bool isdef)
    {
        if (isdef)
        {
            visitIndex = -1;
            return true;
        }
        
        if (0 <= visitIndex) {
            visitIndex++;
            serialize(val);
            insertToken(',');
        } else {
            serializeField(name, val);
        }
        return true;            // keep going
    }
    
    template <typename T>
    bool visit(const char* name, const T &val)
    {
        return visit_isdef(name, val, is_default<T>()(val));
    }

    template <typename T>
    bool visit(const char* name, const T &val, const T& def)
    {
        return visit_isdef(name, val, (val == def));
    }

    template <typename T>
    void resetVisitIndex(const T* val, typename T::VisitEnabled=0) { visitIndex = -1; }
    template <typename T>
    void resetVisitIndex(const T* val, typename T::VisitIndexedEnabled=0) { visitIndex = 0; }
    
    template <typename T>
    void serializeVisitable(const T *val)
    {
        if (val)
        {
            resetVisitIndex(val);
            insertToken(list_paren(0));
            const_cast<T*>(val)->accept(*this);
            insertToken(list_paren(1));
        }
        else
        {
            o += (flags&JSON) ? "null" : "nil";
        }
    }

    template <typename T>
    void serialize(const T *val, typename T::VisitEnabled=0)
    {
        serializeVisitable<T>(val);
    }

    template <typename T>
    void serialize(const T *val, typename T::VisitIndexedEnabled=0)
    {
        serializeVisitable<T>(val);
    }

    template <typename T> void serialize(const ReflectionBase<T> *v)
    {
        serialize(v->name);
    }

    template <typename T> void serialize(const ReflectionProto<T> *v)
    {
        serialize(v->info);
        if (v->proto)
            serialize(v->proto.get());
    }
    
    template <typename T, typename = typename VisitVirtualEnabled<T>::type>
    void serialize(const T *val)
    {
        const ReflectionRegistry<T> &reg = ReflectionRegistry<T>::instance();
        auto it = reg.type_idx.find(std::type_index(typeid(*val)));
        ASSERT(it != reg.type_idx.end());
        if (it == reg.type_idx.end())
            return;
        const ReflectionInfo *info = it->second;
        o += info->name;
        info->serialize(*this, val);
    }

    template <typename T>
    void serialize(const T& val, typename SaveCustom<T>::type *_=NULL)
    {
        o += val.serialize();
    }
    
    template <typename T>
    void serialize(const T &val, typename T::VisitProxy* =0)
    {
        serialize(val.proxy_serialize());
    }

    template <typename T, typename = typename std::enable_if<!std::is_scalar<T>::value &&
                                                             !SaveCustom<T>::value &&
                                                             !VisitProxyEnabled<T>::value>::type >
    void serialize(const T &val)
    {
        serialize(&val);
    }

    inline friend bool SaveFile(const string &fname, const SaveSerializer &ss) {
        return SaveFile(fname, ss.o);
    }
};

struct ParserLocation {

    string fname;
    int    line = -1;
    
    string currentLine;
    int    currentLineColumn = -1;
    vector<pair<int, string>> context;
    bool isEof = false;

    mutable LogRecorder *logger = NULL;

    string format(const char* type, const string &msg, bool first=true) const;
    string shortLoc() const;
    void Report(const char* type, const string &msg) const;
};

#include "Reflection.h"
#include "Lisp.h"

struct SaveParser {

private:
    string       fname;
    string       buffer;
    const char*  start        = NULL;
    const char*  dend         = NULL;
    const char*  data         = NULL;
    uint         dataline     = 0;
    const char*  dataLastLine = NULL;
    float*       progress     = NULL;
    mutable int  error_count  = 0;
    mutable int  warn_count   = 0;
    bool         fail_ok      = false;
    bool         cliMode      = false;
    LogRecorder *logger       = NULL;

    struct ParseContext {

        enum Tag : uchar { NONE=0, INT, FUNC, STR };
        union Name {
            int             idx;
            string_gen_func func;
            const char*     str;
        };

        Tag ta=NONE, tb=NONE;
        Name a, b;

        const char* begin;      // where we begin parsing (data ptr)
        SaveParser *self;
        ParseContext *next;

        string desc() const
        {
            string s;
            s += (ta == FUNC) ? a.func() :
                 (ta == STR) ? string(a.str) : string();
            if (tb != NONE) {
                s += (tb == INT) ? str_format("[%d]", b.idx) :
                     (tb == STR) ? str_format(".%s", b.str) : string();
            }
            return s;
        }
        
        ParseContext(SaveParser *s, const char* n) :
            begin(s->data), self(s), next(s->stack), ta(STR)
        {
            a.str = n;
            self->stack = this;
        }

        ParseContext(SaveParser *s, string_gen_func n) :
            begin(s->data), self(s), next(s->stack), ta(FUNC)
        {
            a.func = n;
            self->stack = this;
        }

        ParseContext(SaveParser *s, string_gen_func n, const char *m) :
            begin(s->data), self(s), next(s->stack), ta(FUNC), tb(STR)
        {
            a.func = n;
            b.str = m;
            self->stack = this;
        }

        ParseContext(SaveParser *s, string_gen_func n, int m) :
            begin(s->data), self(s), next(s->stack), ta(FUNC), tb(INT)
        {
            a.func = n;
            b.idx = m;
            self->stack = this;
        }

        ~ParseContext()
        {
            self->stack = next;
        }
    };
    ParseContext *stack = NULL;

    bool parseBinaryIntegral(uint64 *v);
    bool parseIntegral(uint64* v);
    bool parseIntegral(long long *v);

    template <typename T>
    bool parseIntegral(T* v)
    {
        long long lval = 0;
        if (!parseIntegral(&lval))
            return false;
        PARSE_FAIL_UNLESS((lval >= std::numeric_limits<T>::min() &&
                           lval <= std::numeric_limits<T>::max()),
                          "integral constant %lld is out of range for %s", lval, TYPE_NAME(T));
        PARSE_FAIL_UNLESS(*data != '.', "Expecting integer, got float");
        *v = (T) lval;
        return true;
    }

    bool parseQuotedString(string *s, char quote, const char **after=NULL);

    template <typename T>
    bool parseScalar(T* f)
    {
        return parseFieldIndex(f, 0);
    }

    template <typename T, typename V>
    bool parseScalarVec(V *t)
    {
        T v{};
        fail_ignore i(this);
        if (!parse(&v))
            return false;
        *t = V(v);
        return true;
    }

    template <typename T>
    bool parse1arg(T *val)
    {
        return parseToken('(') && parse(val) && parseToken(')');
    }

    template <typename T>
    bool parseAngle(glm::tvec2<T> *t)
    {
        // allow deg(10) to mean a2v(kToDegrees * 10)
        if (!str_contains("rda", *data))
            return false;
        T angle = 0;
        if (parseToken("rad") || parseToken("a2v"))
        {
            if (!parse1arg(&angle))
                return false;
        }
        else if (parseToken("deg"))
        {
            if (!parse1arg(&angle))
                return false;
            angle *= kToRadians;
        }
        else
        {
            return false;
        }
        *t = a2v(angle);
        return true;
    }

    bool parseAngle(int2 *t) { return false; }

    template <typename T>
    bool parseScalar(glm::tvec2<T> *t)
    {
        return parseAngle(t) || parseScalarVec<T>(t);
    }

    template <typename T>
    bool parseScalar(glm::tvec3<T> *t) { return parseScalarVec<T>(t); }
    template <typename T>
    bool parseScalar(glm::tvec4<T> *t) { return parseScalarVec<T>(t); }

    template <typename T>
    bool parseFieldIndex(glm::tvec2<T>* v, uint i) { return (i < 2) && parse(&(*v)[i]); }
    template <typename T>
    bool parseFieldIndex(glm::tvec3<T>* v, uint i) { return (i < 3) && parse(&(*v)[i]); }
    template <typename T>
    bool parseFieldIndex(glm::tvec4<T>* v, uint i) { return (i < 4) && parse(&(*v)[i]); }

    template <typename T>
    bool parseField(glm::tvec2<T>* v, const string& s)
    {
        switch (s[0]) {
        case 'x': return parse(&v->x);
        case 'y': return parse(&v->y);
        }
        return false;
    }

    template <typename T>
    bool parseField(glm::tvec3<T>* v, const string& s)
    {
        switch (s[0]) {
        case 'x': return parse(&v->x);
        case 'y': return parse(&v->y);
        case 'z': return parse(&v->z);
        }
        return false;
    }

    template <typename T>
    bool parseField(glm::tvec4<T>* v, const string& s)
    {
        switch (s[0]) {
        case 'x': return parse(&v->x);
        case 'y': return parse(&v->y);
        case 'z': return parse(&v->z);
        case 'w': return parse(&v->w);
        }
        return false;
    }
    
    template <typename T, typename U>
    bool parseStruct(T* psb, ReflectionInfoBase<U> *)
    {
        PARSE_FAIL_UNLESS(parse(&psb->type), "expected type when parsing '%s'", PRETTY_TYPE(T));
        // TYPE{a=b, b=c} or just TYPE
        if (!parseToken('{'))
            return true;
        return parseStructContents(psb, '}');
    }

    template <typename T>
    bool parseStruct(T* v, void *)
    {
        if (parseToken('{'))
            return parseStructContents(v, '}');
        else if (parseScalar(v))
            return true;
        PARSE_FAIL("expected opening '{' for %s", PRETTY_TYPE(T));
    }

    template <typename T, typename It, typename Ins>
    bool parseList(It it, It end, Ins ins_it)
    {
        if (!parseToken('{'))
        {
            // parse just one element
            T el{};
            PARSE_FAIL_UNLESS(parse(&el), "expected opening '{' or '%s'", PRETTY_TYPE(T));
            *ins_it = std::move(el);

            if (parseToken('|'))
            {
                ++ins_it;
                return parseList<T, It, Ins>(it, end, ins_it);
            }
                
            return true;
        }
        
        int i=0;
        bool at_end = (it == end);
        while (!parseToken('}'))
        {
            T el{};
            bool parsed = parse(&el);
            PARSE_FAIL_UNLESS(parsed, "while parsing List<%s>[%d]", PRETTY_TYPE(T), i);
            if (!at_end)
            {
				*it = std::move(el);
                ++it;
                if (it == end)
                    at_end = true;
            }
            else
            {
                *ins_it = std::move(el);
                ++ins_it;
            }
            
            ++i;

            parseToken(','); // optional
        }

        return true;
    }

    template <typename T, typename Ins>
    bool parseSet(Ins ins_it)
    {
        if (!parseToken('{'))
        {
            // parse just one element
            T el{};
            PARSE_FAIL_UNLESS(parse(&el), "expected opening '{' or '%s'", PRETTY_TYPE(T));
            *ins_it = std::move(el);

            if (parseToken('|'))
            {
                ++ins_it;
                return parseSet<T, Ins>(ins_it);
            }
                
            return true;
        }
        
        int i=0;
        while (!parseToken('}'))
        {
            T el{};
            bool parsed = parse(&el);
            PARSE_FAIL_UNLESS(parsed, "while parsing List<%s>[%d]", PRETTY_TYPE(T), i);
            *ins_it = std::move(el);
            ++ins_it;
            ++i;

            parseToken(','); // optional
        }

        return true;
    }

    static void SaveReport(LogRecorder *logger, const string &msg)
    {
        if (logger)
            logger->Report(msg);
        ReportEarly(msg);
    }

    template <typename... Args>
    bool fail1(const char* func, uint line, const char* errformat, const Args & ... args) const
    {
        if (fail_ok)
            return false;

        SaveReport(logger, errfmt("error", errformat, args...));
        
#if DEBUG
        if (!logger && error_count == 0)
            OLG_OnAssertFailed(__FILE__, line, func, getCurrentLoc().shortLoc().c_str(), "error");
#endif

        error_count++;
        return false;
    }

    char nextChar();

public:

    SaveParser() ;
    ~SaveParser();

    SaveParser(const char* d, bool fok=false);
    SaveParser(const string &d, bool fok=false);

    bool loadFile(const string& fname_);
    void resetFile();
    bool loadData(const char* dta, const string &fn="-");

    bool isEof()
    {
        skipSpace();
        return !data || *data == '\0';
    }

    void setLogger(LogRecorder *l) { logger = l; }
    LogRecorder *getLogger() const { return logger; }
    
    void setCliMode(bool val) { cliMode = val; }
    bool getCliMode() const { return cliMode; }

    const char* c_str() const { return data; }
    int size() const { return buffer.size() ? buffer.size() : data ? strlen(data) : 0; }
    const std::string &getBuffer() const { return buffer; }

    // return true if data completely parses into v.
    // may write v but return false if there is trailing garbage
    template <typename S, typename T>
    static bool parse(const S &data, T *v, bool fok=false)
    {
        SaveParser p(data, fok);
        return p.parse(v) && p.isEof();
    }

    struct fail_ignore {
        bool        saved_fail_ok;
        SaveParser *self;
        fail_ignore(SaveParser *p) {
            self = p;
            saved_fail_ok = self->fail_ok;
            self->fail_ok = true;
        }
        ~fail_ignore() {
            self->fail_ok = saved_fail_ok;
        }
    };

    const string &getFileName() const { return fname; }
    void setFailOk(bool ok) { fail_ok = ok; }
    bool isFailOK() const { return fail_ok; }
    void setProgress(float *p) { progress = p; }

    ParserLocation getCurrentLoc() const;
    string errmsg(const char* type, const string& msg) const;
        
    template <typename... Args>
    string errfmt(const char* type, const char* errformat, const Args & ... args) const
    {
        return errmsg(type, str_format(errformat, args...));
    }
    
    template <typename... Args>
    void warn(const char* errformat, const Args & ... args) const
    {
        if (fail_ok || warn_count >= kParserMaxWarnings)
            return;

        SaveReport(logger, errfmt("warning", errformat, args...));

        warn_count++;
        if (warn_count == kParserMaxWarnings)
        {
            SaveReport(logger, str_format("%s: warning: ignoring future warnings (kParserMaxWarnings=%d)",
                                          fname, kParserMaxWarnings));
        }
    }
    
    template <typename... Args>
    bool error(const char* errformat, const Args & ... args) const
    {
        if (fail_ok)
            return false;

        SaveReport(logger, errfmt("error", errformat, args...));
    
        error_count++;
        return false;
    }
        
    bool skipSpace(const char** after=NULL);

    template <typename T>
    bool skip(typename std::enable_if<std::is_default_constructible<T>::value, int>::type _=0)
    {
        T v{};
        return parse(&v);
    }
    
    template <typename T>
    bool skip(typename std::enable_if<!std::is_default_constructible<T>::value, int>::type _=0)
    {
        return true;
    }

    bool parseToTokens(string *s, const char *token);
    bool parseToken(char token, const char** after=NULL);
    bool parseToken(const char* token, const char** after=NULL);
    bool atToken(char token);
    bool atToken(const char* token);
    bool parseIdent(string* s, const char** after=NULL);
    bool parseArg(string* s);
    bool parseName(string* s);
    bool parseName(lstring* s)
    {
        string str;
        if (!parseName(&str))
            return false;
        *s = lstring(str);
        return true;
    }
    template <typename T>
    bool parse(SerialEnum<T> *enm)
    {
        uint64 v = 0;
        bool success = parseE(&v, T::getType());
        PARSE_FAIL_UNLESS((v >= std::numeric_limits<typename T::value_type>::min() &&
                           v <= std::numeric_limits<typename T::value_type>::max()),
                          "integral constant %lld is out of range for %s", v, PRETTY_TYPE(T));
        enm->value = v;
        return success;
    }

    // may be overridden
    template <typename T, typename = typename std::enable_if<!std::is_pointer<T>::value && 
	                                                         !SaveCustom<T>::value &&
                                                             !VisitProxyEnabled<T>::value>::type>
    bool parse(T* v)
    {
        ParseContext pc(this, stringifier<T>());
        return parseStruct(v, v);
    }

    template <typename T> bool parse(copy_ptr<T> *ptr) { return parse(ptr->getPtr()); }
    
    template <typename T, typename K>
    bool parse(sref<T, K> *ptr)
    {
        return parse(&ptr->key);
    }
    
    template <typename T> bool parse(unique_ptr<T> *ptr)
    {
        T *v = NULL;
        if (!parse(&v))
            return false;
        *ptr = make_unique(v);
        return true;
    }
    
    template <typename T>
    bool parse(ref_ptr<T> *ptr)
    {
        bool ok = parse(ptr->getPtr());
        if (ok)
            copy_refcount(ptr->get(), +1);
        return ok;
    }
    
    template <typename T>
    bool parseNil(T **psb)
    {
        skipSpace();
        if (*data != 'n' || (!parseToken("nil") && !parseToken("null")))
            return false;
        *psb = NULL;
        return true;
    }

    template <typename T>
    bool parse(const ReflectionBase<T> **v)
    {
        string ident;
        PARSE_FAIL_UNLESS(parseIdent(&ident), "expected type name when parsing '%s'", PRETTY_TYPE(T));
        const ReflectionBase<T> *info = ReflectionRegistry<T>::instance().info(ident.c_str());
        PARSE_FAIL_UNLESS(info, "no subclass '%s' for '%s'", ident, PRETTY_TYPE(T));
        *v = info;
        return true;
    }

    template <typename T>
    bool parse(ReflectionProto<T> *v)
    {
        PARSE_FAIL_UNLESS(parse(&v->info), "while parsing '%s' prototype", PRETTY_TYPE(T));
        if (!atToken('{'))     // optional data
            return true;
        T* proto = (T*)v->info->create();
        v->proto = make_unique(proto);
        return v->info->parse(*this, (void**)&proto);
    }

    template <typename T>
    bool parse(T* val, typename SaveCustom<T>::type *_=NULL)
    {
        skipSpace();
        int c = val->parse(data);
        PARSE_FAIL_UNLESS(c != 0, "expected '%s'", PRETTY_TYPE(T));
        data += c;
        return true;
    }

    template <typename T>
    bool parse(T* val, typename T::VisitProxy prx = typename T::VisitProxy())
    {
        PARSE_FAIL_UNLESS(parse(&prx), "while parsing '%s' proxy", PRETTY_TYPE(T));
        PARSE_FAIL_UNLESS(val->proxy_parse(std::move(prx)), "proxy parse failed");
        return true;
    }
    
    template <typename T>
    bool parse(T** psb, typename VisitVirtualEnabled<T>::type=0)
    {
        if (parseNil(psb))
            return true;
        string ident;
        PARSE_FAIL_UNLESS(parseIdent(&ident), "expected subclass '%s'", PRETTY_TYPE(T));
        const ReflectionBase<T> *info = ReflectionRegistry<T>::instance().info(ident);
        PARSE_FAIL_UNLESS(info, "expected subclass of '%s', found '%s'", PRETTY_TYPE(T), ident);
        if (!atToken('{')) {      // optional parens/params
            *psb = info->create();
            return true;
        }
        return info->parse(*this, (void**)psb);
    }
    
    template <typename T, typename = typename std::enable_if<std::is_default_constructible<T>::value &&
                                                            !VisitVirtualEnabled<T>::value>::type>
    bool parse(T** psb)
    {
        if (parseNil(psb))
            return true;
        if (!*psb)
            *psb = new T();
        ParseContext pc(this, stringifier<T>());
        return parseStruct(*psb, *psb);
    }

    template <typename T> bool parse(vector<T>* v) { return parseList<T>(v->begin(), v->end(), back_inserter(*v)); }
    template <typename T> bool parse(copy_vec<T>* v) { return parseList<T*>(v->begin(), v->end(), back_inserter(v->vec)); }
    template <typename T> bool parse(deque<T>* v) { return parseList<T>(v->begin(), v->end(), back_inserter(*v)); }
    template <typename T> bool parse(std::set<T>* v) { return parseSet<T>(inserter(*v, v->end())); }
    template <typename T> bool parse(std::unordered_set<T>* v) { return parseSet<T>(inserter(*v, v->end())); }
    template <typename K, typename V> bool parse(std::map<K, V>* mp) { return parseMap(mp); }
    template <typename K, typename V> bool parse(std::unordered_map<K, V>* mp) { return parseMap(mp); }

    template <typename T, size_t S> bool parse(std::array<T, S> *v)
    {
        ParseContext pc(this, "array");
        PARSE_FAIL_UNLESS(parseToken('{'), "expected opening '{' for array<%s, %d>", PRETTY_TYPE(T), (int)S);

        for (int i=0; i<S; i++)
        {
            PARSE_FAIL_UNLESS(parse(&(*v)[i]), "while parsing array<%s, %d>[%d]", PRETTY_TYPE(T), (int)S, i);
            parseToken(','); // optional            
        }

        PARSE_FAIL_UNLESS(parseToken('}'), "expected closing '}' for array<%s, %d>", PRETTY_TYPE(T), (int)S);

        return true;
    }

    template <typename T, size_t S> bool parse(static_vector<T, S> *v)
    {
        vector<T> vec;
        parse(&vec);
        PARSE_FAIL_UNLESS(vec.size() <= S, "Too many elemets for static_vector<%s, %d>", PRETTY_TYPE(T), S);
        for_ (it, vec)
            v->push_back(move(it));
        return true;
    }

    template <typename K, typename V>
    bool parse(std::pair<K, V> *v)
    {
        PARSE_FAIL_UNLESS(parseToken('{'), "expected opening '{' for pair<%s, %s>", PRETTY_TYPE(K), PRETTY_TYPE(V));

        PARSE_FAIL_UNLESS(parse(&v->first), "while parsing pair<%s, %s> key", PRETTY_TYPE(K), PRETTY_TYPE(V));
        parseToken(','); // optional
        PARSE_FAIL_UNLESS(parse(&v->second), "while parsing pair<%s, %s> value", PRETTY_TYPE(K), PRETTY_TYPE(V));

        PARSE_FAIL_UNLESS(parseToken('}'), "expected '}' for pair<%s, %s>", PRETTY_TYPE(K), PRETTY_TYPE(V));
        
        return true;
    }

private:

    bool parseKey(string *v);
    bool parseKey(Symbol *s);
    
    template <typename T>
    bool parseKey(T *v)
    {
        if (!parse(v))
            return false;
        parseToken('=');        // optional
        return true;
    }

    template <typename T>
    bool parseMap(T* mp)
    {
        ParseContext pc(this, stringifier<T>());
        PARSE_FAIL_UNLESS(parseToken('{'), "expected opening '{' for %s", PRETTY_TYPE(T));

        while (!parseToken('}'))
        {
            std::pair<typename T::key_type, typename T::mapped_type> p;
            PARSE_FAIL_UNLESS(parseKey(&p.first), "while parsing %s key", PRETTY_TYPE(T));
            PARSE_FAIL_UNLESS(parse(&p.second), "while parsing %s value", PRETTY_TYPE(T));
            // (*mp)[std::move(p.first)] = std::move(p.second);
            mp->insert(std::move(p));
            parseToken(','); // optional
        }
        
        return true;
    }

    // opening bracket already parsed
    template <typename T>
    bool parseStructContents(T* sb, char terminator)
    {
        int i=0;
        std::string s;
        while (!parseToken(terminator))
        {
            if (parseKey(&s)) {
                ParseContext pc(this, stringifier<T>(), s.c_str());
                PARSE_FAIL_UNLESS(parseField(sb, s), "while parsing %s::%s", PRETTY_TYPE(T), s);
                i = -1;
            } else if (i == -1) {
                PARSE_FAIL("expected field name while parsing %s", PRETTY_TYPE(T));
            } else {
                ParseContext pc(this, stringifier<T>(), i);
                if (parseFieldIndex(sb, i))
                    i++;
                else
                    i = -1;
            }
        
            parseToken(','); // optional
        }

        PARSE_FAIL_UNLESS(ParseValidate<T>().validate(*this, *sb), "while validating %s", PRETTY_TYPE(T));

        return true;
    }

    template <typename T>
    bool parseField(T *val, const string& s)
    {
        DynamicObj dy = ReflectionLayout::instance<T>()(val, s);
        PARSE_FAIL_UNLESS(dy.type, "no field %s.'%s'", PRETTY_TYPE(T), s);
        if (dy.self) {
            PARSE_FAIL_UNLESS(dy.parse(*this), "while parsing %s.%s", PRETTY_TYPE(T), s);
        } else {
            PARSE_FAIL_UNLESS(dy.skip(*this), "while skipping %s.%s", PRETTY_TYPE(T), s);
        }
        return true;
    }

    template <typename T>
    bool parseFieldIndex(T *val, uint index /*, typename T::VisitIndexedEnabled=0 */)
    {
        PARSE_FAIL_UNLESS(ReflectionLayout::instance<T>()(val, index).parse(*this) == 1,
                          "while parsing %s[%d]", PRETTY_TYPE(T), index);
        return true;
    }

    template <typename T>
    bool parseFieldIndex(T* val, ...)
    {
        return false;
    }

public:
    bool parse(string* s);
    bool parse(Symbol* s);
    bool parse(lstring* s);
    bool parse(bool* v);
    bool parse(uint* v) { return parseIntegral(v); }
    bool parse(uint64* v) { return parseIntegral(v); }
    bool parse(int* v) { return parseIntegral(v); }
    bool parse(char* v) { return parseIntegral(v); }
    bool parse(uchar* v) { return parseIntegral(v); }
    bool parse(short* v) { return parseIntegral(v); }
    bool parse(ushort* v) { return parseIntegral(v); }
    bool parse(float* v);
    bool parse(double* v);
    bool parseE(uint64 *h, const EnumType &e);
    bool parse(LispVal *cons, LispEnv *heap=NULL);
};


template <typename T>
void ReflectionDispatch<T>::serialize(SaveSerializer &s, void *ptr) const { s.serialize(* (T*) ptr); }
template <typename T>
bool ReflectionDispatch<T>::parse(SaveParser &p, void *ptr) const { return p.parse((T*) ptr); }
template <typename T>
bool ReflectionDispatch<T>::skip(SaveParser &p) const { return p.skip<T>(); }
template <typename T>
LispVal ReflectionDispatch<T>::read(const void *ptr) const
{
    return ptr ? lisp_box(*(const T*)ptr) : LispVal(LT_ERROR);
}
template <typename T>
bool ReflectionDispatch<T>::write(void *ptr, LispVal v, const LispEnv *e) const { return lisp_unbox(v, (T*)ptr, e); }

template <>
inline void ReflectionDispatch<ReflectionNull>::serialize(SaveSerializer &s, void *ptr) const { }
template <>
inline bool ReflectionDispatch<ReflectionNull>::parse(SaveParser &p, void *ptr) const
{
    return p.error("Error Type");
}
template <>
inline bool ReflectionDispatch<ReflectionNull>::skip(SaveParser &p) const { return false; }
template <>
inline LispVal ReflectionDispatch<ReflectionNull>::read(const void *ptr) const
{
    return LispVal(LT_ERROR);
}
template <>
inline bool ReflectionDispatch<ReflectionNull>::write(void *ptr, LispVal v, const LispEnv *e) const { return false; }

template <typename T>
string SerialEnum<T>::toString() const
{
    return SaveSerializer::toString(*this);
}

inline string fmt_attempt(string msg, bool status, const string &errmsg)
{
    if (errmsg.size())
        msg += " !!! " + errmsg;
    msg.append(max(0, 75 - (int)msg.size()), ' ');
    msg += str_format(" [%s]", status ? "  OK  " : "FAILED");
    return msg;
}

template <typename T>
LoadStatus loadFileAndParse(const string& fname, T* data, SaveParser p=SaveParser())
{
    LoadStatus status = LS_OK;
    string errmsg;
    
    if (!p.loadFile(fname)) {
        status = LS_MISSING;
        errmsg = "File Missing";
    } else if (!p.parse(data)) {
        status = LS_ERROR;
        errmsg = "Parse Error";
    } else if (!p.isEof()) {
        errmsg = "Garbage at EOF";
    }

    ReportEarly(fmt_attempt(str_format("* Load %s // %s // %s", fname,
                                       str_bytes_format(p.size()), PRETTY_TYPE(T)),
                            (status == LS_OK), errmsg));
    
    return status;
}

template <typename T>
T loadFileAndParse(const string &fname)
{
    T dat;
    loadFileAndParse(fname, &dat);
    return dat;
}

template <typename T>
LoadStatus loadDataAndParse(const string &text, T* data)
{
    SaveParser p;
    if (text.empty() || !p.loadData(text.c_str()))
        return LS_MISSING;
    else if (!p.parse(data))
        return LS_ERROR;
    return LS_OK;
}

// write data to LOCAL file (not steam cloud)
template <typename T>
bool serializeToFile(const string &fname, const T& val)
{
    SaveSerializer ss;
    ss.serialize(val);
    return ZF_SaveFileRaw(fname.c_str(), ss.str());
}

template <typename T> void SaveSerializer_serialize(SaveSerializer &s, const void *dat)
{
    s.serialize((const T*)dat);
}

template <typename T> bool SaveParser_parse(SaveParser &p, void **dat)
{
    return p.parse((T**)dat);
}


template <typename T>
vector<string> comp_enum(void* data, const char* name, const char *args)
{
    vector<string> opts;
    foreach (const EnumType::Elem &se, T::getType().elems)
        opts.push_back(se.first.str());
    return opts;
}

template <typename T> vector<string> comp_t(SerialEnum<T> *) { return comp_enum<T>(NULL, NULL, NULL); }
template <typename T> vector<string> comp_t(T*) { return vector<string>(); }

struct WidgetBase;

struct CVarBase {

protected:
    const lstring m_name;
    const string  m_comment;
    bool          m_has_minmax = false;
    bool          m_save_value = true;
    const lstring m_file;

    bool initialize();

    template <typename T, typename U>
    bool parseValueOr1(SaveParser &sp, SerialEnum<T> *val)
    {
        SerialEnum<T> val1;
        if (sp.parse(&val1)) {
            *val |= val1;
            ReportEarlyf("CVAR %s = %s (via |=)", getName(), toString());
            return true;
        }
        return false;
    }

    template <typename T>
    bool parseValueOr1(SaveParser &sp, const T& val) { return false; }

    template <typename T>
    bool parseValueAndNot1(SaveParser &sp, SerialEnum<T> *val)
    {
        SerialEnum<T> val1;
        if (sp.parse(&val1)) {
            *val &= ~val1;
            ReportEarlyf("CVAR %s = %s (via &~)", getName(), toString());
            return true;
        }
        return false;
    }

    template <typename T>
    bool parseValueAndNot1(SaveParser &sp, const T& val) { return false; }

    virtual bool parseValue(SaveParser &sp) = 0;
    virtual bool parseValue(SaveParser &sp, string ext) = 0;
    virtual bool parseValueOr(SaveParser &sp) = 0;
    virtual bool parseValueAndNot(SaveParser &sp) = 0;

public:

    CVarBase(lstring name, const char* comment);
    CVarBase(lstring name, const char* comment, const char *file);
    virtual ~CVarBase();

    lstring getName()        const { return m_name; }
    lstring getFile()        const { return m_file; }
    const string &getComment() const { return m_comment; }
    bool hasMinMax()         const { return m_has_minmax; }
    void save(bool save)           { m_save_value = save; }
    bool save()              const { return m_save_value; }

    virtual const void*    getVptr() const = 0;
    virtual bool           isDefault() const = 0;
    virtual std::string    toString() const = 0;
    virtual std::string    getTypeString() const = 0;
    virtual void           serialize(SaveSerializer &ss) const = 0;
    virtual vector<string> comp() const = 0;
    virtual DynamicObj     dyobj() = 0;
    
    static vector<std::string> comp(lstring key);
    static std::string get(lstring key);
    static bool        parseCVars(SaveParser &sp, bool save_value=true);
    static void        onClose();
    static bool        writeToFile();
    static std::string getAll();
    static std::string getMatching(const std::string &query);
    static vector<std::string> getAllNames();
    static pair<CVarBase*, DynamicObj> get_cvar(const char* key);
    static CVarBase   *get_cvar(void *value);
};

template <typename T>
struct CVar final : public CVarBase {

    T* const m_vptr;
    const T m_def, m_min, m_max;

    CVar(lstring name, T* vptr, const T& def, const char* file, const char* comment) :
        CVarBase(name, comment, file), m_vptr(vptr), m_def(def), m_min(), m_max()
    {
        initialize();
    }

    CVar(lstring name, T* vptr, const T& def, const char *file, const char* comment, const T& mn, const T& mx) :
        CVarBase(name, comment, file), m_vptr(vptr), m_def(def), m_min(mn), m_max(mx)
    {
        m_has_minmax = true;
        initialize();
    }

    const T getDefault() const { return m_def; }

    bool parseValue(SaveParser &sp) override
    {
        if (sp.parse(m_vptr)) {
            const string val = toString();
            if (!checkMinMax()) {
                sp.warn("%s: %s: value %s clamped to %s", m_name,
                        getTypeString(), val, toString());
            }
            ReportEarlyf("CVAR %s = %s", getName(), toString());
            return true;
        }
        return false;
    }

    bool parseValue(SaveParser &sp, string ext) override
    {
        if (ReflectionLayout::instance<T>()(m_vptr, ext).parse(sp)) {
            const string val = toString();
            if (!checkMinMax()) {
                sp.warn("%s: %s: value %s clamped to %s", m_name,
                        getTypeString(), val, toString());
            }
            ReportEarlyf("CVAR %s = %s", getName(), toString());
            return true;
        }
        return false;
    }
    
    std::string getTypeString() const override
    {
        std::string s = PRETTY_TYPE(T);
        if (hasMinMax()) {
            s += ", ";
            s += SaveSerializer::toString(m_min);
            s += ", ";
            s += SaveSerializer::toString(m_max);
        }
        return s;
    }
    
    const void *getVptr()              const override { return m_vptr; }
    bool isDefault()                   const override { return *m_vptr == m_def; }
    bool parseValueOr(SaveParser &sp)        override { return parseValueOr1(sp, m_vptr); }
    bool parseValueAndNot(SaveParser &sp)    override { return parseValueAndNot1(sp, m_vptr); }
    std::string toString()             const override { return SaveSerializer::toString((T) *m_vptr); }
    vector<std::string> comp()         const override { return comp_t((T*) NULL); }
    void serialize(SaveSerializer &ss) const override { ss.serialize((T) *m_vptr); }
    DynamicObj dyobj()                       override { return DynamicObj(m_vptr); }
        
    bool checkMinMax() { return true; }
};


template <typename T>
WidgetBase *get_edit_widget(T *ptr) { return NULL; }
WidgetBase *get_edit_widget(float *ptr);
WidgetBase *get_edit_widget(double *ptr);
WidgetBase *get_edit_widget(int *ptr);
WidgetBase *get_edit_widget(uint *ptr);
WidgetBase *get_edit_widget(bool *ptr);
WidgetBase *get_edit_widget(string *ptr);
WidgetBase *get_edit_widget(Symbol *ptr);

WidgetBase *label_widget(WidgetBase *wi, string label);

template <typename T>
WidgetBase* ReflectionDispatch<T>::edit_widget(void *ptr) const
{
    return get_edit_widget((T*) ptr);
}

template <typename T>
bool parsePtr(SaveParser &sp, T** psb)
{
    *psb = new T();
    return true;
}

template <typename T>
T CreateCVar(lstring name, T* vptr, const char *file, const T& value, const char* comment="")
{
    return (new CVar<T>(name, vptr, value, file, comment))->getDefault();
}

template <typename T>
T CreateCVar(lstring name, T* vptr, const char *file, const T& value, const char* comment, const T& mn, const T& mx)
{
    DASSERT(clamp(value, mn, mx) == value);
    CVar<T> *cv = new CVar<T>(name, vptr, value, file, comment, mn, mx);
    return cv->getDefault();
}

#define DEFINE_CVAR1(TYPE, NAME, ...)                     \
    TYPE NAME = CreateCVar< TYPE >(#NAME, &NAME, __FILE__, __VA_ARGS__)
    
#define CREATE_CVAR(NAME, VALUE) CreateCVar(#NAME, &(NAME), __FILE__, (VALUE))

void loadCVars(const char* txt);
string cmd_cvar(void* data, const char* name, const char *args);
string cmd_save_cvars(void* data, const char* name, const char *args);

inline vector<string> comp_cvars(void* data, const char* name, const char *args)
{
    std::string s;
    vector<string> c;
    if (SaveParser(args, true).parseIdent(&s) && (c = CVarBase::comp(s)).size())
    {
        for_ (x, c)
            x = s + " = " + x;
        return c;
    }
        
    return CVarBase::getAllNames();
}

typedef SaveSerializer SSerializer;
typedef SaveParser SParser;


struct AutoSave {

private:
    void*                 self = NULL;
    std::function<bool()> write_fun;
    string                name;
    int                   write_count = 0;
    
    template <typename T>
    bool write_1()
    {
        bool success = SaveFile(name, SSerializer::toString((T*)self));
        DPRINT(SAVE, "Autosaved '%s': %s", name, success ? "OK" : "FAILED");
        write_count++;
        return success;
    }
public:
    
    AutoSave(const string &n) : name("data/save/" + n + ".lua") { }

    template <typename T>
    bool read(T *val)
    {
        self = val;
        write_fun = [this] { return write_1<T>(); };
        return loadFileAndParse(name, val) == LS_OK;
    }

    bool write() { return write_fun && write_fun(); }

    // can't actually write here because self might have destructed
    ~AutoSave() { ASSERT(write_count > 0); }
};

#define AUTOSAVE(S) AutoSave S = AutoSave(str_demangle(typeid(*this).name()))


// override to get a call after loading
template <typename T> void onCacheLoad(T &v) { }

template <typename T>
struct LoaderCache {

    std::map<string, T*> data;

    ~LoaderCache()
    {
        for_ (it, data)
            delete it.second;
    }

    static T* get(const string &file)
    {
        static LoaderCache l;
        auto it = l.data.find(file);
        if (it == l.data.end())
        {
            T *v = new T();
            if (loadFileAndParse(file, v)) {
                onCacheLoad(*v);
                it = l.data.insert(make_pair(file, v)).first;
            } else {
                delete v;
            }
        }
        return (it != l.data.end()) ? it->second : NULL;
    }
};

