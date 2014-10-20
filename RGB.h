//
// RGB.h - color transformations
// 
// Copyright (c) 2014 Arthur Danskin
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

inline uint ALPHA(uint X)   { return X << 24; }
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

inline uint rgbaf2argb(float4 rgba)
{
    rgba.x = clamp(rgba.x, 0.f, 1.f);
    rgba.y = clamp(rgba.y, 0.f, 1.f);
    rgba.z = clamp(rgba.z, 0.f, 1.f);
    rgba.w = clamp(rgba.w, 0.f, 1.f);
    rgba = round(rgba * 255.f);
    return (uint(rgba.w)<<24) | (uint(rgba.x)<<16) | (uint(rgba.y)<<8) | uint(rgba.z);
}

inline uint rgbf2rgb(float3 rgb)
{
    rgb.x = clamp(rgb.x, 0.f, 1.f);
    rgb.y = clamp(rgb.y, 0.f, 1.f);
    rgb.z = clamp(rgb.z, 0.f, 1.f);
    rgb = round(rgb * 255.f);
    return uint(rgb.x)<<16 | uint(rgb.y)<<8 | uint(rgb.z);
}

inline uint rgbf2argb(float3 rgb)
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
    uint  a  = (uint) min(255.f, fa * alpha * 255.f);
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

inline uint rgbaf2abgr(float4 rgba)
{
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

inline void flushNanToZero(float &val)
{
    if (fpu_error(val))
        val = 0;
}

inline void flushNanToZero(float2 &val)
{
    flushNanToZero(val.x);
    flushNanToZero(val.y);
}

inline void flushNanToZero(float3 &val)
{
    flushNanToZero(val.x);
    flushNanToZero(val.y);
    flushNanToZero(val.z);
}


inline float3 rgbf2hsvf(float3 rgb)
{
    float3 hsv = glm::hsvColor(rgb);
    flushNanToZero(hsv);
    return hsv;
}

inline float3 hsvf2rgbf(float3 hsv)
{
    hsv.x = modulo(hsv.x, 360.f);
    hsv.y = clamp(hsv.y, 0.f, 1.f);
    hsv.z = clamp(hsv.z, 0.f, 1.f);
    return glm::rgbColor(hsv);
}

inline uint hsvf2rgb(const float3 &hsv) { return rgbf2rgb(hsvf2rgbf(hsv)); }
inline float3 rgb2hsvf(uint rgb) { return rgbf2hsvf(rgb2rgbf(rgb)); }

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


inline uint clampColorValue(uint color)
{
    float3 hsv = rgb2hsvf(color);
    hsv.z = clamp(hsv.z, 0.6f, 0.8f);
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
