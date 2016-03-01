
//
// SerialCore.h - automatically serializable enum class
// 

// Copyright (c) 2014-2016 Arthur Danskin
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

#ifndef SERIALCORE_H
#define SERIALCORE_H

/////////////////////////////////////////////////////////////////////////////////
// Reflective enum macros
// Easily declare enums that serialize and deserialize, list members at runtime, etc.
//
// For example:
//
// #define BLOCK_SHAPES(F)                         \
//    F(SQUARE,   0)                               \
//    F(OCTAGON,  1)                               \
//
// DEFINE_ENUM(SerialShape, char, BLOCK_SHAPES);
//

#define DEFINE_ENUM_OP(X)                                          \
    U operator X(U val) const { return value X val; }    \
    U operator X##=(U val) { return value X##= val; }   \
    U operator X(SerialEnum val) const { return value X val.value; } \
    U operator X##=(SerialEnum val) { return value X##= val.value; }

typedef std::pair<lstring, uint64> SaveEnum;

template <typename T, typename U=uint64>
struct SerialEnum : public T {

    SerialEnum() {}
    SerialEnum(const U &init) : value(init) {}
    SerialEnum(const SerialEnum &o) : value(o.value) {}
    
    U value = 0;
    
    DEFINE_ENUM_OP(&)
    DEFINE_ENUM_OP(|)
    DEFINE_ENUM_OP(^)

    U operator==(U o) const { return value == o; }
    U operator==(SerialEnum o) const { return value == o.value; }
    U operator!=(U o) const { return value != o; }
    U operator!=(SerialEnum o) const { return value != o.value; }
    bool operator<(SerialEnum o) const { return value < o.value; }
    U operator~() const { return ~value; }
    SerialEnum& operator=(U o) { value = o; return *this; }
    SerialEnum& operator=(SerialEnum o) { value = o.value; return *this; }
    explicit operator bool() const { return bool(value); }

    U get() const { return value; }
    typename T::Fields enm() const { return (typename T::Fields) value; }
    string toString() const;

    U set(U bits, bool val) { return setBits<U>(value, bits, val); }
    friend U setBits(SerialEnum &en, U bits, bool val) { return setBits<U>(en.value, bits, val); }

    bool has(U bits) const { return hasBits<U>(value, bits); }
    friend bool hasBits(const SerialEnum &en, U bits) { return hasBits<U>(en.value, bits); }
    
    static uint getBitUnion()
    {
        static typename T::Type uni = 0;
        if (uni == 0) {
            for (const SaveEnum *se=T::getFields(); se->first; se++)
                uni |= se->second;
        }
        return uni;
    }

    static uint getBitCount()
    {
        return 1 + findLeadingOne(getBitUnion());
    }

    static uint getCount()
    {
        static typename T::Type mx = 0;
        if (mx == 0) {
            for (const SaveEnum *se=T::getFields(); se->first; se++)
                mx = max(mx, (typename T::Type)se->second);
            mx++;
        }
        return mx;
    }
};

#undef DEFINE_ENUM_OP

#define SERIAL_TO_SAVEENUM(K, V) SaveEnum(#K, (V)),
#define SERIAL_TO_ENUM(X, V) X=(V),

#define DEFINE_ENUM(TYPE, NAME, FIELDS)                                 \
    struct NAME ## _ {                                                  \
        typedef TYPE Type;                                              \
        enum Fields : TYPE { ZERO=0, FIELDS(SERIAL_TO_ENUM) };            \
        static const SaveEnum *getFields()                              \
        {                                                               \
            static const SaveEnum val[] = { FIELDS(SERIAL_TO_SAVEENUM) { NULL, 0}  }; \
            return val;                                                 \
        }                                                               \
    };                                                                  \
    typedef SerialEnum< NAME ## _, TYPE > NAME

#define DECLARE_ENUM(TYPE, NAME) struct NAME ## _; typedef SerialEnum< NAME ## _, TYPE > NAME


/////////////////////////////////////////////////////////////////////////////////
// Reflective struct macros
// Easily declare structs that automatically serialize and deserialize, etc
//
// For example:
//
// #define SERIAL_BLOCK_FIELDS(F)                                       \
//     F(uint,              ident,                   0)                 \
//     F(float2,            offset,                  float2(0))         \
//     F(float,             angle,                   0.f)               \
//
// DECLARE_SERIAL_STRUCT(SerialBlock, SERIAL_BLOCK_FIELDS);
//
// (in cpp file)
// DEFINE_SERIAL_STRUCT(SerialBlock, SERIAL_BLOCK_FIELDS);
//

// for use in accept definitions
#define VISIT(FIELD) visit(#FIELD, (FIELD))
#define VISIT_DEF(FIELD, DEF) visit(#FIELD, (FIELD), (DEF))
#define VISIT_SKIP(TYPE, NAME) template visitSkip< TYPE >(NAME)

// internal serial use
#define SERIAL_TO_STRUCT_FIELD(TYPE, NAME, DEFAULT) TYPE NAME = DEFAULT;
#define SERIAL_INITIALIZE_FIELD(_TYPE, NAME, DEFAULT) NAME = (DEFAULT);
#define SERIAL_COPY_FIELD(TYPE, NAME, ...) NAME = sb.NAME;
#define SERIAL_MOVE_FIELD(TYPE, NAME, ...) NAME = std::move(sb.NAME);
#define SERIAL_ELSE_FIELD_NEQUAL(_TYPE, NAME, ...) else if ((this->NAME) != (sb.NAME)) return 0;

#define SERIAL_VISIT_FIELD_AND(TYPE, NAME, DEFAULT) vis.VISIT_DEF(NAME, TYPE(DEFAULT)) &&
#define SERIAL_SKIP_FIELD_AND(TYPE, NAME, DEFAULT) vis.VISIT_SKIP(TYPE, #NAME) &&

#define SERIAL_TO_STRUCT_FIELD2(TYPE, NAME) TYPE NAME = TYPE();
#define SERIAL_VISIT_FIELD_AND2(TYPE, NAME) vis.visit(#NAME, (NAME)) &&

#define SERIAL_PLACEHOLDER(_F)

template <typename T>
struct GetFieldVisitor {
    T*           value = NULL;
    const string field;

    GetFieldVisitor(const string &name) : field(name) {}

    bool visit(const char* name, T& val, const T& def=T())
    {
        if (name != field)
            return true;
        value = &val;
        return false;
    }

    template <typename U>
    bool visit(const char* name, U& val, const U& def=U())
    {
        DASSERT(name != field);
        return true;
    }
    
    template <typename U>
    bool visitSkip(const char *name) { return true; }
};

// get a reference to a field in OBJ named FIELD (the same as getattr in Python).
// Return NULL if not found or type is wrong
template <typename U, typename T>
U* getField(T& obj, const string &field)
{
    GetFieldVisitor<U> vs(field);
    obj.accept(vs);
    return vs.value;
}

template <typename U, typename T>
const U* getField(const T& obj, const string &field)
{
    return getField<U>(const_cast<T&>(obj), field);
}

#define DECLARE_SERIAL_STRUCT_OPS(STRUCT_NAME)                          \
    STRUCT_NAME(const STRUCT_NAME& o) { *this = o; }                    \
    STRUCT_NAME(STRUCT_NAME&& o) NOEXCEPT { *this = std::move(o); }     \
    static const STRUCT_NAME &getDefault();                             \
    bool operator==(const STRUCT_NAME& sb) const;                       \
    STRUCT_NAME& operator=(const STRUCT_NAME& sb);                      \
    STRUCT_NAME& operator=(STRUCT_NAME&& sb) NOEXCEPT
    

#define DECLARE_SERIAL_STRUCT_CONTENTS(STRUCT_NAME, FIELDS_MACRO, IGNORE_MACRO) \
    FIELDS_MACRO(SERIAL_TO_STRUCT_FIELD)                                \
    STRUCT_NAME();                                                      \
    ~STRUCT_NAME();                                                     \
    template <typename V>                                               \
    bool accept(V& vis)                                                 \
    {                                                                   \
        return FIELDS_MACRO(SERIAL_VISIT_FIELD_AND)                     \
            IGNORE_MACRO(SERIAL_SKIP_FIELD_AND)                         \
            true;                                                       \
    }                                                                   \
    typedef int VisitEnabled;                                           \
    DECLARE_SERIAL_STRUCT_OPS(STRUCT_NAME)
    

#define DECLARE_SERIAL_STRUCT(STRUCT_NAME, FIELDS_MACRO)                \
    struct STRUCT_NAME {                                                \
        DECLARE_SERIAL_STRUCT_CONTENTS(STRUCT_NAME, FIELDS_MACRO, SERIAL_PLACEHOLDER); \
    }


#define DEFINE_SERIAL_STRUCT_OPS(STRUCT_NAME, FIELDS_MACRO)     \
    bool STRUCT_NAME::operator==(const STRUCT_NAME& sb) const   \
    {                                                           \
        if (0); FIELDS_MACRO(SERIAL_ELSE_FIELD_NEQUAL) else return 1;   \
    }                                                           \
    STRUCT_NAME& STRUCT_NAME::operator=(const STRUCT_NAME& sb)    \
    {                                                           \
        FIELDS_MACRO(SERIAL_COPY_FIELD);                        \
        return *this;                                           \
    }                                                           \
    STRUCT_NAME& STRUCT_NAME::operator=(STRUCT_NAME&& sb) NOEXCEPT  \
    {                                                           \
        FIELDS_MACRO(SERIAL_MOVE_FIELD);                        \
        return *this;                                           \
    }                                                           \
    const STRUCT_NAME &STRUCT_NAME::getDefault()                \
    {                                                           \
        static const STRUCT_NAME val;                           \
        return val;                                             \
    }


#define DEFINE_SERIAL_STRUCT(STRUCT_NAME, FIELDS_MACRO)         \
    STRUCT_NAME::STRUCT_NAME() {}                               \
    STRUCT_NAME::~STRUCT_NAME() {}                              \
    DEFINE_SERIAL_STRUCT_OPS(STRUCT_NAME, FIELDS_MACRO)

#define DECLARE_DEFINE_SERIAL_STRUCT(STRUCT_NAME, FIELDS_MACRO) \
    DECLARE_SERIAL_STRUCT(STRUCT_NAME, FIELDS_MACRO);           \
    DEFINE_SERIAL_STRUCT(STRUCT_NAME, FIELDS_MACRO)             \

#endif
