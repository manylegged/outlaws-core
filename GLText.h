
//
// GLText.h - draw text on the screen
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

#ifndef GLTEXT_H
#define GLTEXT_H

#include "Graphics.h"

static const int kDefaultFontSize = 12;
static const float2 kAspectMinMax(1.6f, 2.f);

enum { kDefaultFont=0,
       kFallbackFont,
       kSymbolFont,
       kMonoFont,
       kTitleFont };

struct FontStats {
    int    font     = kDefaultFont;
    float  fontSize = -1.f;
    float  advancements[128];
    float2 charMaxSize;
    float2 charAvgSize;

    FontStats() {}
    FontStats(int ft, float sz);
    static const FontStats &get(int font, float size);
};

class GLText {

    OutlawTexture  texture;
    string         texChars;
    float          texFontSize = 0.f;
    float          texPointSize = 0.f;

    string         chars;
    int            font = kDefaultFont;
    float          fontSize = kDefaultFontSize;

    GLText()
    {
        memset(&texture, 0, sizeof(texture));
    }

    void load(const string& str, int font, float size, float pointSize);

public:

    float2 getSize() const
    {
        return texPointSize ? float2(texture.width, texture.height) / texPointSize : float2(0, fontSize);
    }

    float getCharStart(uint chr) const;
    float2 getCharSize(uint chr) const;
    
    // position is lower left corner of text
    void render(const ShaderState* s, float2 pos=float2(0)) const;

    // factory
    static const GLText* get(int font, float size, const string& str);

    static float getScaledSize(float sizeUnscaled);

    enum Align {
        LEFT,
        CENTERED,
        RIGHT,
        DOWN_LEFT,
        DOWN_CENTERED,
        DOWN_RIGHT,
        MID_LEFT,
        MID_CENTERED,
        MID_RIGHT,
    };

private:
    static const GLText* vget(int font, float size, const char *format, va_list vl) __printflike(3, 0);

    static float2 Draw(const ShaderState &s_, float2 p, Align align, int font, uint color,
                       float sizeUnscaled, const std::string &s);

public:


    static float2 DrawScreen(const ShaderState &s_, float2 p, Align align, uint color, 
                             float sizeUnscaled, const char *format, ...) __printflike(6, 7);

    static float2 Put(const ShaderState &s_, float2 p, Align align, uint color, 
                      float sizeUnscaled, const string& str)
    {
        return Draw(s_, p, align, kDefaultFont, color, getScaledSize(sizeUnscaled), str);
    }

    static float2 Put(const ShaderState &s_, float2 p, Align align, int font, uint color, 
                      float sizeUnscaled, const string& str)
    {
        return Draw(s_, p, align, font, color, getScaledSize(sizeUnscaled), str);
    }

    static float2 Fmt(const ShaderState &s_, float2 p, Align align, uint color,
                      float sizeUnscaled, const char* fmt, ...) __printflike(6, 7);

    static float2 Fmt(const ShaderState &s_, float2 p, Align align, int font, uint color,
                      float sizeUnscaled, const char* fmt, ...) __printflike(7, 8);
};

// RELNORM of (0, 0) sets pos in bottom left corner, (1, 1) sets pos in upper right corner
float2 DrawOutlinedText(const ShaderState &ss, float2 pos, float2 relnorm, uint color,
                        float alpha, float tsize, const string& str);

#endif //GLTEXT_H
