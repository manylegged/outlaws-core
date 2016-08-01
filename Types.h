
// some definitions to allow parsing Reassembly data files

#ifndef RTYPES_H
#define RTYPES_H

#include "SerialCore.h"
#include "Save.h"

struct Block {
    uint ident;
    float2 offset;
    float angle;

    int bindingId = 0;

    typedef int VisitIndexedEnabled;
    
    template <typename V>
    bool accept(V& vis)
    {
        return vis.VISIT(ident) && vis.VISIT(offset) && vis.VISIT(angle) &&
            vis.VISIT(bindingId);
    }
};

enum WeaponFireType { WF_DEFAULT=0, WF_FIRE_ALL=1, WF_RIPPLE_FIRE=2 };
typedef std::array<uchar, 4> uchar4;

#define SERIAL_SHIP_DATA(F)                                   \
    F(string, name, NULL)                                     \
    F(string, author, NULL)                                   \
    F(uint,    color0, 0)                                      \
    F(uint,    color1, 0)                                      \
    F(uint,    color2, 0)                                      \
    F(uchar4,  wgroup, uchar4())                               \

struct ShipData {
    DECLARE_SERIAL_STRUCT_CONTENTS(ShipData, SERIAL_SHIP_DATA, SERIAL_PLACEHOLDER);
    int3 getColors() const { return int3(color0, color1, color2); }
    void setColors(int3 v) { color0 = v.x; color1 = v.y; color2 = v.z; }
};

struct BlockCluster {
    ShipData        data;
    vector<Block *> blocks;

    template <typename V>
    bool accept(V& vis)
    {
        return vis.VISIT(data) && vis.VISIT(blocks);
    }

    void loadFromFrom(const char* filename)
    {
        loadFileAndParse(filename, this);
    }
};



#endif // RTYPES_H
