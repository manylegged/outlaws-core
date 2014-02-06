
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

template <typename T>
struct SerialEnum {

    typedef T Type;
    
    SerialEnum() {}
    SerialEnum(uint64 init) : value(init) {}
    
    uint64 value = 0;

    uint64 operator&(uint64 val) const { return value&val; }
    uint64 operator|(uint64 val) const { return value|val; }
    uint64 operator^(uint64 val) const { return value^val; }

    uint64 operator&=(uint64 val) { return value &= val; }
    uint64 operator|=(uint64 val) { return value |= val; }
    uint64 operator^=(uint64 val) { return value ^= val; }

    uint64 operator==(uint64 o) const { return value == o; }
    uint64 operator==(SerialEnum o) const { return value == o.value; }
    uint64 operator!=(uint64 o) const { return value != o; }
    uint64 operator!=(SerialEnum o) const { return value != o.value; }
    SerialEnum& operator=(uint64 o) { value = o; return *this; }
    SerialEnum& operator=(SerialEnum o) { value = o.value; return *this; }

    uint64 get() const { return value; }
    string toString() const;
};



#endif
