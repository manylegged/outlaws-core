
//
// GLText.cpp - draw text on the screen
// - prerendered text is stored in a texture
//

// Copyright (c) 2013-2015 Arthur Danskin
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
DEFINE_CVAR(float2, kAspectMinMax, float2(1.6f, 2.f));

FontStats::FontStats(int ft, float sz) : font(ft), fontSize(sz)
{
    OLSize adv[128] = {};
    static_assert(arraySize(adv) == arraySize(advancements), "size mismatch");
    OL_FontAdvancements(font, fontSize, adv);
    charMaxSize.y = OL_FontHeight(font, fontSize);
    charAvgSize.y = charMaxSize.y;
    int printable = 0;
    for (int i=0; i<vec_size(advancements); i++) {
        advancements[i] = adv[i].x;
        if (isprint(i)) {
            charMaxSize.x = max(charMaxSize.x, advancements[i]);
            charAvgSize.x += advancements[i];
            // Reportf("%s = %f", str_tostr((char)i).c_str(), advancements[i]);
            printable++;
        }
    }
    charAvgSize.x /= printable;
}

const FontStats & FontStats::getUnscaled(int font, float size)
{
    static std::map<pair<int, int>, FontStats> map;
    FontStats &fs = map[make_pair(font, round_int(size))];
    if (fs.fontSize <= 0.f)
        fs = FontStats(font, round_int(size));
    return fs;
}

void GLText::load(const string& str, int font_, float size, float pointSize)
{
    chars    = str;
    fontSize = size;
    font     = font_;

    if (chars == texChars && fontSize == texFontSize && pointSize == texPointSize)
        return;
    
    texture.clear();

    if (!chars.size())
        return;
    
    OutlawImage img = {};
    int status = OL_StringImage(&img, chars.c_str(), fontSize, font,
                                globals.windowSizePoints.x, globals.windowSizePoints.y);

    if (!status || img.width <= 0 || img.height <= 0)
        return;
    
    texture.TexImage2D(img);
    OL_FreeImage(&img);

    texChars     = chars;
    texFontSize  = fontSize;
    texPointSize = pointSize;
}

void GLText::render(const ShaderState* s, float2 pos) const
{
    if (texture.empty())
        return;
    
    ASSERT(chars == texChars && fontSize == texFontSize);
    
    const float2 textSizePoints = getSize();

    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    ShaderTexture::instance().DrawRectCornersUpsideDown(*s, texture, pos, pos + textSizePoints);
    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
}


const GLText* GLText::vget(int font, float size, const char *format, va_list vl)
{
    string s = str_vformat(format, vl);
    return getUnscaled(font, size, s);
}

float2 GLText::Draw(const ShaderState &s_, float2 p, Align align, int font, uint color,
                    float sizeUnscaled, const std::string &str)
{
    if (str.empty())
        return float2(0.f);
    const GLText* st = getUnscaled(font, sizeUnscaled, str);

#if __APPLE__
    const float mid_y = 0.60f;
#else
    const float mid_y = 0.55f;
#endif

    float2 scale;
    switch (align) {
    case LEFT:          scale = float2(0.f,  0.f); break;
    case CENTERED:      scale = float2(0.5f, 0.f); break;
    case RIGHT:         scale = float2(1.f,  0.f); break;
    case MID_LEFT:      scale = float2(0.f,  mid_y); break;
    case MID_CENTERED:  scale = float2(0.5f, mid_y); break;
    case MID_RIGHT:     scale = float2(1.f,  mid_y); break;
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

float GLText::getCharStart(uint chr) const
{
    if (chr == 0)
        return 0.f;
    const GLText *st = getUnscaled(font, fontSize, chars.substr(0, chr));
    return st->getSize().x;
}
    
float2 GLText::getCharSize(uint chr) const
{
    string cr = (chr < chars.size()) ? utf8_substr(chars, chr, 1) : "a";
    const GLText *st = getUnscaled(font, fontSize, cr);
    return st->getSize();
}

float GLText::getScaledSize(float sizeUnscaled)
{
    const float2 ws = clamp_aspect(globals.windowSizePoints, kAspectMinMax.x, kAspectMinMax.y);
    return sizeUnscaled * ws.y / kTextScaleHeight;
}

static DEFINE_CVAR(int, kGLTextCacheSize, 128);
static int     s_cacheSize = kGLTextCacheSize;
static GLText *s_cache;

const GLText* GLText::getUnscaled(int font, float size, const string& s)
{
    const float pointSize = OL_GetCurrentBackingScaleFactor();
    //Reportf("scale: %g", pointSize);

    if (!s_cache || s_cacheSize != kGLTextCacheSize)
    {
        delete[] s_cache;
        s_cacheSize = kGLTextCacheSize;
        s_cache = new GLText[s_cacheSize];
    }
    
    for (int i=0; i<s_cacheSize; i++)
    {
        GLText &it = s_cache[i];
        if (it.chars == s && 
            isZero(it.fontSize - size) && 
            it.texPointSize == pointSize &&
            it.font == font)
        {
            return &it;
        }
    }

    // Reportf("RAND %d (gltext)", randrange(1, 101));
    const int i = randrange(0, s_cacheSize);
    s_cache[i].load(s, font, size, pointSize);
    return &s_cache[i];
}

void GLText::clear()
{
    texture.clear();
    texChars.clear();
    chars.clear();
}

void GLText::clear_cache()
{
    for (int i=0; i<s_cacheSize; i++)
        s_cache[i].clear();
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

float2 GLText::Fmt(const ShaderState &s_, float2 p, Align align, uint color, 
                   float sizeUnscaled, const char* fmt, ...)
{
    va_list vl;
    va_start(vl, fmt);
    float2 size = Put(s_, p, align, color, sizeUnscaled, str_vformat(fmt, vl));
    va_end(vl);
    return size;
}

float2 GLText::Fmt(const ShaderState &s_, float2 p, Align align, int font, uint color,
                   float sizeUnscaled, const char* fmt, ...)
{
    va_list vl;
    va_start(vl, fmt);
    float2 size = Put(s_, p, align, font, color, sizeUnscaled, str_vformat(fmt, vl));
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
    const GLText* txt = GLText::get(font, tsize, str);
    ss.translate(pos - relnorm * txt->getSize());
    ss.color(color, alpha);
    txt->render(&ss);
    return txt->getSize() + 2.f * kOutlinePad;
}

