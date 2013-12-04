
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

#include "Geometry.h"
#include "Graphics.h"

static const int kDefaultFontSize = 12;

class GLText {

    OutlawTexture  texture;
    string         texChars;
    float          texFontSize;
    float          texPointSize;

    string         chars;
    OutlawFont     font;
    float          fontSize;

    GLText()
    {
        memset(&texture, 0, sizeof(texture));
        fontSize    = kDefaultFontSize;
        font        = OF_SansMono;
        texFontSize = 0;
    }
    ~GLText();

    void   load();

    float2 getAdvancement(char c) const;

public:

    float2 getSize() const
    {
        return texPointSize ? float2(texture.width, texture.height) / texPointSize : float2(0, fontSize);
    }

    float getCharStart(uint chr) const;
    float2 getCharSize(uint chr) const;
    
    // position is lower left corner of text
    void render(ShaderState* s, float2 pos=float2(0)) const;

    static const GLText* vget(float size, const char *format, va_list vl) __printflike(2, 0);

    // factory
    static const GLText* get(float size, const string& str);

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

    static float2 Draw(const ShaderState &s_, float2 p, Align align, uint color,
                       float sizeUnscaled, const std::string &s);


    static float2 DrawScreen(const ShaderState &s_, float2 p, Align align, uint color, 
                             float sizeUnscaled, const char *format, ...) __printflike(6, 7);

    static float2 DrawStr(const ShaderState &s_, float2 p, Align align, uint color, 
                          float sizeUnscaled, const string& str)
    {
        return Draw(s_, p, align, color, getScaledSize(sizeUnscaled), str);
    }
    
};

void DrawTextBox(const ShaderState& ss1, const View& view, float2 point, float2 rad, const string& text, uint tSize, uint fgColor, uint bgColor=0);

// RELNORM of (0, 0) sets pos in bottom left corner, (1, 1) sets pos in upper right corner
float2 DrawOutlinedText(const ShaderState &ss, float2 pos, float2 relnorm, uint color,
                        float alpha, float tsize, const string& str);

#endif //GLTEXT_H
