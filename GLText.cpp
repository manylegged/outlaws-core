
//
// GLText.cpp - draw text on the screen
// - prerendered text is stored in a texture
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


#include "StdAfx.h"
#include "GLText.h"
#include "Shaders.h"


GLText::~GLText()
{
    if (texture.width > 0)
        glDeleteTextures(1, &texture.texnum);
}

void GLText::load()
{
    float pointSize = OL_GetCurrentBackingScaleFactor();
    if (chars != texChars || fontSize != texFontSize || pointSize != texPointSize)
    {
        if (texture.width > 0)
            glDeleteTextures(1, &texture.texnum);
        memset(&texture, 0, sizeof(texture));

        if (chars.size())
        {
            int status = OL_StringTexture(&texture, chars.c_str(), fontSize, font, -1.f, -1.f);
            if (status && texture.width > 0 && texture.height > 0)
            {
                texChars     = chars;
                texFontSize  = fontSize;
                texPointSize = pointSize;
            }
            ASSERT(status);
        }
    }
}

void GLText::render(const ShaderState* s, float2 pos) const
{
    if (texture.width > 0 && texture.height > 0)
    {
        ASSERT(chars == texChars && fontSize == texFontSize);

        const float2 textSizePoints = getSize();
        ShaderTexture::instance().DrawRectCornersUpsideDown(*s, texture, pos, pos + textSizePoints);
    }
}


const GLText* GLText::vget(int font, float size, const char *format, va_list vl)
{
    string s = str_vformat(format, vl);
    return get(font, size, s);
}

float2 GLText::Draw(const ShaderState &s_, float2 p, Align align, int font, uint color,
                    float sizeUnscaled, const std::string &str)
{
    const GLText* st = get(font, sizeUnscaled, str);

    float2 scale;
    switch (align) {
    case LEFT:          scale = float2(0.f,  0.f); break;
    case CENTERED:      scale = float2(0.5f, 0.f); break;
    case RIGHT:         scale = float2(1.f,  0.f); break;
    case MID_LEFT:      scale = float2(0.f,  0.5f); break;
    case MID_CENTERED:  scale = float2(0.5f, 0.5f); break;
    case MID_RIGHT:     scale = float2(1.f,  0.5f); break;
    case DOWN_LEFT:     scale = float2(0.f,  1.f); break;
    case DOWN_CENTERED: scale = float2(0.5f, 1.f); break;
    case DOWN_RIGHT:    scale = float2(1.f,  1.f); break;
    }

    ShaderState s = s_;
    float2 pos = round(p - (scale * st->getSize()));
    s.color32(color);
    st->render(&s, pos);
    return st->getSize();
}

float2 GLText::getAdvancement(char c) const
{
    static float2 *advancements[50];
    ASSERT(fontSize > 0);
    ASSERT(fontSize < 50);
    int isize = round(fontSize);
    if (!advancements[isize]) {
        advancements[isize] = new float2[128];
        OL_FontAdvancements(font, fontSize, (OLSize*) advancements[isize]);
    }
    ASSERT(c >= 0);
    return advancements[isize][(int)c];
}

float GLText::getCharStart(uint chr) const
{
    float pos = 0;
    for (uint i=0; i<chr; i++)
        pos += getAdvancement(chars[i]).x;
    return pos;
}
    

float2 GLText::getCharSize(uint chr) const
{
    return getAdvancement(chars[chr]);
}

float GLText::getScaledSize(float sizeUnscaled)
{
    View view;
    OL_GetWindowSize(&view.sizePixels.x, &view.sizePixels.y,
                  &view.sizePoints.x, &view.sizePoints.y);
    return sizeUnscaled * view.sizePoints.y / 720.f;
}

const GLText* GLText::get(int font, float size, const string& s)
{
    float pointSize = OL_GetCurrentBackingScaleFactor();
    //ReportMessagef("scale: %g", pointSize);
    
    static const int cacheSize = 64;
    static GLText cache[cacheSize];
    for (uint i=0; i<cacheSize; i++)
    {
        if (cache[i].chars == s && 
            isZero(cache[i].fontSize - size) && 
            cache[i].texPointSize == pointSize &&
            cache[i].font == font)
        {
            return &cache[i];
        }
    }

    int i = randrange(0, cacheSize);
    cache[i].chars    = s;
    cache[i].fontSize = size;
    cache[i].font     = font;
    cache[i].load();
    return &cache[i];

}

float2 GLText::DrawScreen(const ShaderState &s_, float2 p, Align align, uint color, 
                             float sizeUnscaled, const char *format, ...)
{
    va_list vl;
    va_start(vl, format);
    float2 size = Draw(s_, p, align, kDefaultFont, color, sizeUnscaled, str_vformat(format, vl));
    va_end(vl);
    return size;
}

void DrawTextBox(const ShaderState& ss1, const View& view, float2 point, float2 rad, 
                 const string& text, uint tSize, uint fgColor, uint bgColor, int font)
{
    if ((fgColor&ALPHA_OPAQUE) == 0)
        return;

    if (bgColor == 0)
        bgColor = 0x0d0715|ALPHAF(0.6);

    ShaderState ss = ss1;    
    const GLText* st = GLText::get(font, GLText::getScaledSize(tSize), text);
    float2 boxSz = 5.f + 0.5f * st->getSize();

    float2 corneroffset = rad + st->getSize();
    if (point.x + corneroffset.x > view.sizePoints.x)
        corneroffset.x = -corneroffset.x;
    if (point.y + corneroffset.y > view.sizePoints.y)
        corneroffset.y = -corneroffset.y;
    
    float2 center = round(point + 0.5f * corneroffset);

    ss.translate(center);
    ss.color32(bgColor);
    ShaderUColor::instance().DrawRect(ss, boxSz);
    ss.color32(fgColor);
    ShaderUColor::instance().DrawLineRect(ss, boxSz);
    ss.translate(round(-0.5f * st->getSize()));
    st->render(&ss);
}

const float2 kOutlinePad = float2(2.f, 1.f);

float2 DrawOutlinedText(const ShaderState &s_, float2 pos, float2 relnorm, uint color,
                        float alpha, float tsize, const string& str)
{
    ShaderState ss = s_;
    const int font = tsize > 24 ? kTitleFont : kDefaultFont;
    const GLText* txt = GLText::get(font, GLText::getScaledSize(tsize), str);
    ss.translate(pos - relnorm * txt->getSize());
    // DrawButton(&ss, 0.5f * txt->getSize(), 0.5f * txt->getSize() + float2(2.f), kOverlayBG, ALPHAF(0.5)|color, 0.5f * alpha);
    // DrawBox(&ss, 0.5f * txt->getSize(), 0.5f * txt->getSize() + kOutlinePad, kOverlayBG,
    //         SetAlphaAXXX(color, 0.5f), alpha);
    ss.color(color, alpha);
    txt->render(&ss);
    return txt->getSize() + 2.f * kOutlinePad;
}

