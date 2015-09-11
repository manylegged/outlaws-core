//
// RGB.h - color transformations
// 
// Copyright (c) 2014-2015 Arthur Danskin
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

inline uint rgbaf2argb(const float4 &rgba_)
{
	float4 rgba = rgba_;
    rgba.x = clamp(rgba.x, 0.f, 1.f);
    rgba.y = clamp(rgba.y, 0.f, 1.f);
    rgba.z = clamp(rgba.z, 0.f, 1.f);
    rgba.w = clamp(rgba.w, 0.f, 1.f);
    rgba = round(rgba * 255.f);
    return (uint(rgba.w)<<24) | (uint(rgba.x)<<16) | (uint(rgba.y)<<8) | uint(rgba.z);
}

inline uint rgbf2rgb(const float3 &rgb_)
{
	float3 rgb = rgb_;
    rgb.x = clamp(rgb.x, 0.f, 1.f);
    rgb.y = clamp(rgb.y, 0.f, 1.f);
    rgb.z = clamp(rgb.z, 0.f, 1.f);
    rgb = round(rgb * 255.f);
    return uint(rgb.x)<<16 | uint(rgb.y)<<8 | uint(rgb.z);
}

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

inline uint PremultiplyAlphaAXXX(uint color)
{
    float4 c = argb2rgbaf(color);
    c.x *= c.w;
    c.y *= c.w;
    c.z *= c.w;
    return rgbaf2argb(c);
}

inline uint PremultiplyAlphaAXXX(uint color, float alpha)
{
    float4 c = argb2rgbaf(color);
    c.x *= c.w;
    c.y *= c.w;
    c.z *= c.w;
    c.w = alpha;
    return rgbaf2argb(c);
}

inline uint PremultiplyAlphaXXX(uint color, float preAlpha, float alpha)
{
    float3 c = rgb2rgbf(color);
    c.x *= preAlpha;
    c.y *= preAlpha;
    c.z *= preAlpha;
    return ALPHAF(alpha)|rgbf2rgb(c);
}

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

inline uint32 argb2abgr(uint32 argb, float alpha)
{
    float fa = (float) (argb>>24) / 255.f;
    uint  a  = clamp(round_int(fa * alpha * 255.f), 0, 0xff);
    uint  r0b = (argb&0xff00ff);
    return (a << 24) | (r0b << 16)| (argb&0x00ff00)  | (r0b >> 16);
}

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

inline uint rgbaf2abgr(const float4 &rgba_)
{
	float4 rgba = rgba_;
    rgba.x = clamp(rgba.x, 0.f, 1.f);
    rgba.y = clamp(rgba.y, 0.f, 1.f);
    rgba.z = clamp(rgba.z, 0.f, 1.f);
    rgba.w = clamp(rgba.w, 0.f, 1.f);
    rgba = round(rgba * 255.f);
    return (uint(rgba.w)<<24) | (uint(rgba.z)<<16) | (uint(rgba.y)<<8) | uint(rgba.x);
}

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

inline uint randlerpXXX(const std::initializer_list<uint>& lst)
{
    float3 color;
    float total = 0.f;
    foreach (uint cl, lst)
    {
        const float val = randrange(epsilon, 1.f);
        color += val * rgb2rgbf(cl);
        total += val;
    }
    return rgbf2rgb(color / total);
}

// lolengine.net/blog/2013/07/27/rgb-to-hsv-in-glsl

inline float3 rgb2hsv_(float3 c)
{
    float4 K = float4(0.f, -1.f / 3.f, 2.f / 3.f, -1.f);
    float4 p = glm::mix(float4(c.z, c.y, K.w, K.z), float4(c.y, c.z, K.x, K.y), glm::step(c.z, c.y));
    float4 q = glm::mix(float4(p.x, p.y, p.w, c.x), float4(c.x, p.y, p.z, p.x), glm::step(p.x, c.x));

    float d = q.x - min(q.w, q.y);
    float e = 1.0e-10;
    return float3(abs(q.z + (q.w - q.y) / (6.f * d + e)), d / (q.x + e), q.x);
}

inline float3 hsv2rgb_(float3 c)
{
    float4 K = float4(1.f, 2.f / 3.f, 1.f / 3.f, 3.f);
    float3 p = abs(glm::fract(float3(c.x) + float3(K.x, K.y, K.z)) * 6.f - float3(K.w));
    return c.z * glm::mix(float3(K.x), clamp(p - float3(K.x), 0.f, 1.f), c.y);
}

inline float3 rgbf2hsvf(float3 rgb)
{
    float3 hsv = rgb2hsv_(rgb);
    hsv.x *= 360.f;
    return hsv;
}

inline float3 hsvf2rgbf(float3 hsv)
{
    hsv.x = modulo(hsv.x/360.f, 1.f);
    hsv.y = clamp(hsv.y, 0.f, 1.f);
    hsv.z = clamp(hsv.z, 0.f, 1.f);
    return hsv2rgb_(hsv);
}

inline uint hsvf2rgb(const float3 &hsv) { return rgbf2rgb(hsvf2rgbf(hsv)); }
inline float3 rgb2hsvf(uint rgb) { return rgbf2hsvf(rgb2rgbf(rgb)); }

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
