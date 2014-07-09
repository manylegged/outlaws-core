
//
// SerialCore.h - automatically serializable enum class
// 

// Copyright (c) 2014 Arthur Danskin
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

template <typename T, typename U=uint64>
struct SerialEnum : public T {

    SerialEnum() {}
    SerialEnum(U init) : value(init) {}
    
    U value = 0;
    
    DEFINE_ENUM_OP(&)
    DEFINE_ENUM_OP(|)
    DEFINE_ENUM_OP(^)

    U operator==(U o) const { return value == o; }
    U operator==(SerialEnum o) const { return value == o.value; }
    U operator!=(U o) const { return value != o; }
    U operator!=(SerialEnum o) const { return value != o.value; }
    SerialEnum& operator=(U o) { value = o; return *this; }
    SerialEnum& operator=(SerialEnum o) { value = o.value; return *this; }
    explicit operator bool() const { return bool(value); }

    U get() const { return value; }
    string toString() const;
};

#undef DEFINE_ENUM_OP

typedef std::pair<lstring, uint64> SaveEnum;
#define SERIAL_TO_SAVEENUM(K, V) SaveEnum(#K, V),
#define SERIAL_TO_ENUM(X, V) X=V,

#define DEFINE_ENUM(TYPE, NAME, FIELDS)                                 \
    struct NAME ## _ {                                                  \
        enum Fields : TYPE { FIELDS(SERIAL_TO_ENUM) };              \
        static const SaveEnum *getFields()                              \
        {                                                               \
            static const SaveEnum val[] = { FIELDS(SERIAL_TO_SAVEENUM) { NULL, 0}  }; \
            return val;                                                 \
        }                                                               \
    };                                                                  \
    typedef SerialEnum< NAME ## _, TYPE > NAME


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

#define SERIAL_TO_STRUCT_FIELD(TYPE, NAME, DEFAULT) TYPE NAME = DEFAULT;
#define SERIAL_VISIT_FIELD_AND(TYPE, NAME, DEFAULT) vis.visit(#NAME, (NAME), TYPE(DEFAULT)) &&
#define SERIAL_INITIALIZE_FIELD(_TYPE, NAME, DEFAULT) NAME = (DEFAULT);
#define SERIAL_COPY_FIELD(TYPE, NAME, _DEFAULT) NAME = sb.NAME;
#define SERIAL_ELSE_FIELD_NEQUAL(_TYPE, NAME, _DEFAULT) else if ((this->NAME) != (sb.NAME)) return 0;
#define VISIT(FIELD) visit(#FIELD, (FIELD))
#define VISIT_DEF(FIELD, DEF) visit(#FIELD, (FIELD), (DEF))

#define DECLARE_SERIAL_STRUCT_CONTENTS(STRUCT_NAME, FIELDS_MACRO)   \
    FIELDS_MACRO(SERIAL_TO_STRUCT_FIELD);                           \
    STRUCT_NAME() {}                                                \
    STRUCT_NAME(const STRUCT_NAME& o) { *this = o; }                \
    template <typename V>                                           \
    bool accept(V& vis)                                             \
    {                                                               \
        return FIELDS_MACRO(SERIAL_VISIT_FIELD_AND) true;       \
    }                                                               \
    static const STRUCT_NAME &getDefault();                         \
    bool operator==(const STRUCT_NAME& sb) const;                   \
    STRUCT_NAME& operator=(const STRUCT_NAME& sb);                  \
    typedef int VisitEnabled;                                       \



#define DECLARE_SERIAL_STRUCT(STRUCT_NAME, FIELDS_MACRO)            \
    struct STRUCT_NAME {                                            \
        DECLARE_SERIAL_STRUCT_CONTENTS(STRUCT_NAME, FIELDS_MACRO);  \
    }                                                               \



#define DEFINE_SERIAL_STRUCT(STRUCT_NAME, FIELDS_MACRO)         \
    bool STRUCT_NAME::operator==(const STRUCT_NAME& sb) const   \
    {                                                           \
        if (0); FIELDS_MACRO(SERIAL_ELSE_FIELD_NEQUAL) else return 1;   \
    }                                                           \
    STRUCT_NAME& STRUCT_NAME::operator=(const STRUCT_NAME& sb)    \
    {                                                           \
        FIELDS_MACRO(SERIAL_COPY_FIELD);                        \
        return *this;                                           \
    }                                                           \
    const STRUCT_NAME &STRUCT_NAME::getDefault()                \
    {                                                           \
        static const STRUCT_NAME val;                           \
        return val;                                             \
    }


#endif
