//
// RGB.h - color transformations
// 
// Copyright (c) 2014-2016 Arthur Danskin
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

#ifndef RGB_H
#define RGB_H

inline uint ALPHA(float X)  { return uint(clamp(X, 0.f, 1.f) * 255.f) << 24; }
inline uint ALPHAF(float X) { return uint(clamp(X, 0.f, 1.f) * 255.f) << 24; }
#define ALPHA_OPAQUE 0xff000000


inline float3 rgb2rgbf(uint rgb)
{
    const float m = 1.f / 255.f;
    return m * float3((rgb>>16)&0xff, (rgb>>8)&0xff,  rgb&0xff);
}

inline float4 rgb2rgbaf(uint rgb, float alpha=1.f)
{
    return float4(rgb2rgbf(rgb), alpha);
}

inline float4 argb2rgbaf(uint argb)
{
    const float m = 1.f / 255.f;
    return m * float4((argb>>16)&0xff, (argb>>8)&0xff, (argb&0xff), (argb>>24)&0xff);
}

// Graphics.cpp
uint rgbaf2argb(const float4 &rgba_);

uint rgbf2rgb(const float3 &rgb_);

inline uint rgbf2argb(const float3 &rgb)
{
    return 0xff000000 | rgbf2rgb(rgb);
}

inline uint MultAlphaAXXX(uint color, float alpha)
{
    float a  = (float) (color>>24) / 255.f;
    float aa = a * alpha;
    uint  na = (uint) min(255.f, aa * 255.f);
    return (color&0xffffff) | (na<<24);
}

inline uint SetAlphaAXXX(uint color, float alpha)
{
    return (color&0xffffff) | ((uint) (alpha * 255.f) << 24);
}

inline float GetAlphaAXXX(uint color)
{
    return (float) ((color&ALPHA_OPAQUE)>>24) / 255.f;
}

uint PremultiplyAlphaAXXX(uint color);

uint PremultiplyAlphaAXXX(uint color, float alpha);

uint PremultiplyAlphaXXX(uint color, float preAlpha, float alpha);

// Inverse of sRGB "gamma" function. (approx 2.2)
inline double inv_gam_srgb(int ic) {
    double c = ic/255.0;
    if ( c <= 0.04045 )
        return c/12.92;
    else 
        return pow(((c+0.055)/(1.055)),2.4);
}

inline float3 inv_gam_srgb(float3 c)
{
    return float3(inv_gam_srgb(c.x), inv_gam_srgb(c.y), inv_gam_srgb(c.z));
}

// sRGB "gamma" function (approx 2.2)
inline int gam_srgb(double v) {
    if(v<=0.0031308)
        v *= 12.92;
    else 
        v = 1.055*pow(v,1.0/2.4)-0.055;
    return int(v*255+.5);
}

inline float GetLumargbf(float3 c)
{
    float luma = dot(float3(0.299f, 0.587f, 0.114f), c);
    //float luma = gam_srgb(dot(float3(0.299f, 0.587f, 0.114f), inv_gam_srgb(c)));
    return luma;
}

inline float GetLumargb(uint color)
{
    return GetLumargbf(rgb2rgbf(color));
}

inline uint GetContrastWhiteBlack(uint color)
{
    return GetLumargb(color) > 0.5f ? 0x000000 : 0xffffff;
}

inline uint GetContrastWhiteBlack(float3 color)
{
    return GetLumargbf(color) > 0.5f ? 0x000000 : 0xffffff;
}

uint32 argb2abgr(uint32 argb, float alpha);
uint32 argb2abgr_zero_alpha(uint32 argb, float alpha);

inline uint32 argb2abgr(uint32 argb)
{
    uint  r0b = (argb&0xff00ff);
    return (r0b << 16) | (argb&0xff00ff00) | (r0b >> 16);
}

inline uint32 argb2rgba(uint32 argb)
{
    return (argb<<8)|(argb>>24);
}
inline float4 abgr2rgbaf(uint32 abgr)
{
    const float m = 1.f / 255.f;
    return m * float4((abgr)&0xff, (abgr>>8)&0xff, ((abgr>>16)&0xff), (abgr>>24)&0xff);
}

inline uint32 rgb2bgr(uint32 rgb)
{
    uint  r0b = (rgb&0xff00ff);
    return (r0b << 16) | (rgb&0x0000ff00) | (r0b >> 16);
}

// how convenient
#define abgr2argb argb2abgr
#define bgr2rgb rgb2bgr

inline uint32 bgra2argb(uint32 bgra)
{
    return (bgra<<24) | ((bgra&0xff00)<<8) | ((bgra&0xff0000)>>8) | (bgra>>24);
}

uint rgbaf2abgr(const float4 &rgba_);

inline uint lerpAXXX(uint color, uint color1, float v)
{
    return rgbaf2argb(lerp(argb2rgbaf(color), argb2rgbaf(color1), v));
}

inline uint lerpXXX(uint color, uint color1, float v)
{
    return rgbf2rgb(lerp(rgb2rgbf(color), rgb2rgbf(color1), v));
}

inline uint randlerpXXX(uint color, uint color1)
{
    return lerpXXX(color, color1, randrange(0.f, 1.f));
}

uint randlerpXXX(const std::initializer_list<uint>& lst);

// lolengine.net/blog/2013/07/27/rgb-to-hsv-in-glsl

float3 rgb2hsv_(float3 c);
float3 hsv2rgb_(float3 c);

inline float3 rgbf2hsvf(float3 rgb)
{
    float3 hsv = rgb2hsv_(rgb);
    hsv.x *= 360.f;
    return hsv;
}

float3 hsvf2rgbf(float3 hsv);

inline uint hsvf2rgb(const float3 &hsv) { return rgbf2rgb(hsvf2rgbf(hsv)); }
inline float3 rgb2hsvf(uint rgb) { return rgbf2hsvf(rgb2rgbf(rgb)); }

inline uint hsvlerpXXX(uint color, uint color1, float v)
{
    return hsvf2rgb(lerp(rgb2hsvf(color), rgb2hsvf(color1), v));
}


inline float distanceHsv(float3 a, float3 b)
{
    return length(float3(min(a.y, b.y) * ((1 - dotAngles(toradians(a.x), toradians(b.x))) / 2.f),
                          a.y - b.y,
                          a.z - b.z));
}

inline float distanceRgb(uint a, uint b)
{
    return distanceHsv(rgb2hsvf(a), rgb2hsvf(b));
}

inline float3 colorIntensify(float3 color)
{
    float3 hsv = rgbf2hsvf(color);
    // if (hsv.y > 0.2f)
        // hsv.y = max(0.7f, hsv.y); // saturation
    hsv.z = max(0.9f, hsv.z); // value
    return hsvf2rgbf(hsv);
}

inline uint colorIntensify(uint color)
{
    return rgbf2rgb(colorIntensify(rgb2rgbf(color)));
}


inline uint colorClampValue(uint color)
{
    float3 hsv = rgb2hsvf(color);
    hsv.z = clamp(hsv.z, 0.6f, 0.8f);
    return hsvf2rgb(hsv);
}

inline uint colorClampValue(uint color, float mn, float mx)
{
    float3 hsv = rgb2hsvf(color);
    hsv.z = clamp(hsv.z, mn, mx);
    return hsvf2rgb(hsv);
}


inline uint colorChangeHue(uint color, float v)
{
    float3 hsv = rgb2hsvf(color);
    hsv.x += v;
    return hsvf2rgb(hsv);
}

inline uint colorChangeSaturation(uint color, float v)
{
    float3 hsv = rgb2hsvf(color);
    hsv.y += v;
    return hsvf2rgb(hsv);
}

inline uint colorChangeValue(uint color, float v)
{
    float3 hsv = rgb2hsvf(color);
    hsv.z += v;
    return hsvf2rgb(hsv);
}

inline uint SetValuergb(uint color, float v)
{
    float3 hsv = rgb2hsvf(color);
    hsv.z = v;
    return hsvf2rgb(hsv);
}

inline uint mult_hsv(uint color, float3 hsv)
{
    float3 c = rgb2hsvf(color);
    return hsvf2rgb(c * hsv);
}

inline uint Af2A(float a)
{
    return ((uint) (255 * a)) << 24;
}



#endif // RGB_H
