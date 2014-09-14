
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

static DEFINE_CVAR(float, kTextScaleHeight, IS_TABLET ? 320.f : 720.f);

void GLText::load(const string& str, int font_, float size, float pointSize)
{
    chars    = str;
    fontSize = size;
    font     = font_;

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

    DPRINT(GUI, ("Rendered string texture %3d chars, %d/%.1f: %3dx%3d@%.fx", 
                 (int)str.size(), font, size, texture.width, texture.height, texPointSize));
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
    if (str.empty())
        return float2(0.f);
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
    chr = min(chr, (uint)chars.size());
    float pos = 0;
    for (uint i=0; i<chr; i++)
        pos += getAdvancement(chars[i]).x;
    return pos;
}
    

float2 GLText::getCharSize(uint chr) const
{
    char c = chr < chars.size() ? chars[chr] : ' ';
    return float2(getAdvancement(c).x, getFontHeight(font, fontSize));
}

static std::unordered_map< pair<int, int>, float > s_fontHeights;

float GLText::getFontHeight(int font, float size)
{
    float& height = s_fontHeights[make_pair(font, (int) ceil(size))];
    if (height == 0)
        height = OL_FontHeight(font, size);
    return height;
}

float GLText::getScaledSize(float sizeUnscaled)
{
    return sizeUnscaled * globals.windowSizePoints.y / kTextScaleHeight;
}

const GLText* GLText::get(int font, float size, const string& s)
{
    float pointSize = OL_GetCurrentBackingScaleFactor();
    //ReportMessagef("scale: %g", pointSize);

    static DEFINE_CVAR(int, kGLTextCacheSize, 128);

    static int cacheSize = kGLTextCacheSize;
    static GLText *cache = new GLText[cacheSize];
    if (cacheSize != kGLTextCacheSize) {
        GLText *ncache = new GLText[kGLTextCacheSize];
        for (int i=0; i<min(cacheSize, kGLTextCacheSize); i++)
            ncache[i] = cache[i];
        cacheSize = kGLTextCacheSize;
    }
    
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

    const int i = randrange(0, cacheSize);
    cache[i].load(s, font, size, pointSize);
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

const float2 kOutlinePad = float2(2.f, 1.f);

float2 DrawOutlinedText(const ShaderState &s_, float2 pos, float2 relnorm, uint color,
                        float alpha, float tsize, const string& str)
{
    if (str.empty())
        return float2();
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

