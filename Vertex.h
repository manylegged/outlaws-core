
//
// Vertex.h - vertex formats for use with Mesh in Graphics.h
// 

// Copyright (c) 2013 Arthur Danskin
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

#ifndef VERTEX_H
#define VERTEX_H

struct VertexPos {
    float3 pos;
};

struct VertexPosTex {
    float3 pos;
    float2 texCoord;
};

struct VertexPosColorTex {
    float3 pos;
    uint   color;
    float2 texCoord;
};

struct VertexPosColor {
    float3 pos;
    uint   color;

    VertexPosColor() {}
    VertexPosColor(float2 p, uint c) : pos(p, 0.f), color(c) {}

//    void setColor(uint c, float a=1)   { color = argb2abgr(c|0xff000000, a); }
//    void setColor32(uint c, float a=1) { color = argb2abgr(c, a); }
};

struct VertexPosColorLuma {
    float3 pos;
    uint   color;
    float  luma{1.f};          // scales color

    VertexPosColorLuma() {}
    VertexPosColorLuma(float2 p, uint c, float l=1.f) : pos(float3(p.x, p.y, 0.f)), color(c), luma(l) {}
    VertexPosColorLuma(float3 p, uint c, float l=1.f) : pos(p), color(c), luma(l) {}
};

struct VertexPos2ColorTime {
    float3 pos;
    uint   color;
    uint   color1;
    float  time;

    void setColor(uint c, uint c1, float a=1)
    {
        color = argb2abgr(c|0xff000000, a);
        color1 = argb2abgr(c1|0xff000000, a);
    }
    void setColor32(uint c, float a, uint c1, float a1)
    {
        color = argb2abgr(c, a); 
        color1 = argb2abgr(c1|0xff000000, a1);
    }
};

struct VertexPosTime {
    float3 pos;
    float  time;
    
};



#endif // VERTEX_H
