
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
