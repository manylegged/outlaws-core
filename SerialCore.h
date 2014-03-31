
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


typedef pair<lstring, uint64> SaveEnum;
#define TO_SAVEENUM(K, V) SaveEnum(#K, V),
#define TO_ENUM(X, V) X=V,
#define DEFINE_ENUM(NAME, FIELDS)                                       \
    struct NAME {                                                       \
        enum Fields : uint64 { FIELDS(TO_ENUM) };                                \
        static const SaveEnum *getFields()                              \
        {                                                               \
            static const SaveEnum val[] = { FIELDS(TO_SAVEENUM) { NULL, 0}  }; \
            return val;                                                 \
        }                                                               \
    }

#define DEFINE_ENUM_OP(X)                                          \
    uint64 operator X(uint64 val) const { return value X val; }    \
    uint64 operator X##=(uint64 val) { return value X##= val; }   \
    uint64 operator X(SerialEnum val) const { return value X val.value; } \
    uint64 operator X##=(SerialEnum val) { return value X##= val.value; }

template <typename T>
struct SerialEnum {

    typedef T Type;
    
    SerialEnum() {}
    SerialEnum(uint64 init) : value(init) {}
    
    uint64 value = 0;
    
    DEFINE_ENUM_OP(&)
    DEFINE_ENUM_OP(|)
    DEFINE_ENUM_OP(^)

    uint64 operator==(uint64 o) const { return value == o; }
    uint64 operator==(SerialEnum o) const { return value == o.value; }
    uint64 operator!=(uint64 o) const { return value != o; }
    uint64 operator!=(SerialEnum o) const { return value != o.value; }
    SerialEnum& operator=(uint64 o) { value = o; return *this; }
    SerialEnum& operator=(SerialEnum o) { value = o.value; return *this; }
    explicit operator bool() const { return bool(value); }

    uint64 get() const { return value; }
    string toString() const;
};


#endif
