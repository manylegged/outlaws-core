
//
// Shaders.h - some basic GLSL shaders
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

#ifndef SHADERS_H
#define SHADERS_H

#include "Graphics.h"


struct ShaderPosBase: public ShaderProgramBase {

    virtual void UseProgram(const ShaderState& ss, const float2* ptr) const = 0;


    // a b
    // c d
    void DrawQuad(const ShaderState& ss, float2 a, float2 b, float2 c, float2 d) const
    {
        const float2 v[] = { a, b, c, d };
        static const ushort i[] = {0, 1, 2, 1, 3, 2};

        UseProgram(ss, &v[0]);
        ss.DrawElements(GL_TRIANGLES, 6, i);
        UnuseProgram();
    }

    void DrawSquare(const ShaderState &ss, float r) const
    {
        DrawQuad(ss, float2(-r, -r), float2(-r, r), float2(r, -r), float2(r, r));
    }

    void DrawRect(const ShaderState &ss, float2 r) const
    {
        DrawQuad(ss, float2(-r.x, -r.y), float2(-r.x, r.y), float2(r.x, -r.y), r);
    }

    void DrawRect(const ShaderState &ss, float2 pos, float2 r) const
    {
        DrawQuad(ss, pos + float2(-r.x, -r.y), pos + float2(-r.x, r.y), 
                 pos + float2(r.x, -r.y), pos + r);
    }

    void DrawColorRect(ShaderState& ss, uint color, float2 r) const
    {
        if (color&ALPHA_OPAQUE) {
            ss.color32(color);
            DrawRect(ss, r);
        }
    }

    void DrawRectCorners(const ShaderState &ss, float2 a, float2 b) const
    {
        float2 bl(min(a.x, b.x), min(a.y, b.y));
        float2 ur(max(a.x, b.x), max(a.y, b.y));
        DrawQuad(ss, float2(bl.x, ur.y), ur, bl, float2(ur.x, bl.y));
    }

    void DrawColorRectCorners(ShaderState &ss, uint color, float2 a, float2 b) const
    {
        if (color&ALPHA_OPAQUE) {
            ss.color32(color);
            float2 bl(min(a.x, b.x), min(a.y, b.y));
            float2 ur(max(a.x, b.x), max(a.y, b.y));
            DrawQuad(ss, float2(bl.x, ur.y), ur, bl, float2(ur.x, bl.y));
        }
    }

    void DrawLineRect(const ShaderState &ss, float2 pos, float2 rad) const
    {
        DrawLineRectCorners(ss, pos - rad, pos + rad);
    }

    void DrawColorLineRect(ShaderState& ss, uint color, float2 r) const
    {
        if (color&ALPHA_OPAQUE) {
            ss.color32(color);
            DrawLineRect(ss, r);
        }
    }

    // a b
    // c d
    void DrawLineQuad(const ShaderState& ss, float2 a, float2 b, float2 c, float2 d, 
                      bool outline=true, bool cross=false) const
    {
        const float2        v[] = { a, b, c, d };
        static const ushort i[] = {0, 1, 1, 3, 3, 2, 2, 0, 0, 3, 1, 2};

        UseProgram(ss, v);
        ss.DrawElements(GL_LINES, (outline ? 8 : 0) + (cross ? 4 : 0), outline ? i : (i + 8));
        UnuseProgram();
    }

    void DrawLineSquare(const ShaderState& ss, float r) const
    {
        DrawLineQuad(ss, float2(-r, -r), float2(-r, r), float2(r, -r), float2(r, r));
    }

    void DrawLineRect(const ShaderState& ss, float2 r) const
    {
        DrawLineQuad(ss, float2(-r.x, -r.y), float2(-r.x, r.y), float2(r.x, -r.y), r);
    }

    void DrawLineRectCorners(const ShaderState& ss, float2 a, float2 b) const
    {
        float2 bl(min(a.x, b.x), min(a.y, b.y));
        float2 ur(max(a.x, b.x), max(a.y, b.y));
        DrawLineQuad(ss, float2(bl.x, ur.y), ur, bl, float2(ur.x, bl.y));
    }

    void DrawLine(const ShaderState& ss, float2 a, float2 b) const
    {
        float2 v[] = { a, b };
        UseProgram(ss, v);
        ss.DrawArrays(GL_LINES, 2);
        UnuseProgram();
    }

    void DrawLineTri(const ShaderState& ss, float2 a, float2 b, float2 c) const
    {
        float2 v[] = { a, b, c };
        static const ushort i[] = {0, 1, 1, 2, 2, 0};
        UseProgram(ss, v);
        ss.DrawElements(GL_LINES, 6, i);
        UnuseProgram();
    }


    void DrawTri(const ShaderState& ss, float2 a, float2 b, float2 c) const
    {
        float2 v[] = { a, b, c };
        UseProgram(ss, v);
        ss.DrawArrays(GL_TRIANGLES, 3);
        UnuseProgram();
    }

    void DrawGrid(const ShaderState &ss, double width, double3 first, double3 last) const;

    void DrawLineCircle(const ShaderState& ss, float radius, int numVerts=32) const;
    void DrawCircle(const ShaderState& ss, float radius, int numVerts=32) const;
};

struct ShaderUColor : public ShaderPosBase {

private:
    uint m_colorSlot;
        
    ShaderUColor()
    {
        LoadProgram("ShaderUColor",
                    "varying vec4 DestinationColor;\n"
                    , 
                    "uniform vec4 SourceColor;\n"
                    "void main(void) {\n"
                    "    DestinationColor = SourceColor;\n"
                    "    gl_Position = Transform * Position;\n"
                    "}\n"
                    ,
                    "void main(void) {\n"
                    "    gl_FragColor = DestinationColor;\n"
                    "}\n"
            );
        m_colorSlot = getUniformLocation("SourceColor");
    }
public:

    void UseProgram(const ShaderState& ss, const float2* ptr) const
    {
        UseProgramBase(ss, ptr, (float2*)NULL);
        float4 c = ABGR2RGBAf(ss.uColor);
        glUniform4fv(m_colorSlot, 1, &c[0]);
        glReportError();
    }

    void UseProgram(const ShaderState& ss, const VertexPos* ptr, const VertexPos* base) const
    {
        UseProgramBase(ss, &ptr->pos, base);
        float4 c = ABGR2RGBAf(ss.uColor);
        glUniform4fv(m_colorSlot, 1, &c[0]);
        glReportError();
    }

    static const ShaderUColor& instance()   
    {
        static ShaderUColor* p = new ShaderUColor();
        return *p;
    }
    
};

struct ShaderIridescent : public ShaderProgramBase {

private:
    int SourceColor0;
    int SourceColor1;
    int TimeU;
    int TimeA;
    
    ShaderIridescent();

public:

    void UseProgram(const ShaderState& input, const VertexPos2ColorTime* ptr, const VertexPos2ColorTime* base) const
    {
        UseProgramBase(input, &ptr->pos, base);
        vertexAttribPointer(SourceColor0, &ptr->color, base);
        vertexAttribPointer(SourceColor1, &ptr->color1, base);
        vertexAttribPointer(TimeA, &ptr->time, base);
        glUniform1f(TimeU, globals.renderTime);
    }

    static const ShaderIridescent& instance()   
    {
        static ShaderIridescent* p = new ShaderIridescent();
        return *p;
    }

};

struct ShaderUIridescent : public ShaderProgramBase {

private:
    int SourceColor0;
    int SourceColor1;
    int TimeU;
    int TimeA;
    
    ShaderUIridescent()
    {
        LoadProgram("ShaderUIridescent",
                    "varying vec4 DestinationColor;\n"
                    ,
                    "uniform vec4 SourceColor0;\n"
                    "uniform vec4 SourceColor1;\n"
                    "uniform float TimeU;\n"
                    "attribute float TimeA;\n"
                    "void main(void) {\n"
                    "    gl_Position = Transform * Position;\n"
                    "    float val = 0.5 + 0.5 * sin(TimeU + TimeA);\n"
                    "    DestinationColor = mix(SourceColor0, SourceColor1, val);\n"
                    "}\n"
                    ,
                    "void main(void) {\n"
                    "    gl_FragColor = DestinationColor;\n"
                    "}\n"
            );
        GET_UNIF_LOC(SourceColor0);
        GET_UNIF_LOC(SourceColor1);
        GET_UNIF_LOC(TimeU);
        GET_ATTR_LOC(TimeA);
    }

public:

    void UseProgram(const ShaderState& ss, const VertexPosTime* ptr, const VertexPosTime* base) const
    {
        UseProgramBase(ss, &ptr->pos, base);
        float4 c0 = ABGR2RGBAf(ss.uColor);
        float4 c1 = ABGR2RGBAf(ss.uColor1);
        glUniform4fv(SourceColor0, 1, &c0[0]);
        glUniform4fv(SourceColor1, 1, &c1[0]);
        glUniform1f(TimeU, globals.renderTime);
        vertexAttribPointer(TimeA, &ptr->time, base);
    }

    static const ShaderUIridescent& instance()   
    {
        static ShaderUIridescent* p = new ShaderUIridescent();
        return *p;
    }

};



struct ShaderTextureBase: public ShaderProgramBase {

    virtual void UseProgram(const ShaderState &ss, const VertexPosTex *ptr, const OutlawTexture& ot) const = 0;

    void BindTexture(const OutlawTexture& texture, int slot) const
    {
        ASSERT(texture.width && texture.texnum);
        glActiveTexture(GL_TEXTURE0 + slot);
        glBindTexture(GL_TEXTURE_2D, texture.texnum);
    }

    // a b
    // c d
    void DrawQuad(const ShaderState& ss, const OutlawTexture& texture, float2 scale, 
                  float2 a, float2 b, float2 c, float2 d) const;

    void DrawRectCorners(const ShaderState &ss, const OutlawTexture& texture, float2 a, float2 b) const
    {
        float2 bl(min(a.x, b.x), min(a.y, b.y));
        float2 ur(max(a.x, b.x), max(a.y, b.y));
        DrawQuad(ss, texture, float2(1.f), float2(bl.x, ur.y), ur, bl, float2(ur.x, bl.y));
    }

    void DrawRectCornersUpsideDown(const ShaderState &ss, const OutlawTexture& texture, float2 a, float2 b) const
    {
        // OpenGL texture coordinate origin is in the lower left.
        // but all my image loaders put the origin in the upper left
        // so flip it

        float2 bl(min(a.x, b.x), min(a.y, b.y));
        float2 ur(max(a.x, b.x), max(a.y, b.y));
        DrawQuad(ss, texture, float2(1.f), bl, float2(ur.x, bl.y), float2(bl.x, ur.y), ur);
    }

    void DrawRect(const ShaderState &ss, const OutlawTexture& texture, float2 pos, float2 rad) const
    {
        DrawRectCorners(ss, texture, pos - rad, pos + rad);
    }

    void DrawRectScale(const ShaderState &ss, const OutlawTexture& texture, float2 scale, float2 pos, float2 rad) const
    {
        DrawQuad(ss, texture, scale, pos + flipX(rad), pos + rad, pos - rad, pos + flipY(rad));
    }

    void DrawRectWidth(const ShaderState &ss, const OutlawTexture& texture, float2 pos, float width) const
    {
        float2 rad(width, width * texture.height / texture.width);
        DrawRect(ss, texture, pos, rad);
    }

    void DrawRectUpsideDown(const ShaderState &ss, const OutlawTexture& texture, float2 pos, float2 rad) const
    {
        DrawRectCornersUpsideDown(ss, texture, pos - rad, pos + rad);
    }

    void DrawButton(const ShaderState &ss, const OutlawTexture& texture, float2 pos, float2 r) const;
};

struct ShaderTexture : public ShaderTextureBase {

private:
    uint m_uTexture;
    uint m_uColorSlot;
    uint m_aTexCoords;

public:
    ShaderTexture()
    {
        LoadProgram("ShaderTexture",
                    "varying vec2 DestTexCoord;\n"
                    "varying vec4 DestColor;\n"
                    ,
                    "attribute vec2 SourceTexCoord;\n"
                    "uniform vec4 SourceColor;\n"
                    "void main(void) {\n"
                    "    DestTexCoord = SourceTexCoord;\n"
                    "    DestColor    = SourceColor;\n"
                    "    gl_Position  = Transform * Position;\n"
                    "}\n"
                    ,
                    "uniform sampler2D texture1;\n"
                    "void main(void) {\n"
                    "    gl_FragColor = DestColor * texture2D(texture1, DestTexCoord);\n"
                    "}\n"
                    );
        m_uTexture = getUniformLocation("texture1");
        m_uColorSlot = getUniformLocation("SourceColor");
        m_aTexCoords = getAttribLocation("SourceTexCoord");
    }

    void UseProgram(const ShaderState &ss, const VertexPosTex *ptr, const OutlawTexture &ot) const override
    {
        UseProgram(ss, ptr, (const VertexPosTex*) NULL);
    }

    template <typename Vtx>
    void UseProgram(const ShaderState &ss, const Vtx *ptr, const Vtx *base) const
    {
        UseProgramBase(ss, &ptr->pos, base);

        vertexAttribPointer(m_aTexCoords, &ptr->texCoord, base);
        glUniform1i(m_uTexture, 0);

        float4 c = ABGR2RGBAf(ss.uColor);
        glUniform4fv(m_uColorSlot, 1, &c[0]); 
    }

    static const ShaderTexture& instance()   
    {
        static ShaderTexture* p = new ShaderTexture();
        return *p;
    }

};

struct ShaderTextureHSV : public ShaderTextureBase {

private:
    uint m_uTexture;
    uint m_uColorSlot;
    uint m_aTexCoords;

public:
    ShaderTextureHSV()
    {
        LoadProgram("ShaderTextureHSV");
        m_uTexture = getUniformLocation("texture1");
        m_uColorSlot = getUniformLocation("SourceColor");
        m_aTexCoords = getAttribLocation("SourceTexCoord");
    }

    void UseProgram(const ShaderState &ss, const VertexPosTex *ptr, const OutlawTexture &ot) const override
    {
        UseProgram(ss, ptr, (const VertexPosTex*) NULL);
    }

    template <typename Vtx>
    void UseProgram(const ShaderState &ss, const Vtx *ptr, const Vtx *base) const
    {
        UseProgramBase(ss, &ptr->pos, base);

        vertexAttribPointer(m_aTexCoords, &ptr->texCoord, base);
        glUniform1i(m_uTexture, 0);

        float4 c = ABGR2RGBAf(ss.uColor);
        glUniform4fv(m_uColorSlot, 1, &c[0]); 
    }

    static const ShaderTextureHSV& instance()   
    {
        static ShaderTextureHSV* p = new ShaderTextureHSV();
        return *p;
    }

};

struct ShaderTonemapDither : public ShaderTextureBase {

private:
    uint texture1;
    uint dithertex;
    uint SourceTexCoord;

public:
    ShaderTonemapDither();

    void UseProgram(const ShaderState &ss, const VertexPosTex *ptr, const OutlawTexture &ot) const;

    static const ShaderTonemapDither& instance()
    {
        static ShaderTonemapDither* p = new ShaderTonemapDither();
        return *p;
    }

};

struct ShaderTonemap : public ShaderTextureBase {

private:
    uint texture1;
    uint SourceTexCoord;

public:
    ShaderTonemap();

    void UseProgram(const ShaderState &ss, const VertexPosTex *ptr, const OutlawTexture &ot) const;

    static const ShaderTonemap& instance()   
    {
        static ShaderTonemap* p = new ShaderTonemap();
        return *p;
    }

};

// seperable gaussian blur
struct ShaderBlur : public ShaderTextureBase {

private:
    uint usourceTex;
    uint asourceTexCoord;
    uint ucoefficients;
    uint uoffsets;
    
    float  coefficients[5];
    mutable float2 offsets[5];
    mutable uint dimension = 0;

    ShaderBlur();
public:

    void UseProgram(const ShaderState &ss, const VertexPosTex *ptr, const OutlawTexture &ot) const;

    void setDimension(uint dim) const 
    {
        dimension = dim; 
    }

    static const ShaderBlur& instance()   
    {
        static ShaderBlur* p = new ShaderBlur();
        return *p;
    }
};

struct ShaderColor : public ShaderProgramBase {

private:
    int m_colorSlot;

    ShaderColor()
    {
        LoadProgram("ShaderColor",
                    //"#extension GL_EXT_gpu_shader4 : require\n"
                    //"flat varying vec4 DestinationColor;\n"
                    "varying vec4 DestinationColor;\n"
                    ,
                    "attribute vec4 SourceColor;\n"
                    "void main(void) {\n"
                    "    DestinationColor = SourceColor;\n"
                    "    gl_Position = Transform * Position;\n"
                    "}\n"
                    ,
                    "void main(void) {\n"
                    "    gl_FragColor = DestinationColor;\n"
                    "}\n"
                    );
        m_colorSlot = getAttribLocation("SourceColor");
    }

public:

    template <typename Vtx>
    void UseProgram(const ShaderState&ss, const Vtx* ptr, const Vtx* base) const
    {
        UseProgramBase(ss, &ptr->pos, base);
        vertexAttribPointer(m_colorSlot, &ptr->color, base);
    }

    static const ShaderColor& instance()   
    {
        static ShaderColor* p = new ShaderColor();
        return *p;
    }

};

struct ShaderColorDither : public ShaderProgramBase {

private:
    int SourceColor, dithertex;

    ShaderColorDither()
    {
        LoadProgram("ShaderColorDither");
        GET_ATTR_LOC(SourceColor);
        GET_UNIF_LOC(dithertex);
    }

public:

    template <typename Vtx>
    void UseProgram(const ShaderState&ss, const Vtx* ptr, const Vtx* base) const
    {
        UseProgramBase(ss, &ptr->pos, base);
        vertexAttribPointer(SourceColor, &ptr->color, base);
        getDitherTex().BindTexture(0);
        glUniform1i(dithertex, 0);
    }

    static const ShaderColorDither& instance()   
    {
        static ShaderColorDither* p = new ShaderColorDither();
        return *p;
    }

};

struct ShaderHsv : public ShaderProgramBase {

private:
    int m_colorSlot;

    ShaderHsv()
    {
        LoadProgram("ShaderHsv");
        m_colorSlot = getAttribLocation("ColorHSVA");
    }

public:

    template <typename Vtx>
    void UseProgram(const ShaderState&ss, const Vtx* ptr, const Vtx* base) const
    {
        UseProgramBase(ss, &ptr->pos, base);
        vertexAttribPointer(m_colorSlot, &ptr->color, base);
    }

    static const ShaderHsv& instance()
    {
        static ShaderHsv* p = new ShaderHsv();
        return *p;
    }

};

struct ShaderColorLuma : public ShaderProgramBase {

private:
    int SourceColor, Luma;

    ShaderColorLuma();

public:

    template <typename Vtx>
    void UseProgram(const ShaderState&ss, const Vtx* ptr, const Vtx* base) const
    {
        UseProgramBase(ss, &ptr->pos, base);
        vertexAttribPointer(SourceColor, &ptr->color, base);
        vertexAttribPointer(Luma, &ptr->luma, base);
    }

    static const ShaderColorLuma& instance()   
    {
        static ShaderColorLuma* p = new ShaderColorLuma();
        return *p;
    }

};

struct ShaderSmoothColor : public ShaderProgramBase {

private:
    int m_colorSlot;

    ShaderSmoothColor()
    {
        LoadProgram("ShaderSmoothColor",
                    "varying vec4 DestinationColor;\n"
                    ,
                    "attribute vec4 SourceColor;\n"
                    "void main(void) {\n"
                    "    DestinationColor = SourceColor;\n"
                    "    gl_Position = Transform * Position;\n"
                    "}\n"
                    ,
                    "void main(void) {\n"
                    "    gl_FragColor = DestinationColor;\n"
                    "}\n"
                    );
        m_colorSlot = getAttribLocation("SourceColor");
    }

public:

    template <typename Vtx>
    void UseProgram(const ShaderState&ss, const Vtx* ptr, const Vtx* base) const
    {
        UseProgramBase(ss, &ptr->pos, base);
        vertexAttribPointer(m_colorSlot, &ptr->color, base);
    }

    static const ShaderSmoothColor& instance()   
    {
        static ShaderSmoothColor* p = new ShaderSmoothColor();
        return *p;
    }
};

struct ShaderNoise : public ShaderProgramBase {
    
    uint m_colorSlot;
    uint m_uRandomSeed;
        
    ShaderNoise()
    {
        LoadProgram("ShaderNoise",
                    "varying vec4 DestinationColor;\n"
                    , 
                    "attribute vec4 SourceColor;\n"
                    "void main(void) {\n"
                    "    DestinationColor = SourceColor;\n"
                    "    gl_Position = Transform * Position;\n"
                    "}\n"
                    ,
                    "uniform float RandomSeed;\n"
                    "float rand(vec2 co){\n"
                    "    return fract(sin(dot(co.xy ,vec2(12.9898,78.233))) * 43758.5453);\n"
                    "}\n"
                    "void main(void) {\n"
                    "    gl_FragColor = DestinationColor * (0.5 + rand(RandomSeed * floor(gl_FragCoord.xy / 4.)));\n"
                    "}\n"
                    );
        m_colorSlot = getAttribLocation("SourceColor");
        m_uRandomSeed = getUniformLocation("RandomSeed");
    }

    void UseProgram(const ShaderState &ss, const VertexPosColor* ptr, const VertexPosColor* base) const
    {
        UseProgramBase(ss, &ptr->pos, base);
        vertexAttribPointer(m_colorSlot, &ptr->color, base);
        glUniform1f(m_uRandomSeed, fmod(globals.renderTime, 1.0));
    }
};

#endif // SHADERS_H
