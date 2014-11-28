
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

struct ShaderPosBase : public ShaderProgramBase {

    virtual void UseProgram(const ShaderState& ss, const float2* ptr) const = 0;
    virtual ~ShaderPosBase() {}


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

struct ShaderUColor : public ShaderPosBase, public ShaderBase<ShaderUColor> {

    uint m_colorSlot;
        
    void LoadTheProgram()
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
    void UseProgram(const ShaderState& ss, const float2* ptr) const
    {
        UseProgramBase(ss, ptr, (float2*)NULL);
        float4 c = abgr2rgbaf(ss.uColor);
        glUniform4fv(m_colorSlot, 1, &c[0]);
        glReportError();
    }

    void UseProgram(const ShaderState& ss, const VertexPos* ptr, const VertexPos* base) const
    {
        UseProgramBase(ss, &ptr->pos, base);
        float4 c = abgr2rgbaf(ss.uColor);
        glUniform4fv(m_colorSlot, 1, &c[0]);
        glReportError();
    }
};

struct ShaderIridescent : public ShaderProgramBase, public ShaderBase<ShaderIridescent> {

    int SourceColor0;
    int SourceColor1;
    int TimeU;
    int TimeA;

    void LoadTheProgram()
    {
        LoadProgram("ShaderIridescent");
        GET_ATTR_LOC(SourceColor0);
        GET_ATTR_LOC(SourceColor1);
        GET_ATTR_LOC(TimeA);
        GET_UNIF_LOC(TimeU);
    }

    void UseProgram(const ShaderState& input, const VertexPos2ColorTime* ptr, const VertexPos2ColorTime* base) const
    {
        UseProgramBase(input, &ptr->pos, base);
        vertexAttribPointer(SourceColor0, &ptr->color, base);
        vertexAttribPointer(SourceColor1, &ptr->color1, base);
        vertexAttribPointer(TimeA, &ptr->time, base);
        glUniform1f(TimeU, globals.renderTime);
    }
};

struct ShaderResource : public ShaderProgramBase, public ShaderBase<ShaderResource> {

    int SourceColor0, SourceColor1, Radius;
    int ToPixels, Time;
    
    void LoadTheProgram()
    {
        LoadProgram("ShaderResource");
        GET_ATTR_LOC(SourceColor0);
        GET_ATTR_LOC(SourceColor1);
        GET_ATTR_LOC(Radius);
        GET_UNIF_LOC(ToPixels);
        GET_UNIF_LOC(Time);
    }

    mutable float pointsToPixels = 1.f;

    void UseProgram(const ShaderState& input, const VertexPos2ColorTime* ptr, const VertexPos2ColorTime* base) const
    {
        UseProgramBase(input, &ptr->pos, base);
        vertexAttribPointer(SourceColor0, &ptr->color, base);
        vertexAttribPointer(SourceColor1, &ptr->color1, base);
        vertexAttribPointer(Radius, &ptr->time, base);
        glUniform1f(ToPixels, pointsToPixels);
        glUniform1f(Time, globals.renderTime);
    }
};

struct ShaderWormhole : public ShaderProgramBase, public ShaderBase<ShaderWormhole> {

    int SourceColor0, SourceColor1, TexCoord;
    int Time;
    
    void LoadTheProgram()
    {
        LoadProgram("ShaderWormhole");
        GET_ATTR_LOC(SourceColor0);
        GET_ATTR_LOC(SourceColor1);
        GET_ATTR_LOC(TexCoord);
        GET_UNIF_LOC(Time);
    }

    void UseProgram(const ShaderState& input, const VertexPos2ColorTex* ptr, const VertexPos2ColorTex* base) const
    {
        UseProgramBase(input, &ptr->pos, base);
        vertexAttribPointer(SourceColor0, &ptr->color, base);
        vertexAttribPointer(SourceColor1, &ptr->color1, base);
        vertexAttribPointer(TexCoord, &ptr->tex, base);
        glUniform1f(Time, globals.renderTime);
    }
};


struct ShaderTextureBase : public ShaderProgramBase {

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

struct ShaderTexture : public ShaderTextureBase, public ShaderBase<ShaderTexture> {

    uint m_uTexture;
    uint m_uColorSlot;
    uint m_aTexCoords;

    void LoadTheProgram()
    {
        LoadProgram("ShaderTexture");
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

        vertexAttribPointer(m_aTexCoords, &ptr->tex, base);
        glUniform1i(m_uTexture, 0);

        float4 c = abgr2rgbaf(ss.uColor);
        glUniform4fv(m_uColorSlot, 1, &c[0]); 
    }
};

struct ShaderTextureWarp : public ShaderTextureBase, public ShaderBase<ShaderTextureWarp> {

    GLuint texture1, warpTex, SourceColor, SourceTexCoord, camWorldPos, camWorldSize, time;

    mutable float2 camPos;
    mutable float2 camSize;
    
    void LoadTheProgram()
    { 
        LoadProgram("ShaderTextureWarp");
        GET_UNIF_LOC(texture1);
        GET_UNIF_LOC(warpTex);
        GET_UNIF_LOC(SourceColor);
        GET_UNIF_LOC(camWorldPos);
        GET_UNIF_LOC(camWorldSize);
        GET_UNIF_LOC(time);
        GET_ATTR_LOC(SourceTexCoord);
    }

    void UseProgram(const ShaderState &ss, const VertexPosTex *ptr, const OutlawTexture &ot) const override
    {
        UseProgram(ss, ptr, (const VertexPosTex*) NULL);
    }

    template <typename Vtx>
    void UseProgram(const ShaderState &ss, const Vtx *ptr, const Vtx *base) const
    {
        UseProgramBase(ss, &ptr->pos, base);

        vertexAttribPointer(SourceTexCoord, &ptr->tex, base);
        glUniform1i(texture1, 0);
        glUniform1i(warpTex, 1);
        glUniform2fv(camWorldPos, 1, &camPos[0]);
        glUniform2fv(camWorldSize, 1, &camSize[0]);
        glUniform1f(time, globals.renderTime);

        float4 c = abgr2rgbaf(ss.uColor);
        glUniform4fv(SourceColor, 1, &c[0]); 
    }
};

struct ShaderTextureHSV : public ShaderTextureBase, public ShaderBase<ShaderTextureHSV> {

    uint m_uTexture;
    uint m_uColorSlot;
    uint m_aTexCoords;

    void LoadTheProgram()
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

        vertexAttribPointer(m_aTexCoords, &ptr->tex, base);
        glUniform1i(m_uTexture, 0);

        float4 c = abgr2rgbaf(ss.uColor);
        glUniform4fv(m_uColorSlot, 1, &c[0]); 
    }
};

struct ShaderTonemapDither : public ShaderTextureBase, public ShaderBase<ShaderTonemapDither> {

    uint texture1;
    uint dithertex;
    uint SourceTexCoord;

    void LoadTheProgram();

    void UseProgram(const ShaderState &ss, const VertexPosTex *ptr, const OutlawTexture &ot) const;
};

struct ShaderTonemap : public ShaderTextureBase, public ShaderBase<ShaderTonemap> {

    uint texture1;
    uint SourceTexCoord;

    void LoadTheProgram();
    void UseProgram(const ShaderState &ss, const VertexPosTex *ptr, const OutlawTexture &ot) const;
};

// seperable gaussian blur
struct ShaderBlur : public ShaderTextureBase {

private:
    uint usourceTex;
    uint asourceTexCoord;
    uint uoffsets;
    
    mutable int            dimension = 0;
    mutable vector<float2> offsets;

    int samples = 0;
    int scale = 0;

    float getBlurOffset(int sample) const;
    void LoadTheProgram() { LoadShader(samples, scale); }

public:

    void LoadShader(int smpls, int scl);

    void UseProgram(const ShaderState &ss, const VertexPosTex *ptr, const OutlawTexture &ot) const;

    void setDimension(uint dim) const 
    {
        dimension = dim; 
    }

    // radius is points of blur radius (not pixels)
    static const ShaderBlur &instance(int radius);
};

struct ShaderColor : public ShaderProgramBase, public ShaderBase<ShaderColor> {

    int m_colorSlot;

    void LoadTheProgram()
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

    template <typename Vtx>
    void UseProgram(const ShaderState&ss, const Vtx* ptr, const Vtx* base) const
    {
        UseProgramBase(ss, &ptr->pos, base);
        vertexAttribPointer(m_colorSlot, &ptr->color, base);
    }
};

template <typename T, typename L>
void MeshPair<T, L>::Handle::Draw(const ShaderState& ss)
{
    mp.Draw(ss, ShaderColor::instance(), ShaderColor::instance());
}

struct ShaderColorDither : public ShaderProgramBase, public ShaderBase<ShaderColorDither> {

    int SourceColor, dithertex;

    void LoadTheProgram()
    {
        LoadProgram("ShaderColorDither");
        GET_ATTR_LOC(SourceColor);
        GET_UNIF_LOC(dithertex);
    }

    template <typename Vtx>
    void UseProgram(const ShaderState&ss, const Vtx* ptr, const Vtx* base) const
    {
        UseProgramBase(ss, &ptr->pos, base);
        vertexAttribPointer(SourceColor, &ptr->color, base);
        getDitherTex().BindTexture(0);
        glUniform1i(dithertex, 0);
    }
};

struct ShaderHsv : public ShaderProgramBase, public ShaderBase<ShaderHsv> {

    int m_colorSlot;

    void LoadTheProgram()
    {
        LoadProgram("ShaderHsv");
        m_colorSlot = getAttribLocation("ColorHSVA");
    }

    template <typename Vtx>
    void UseProgram(const ShaderState&ss, const Vtx* ptr, const Vtx* base) const
    {
        UseProgramBase(ss, &ptr->pos, base);
        vertexAttribPointer(m_colorSlot, &ptr->color, base);
    }
};

struct ShaderColorLuma : public ShaderProgramBase, public ShaderBase<ShaderColorLuma> {

    int SourceColor, Luma;

    void LoadTheProgram();

    template <typename Vtx>
    void UseProgram(const ShaderState&ss, const Vtx* ptr, const Vtx* base) const
    {
        UseProgramBase(ss, &ptr->pos, base);
        vertexAttribPointer(SourceColor, &ptr->color, base);
        vertexAttribPointer(Luma, &ptr->luma, base);
    }
};

struct ShaderSmoothColor : public ShaderProgramBase, public ShaderBase<ShaderSmoothColor> {

    int m_colorSlot;

    void LoadTheProgram()
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

    template <typename Vtx>
    void UseProgram(const ShaderState&ss, const Vtx* ptr, const Vtx* base) const
    {
        UseProgramBase(ss, &ptr->pos, base);
        vertexAttribPointer(m_colorSlot, &ptr->color, base);
    }
};

struct ShaderNoise : public ShaderProgramBase, public ShaderBase<ShaderNoise> {
    
    uint m_colorSlot;
    uint m_uRandomSeed;

    void LoadTheProgram()
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
