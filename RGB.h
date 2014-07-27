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


inline float3 RGB2RGBf(uint rgb)
{
    const float m = 1.f / 255.f;
    return m * float3((rgb>>16)&0xff, (rgb>>8)&0xff,  rgb&0xff);
}

inline float4 RGB2RGBAf(uint rgb, float alpha=1.f)
{
    return float4(RGB2RGBf(rgb), alpha);
}

inline float4 ARGB2RGBAf(uint argb)
{
    const float m = 1.f / 255.f;
    return m * float4((argb>>16)&0xff, (argb>>8)&0xff, (argb&0xff), (argb>>24)&0xff);
}

inline uint RGBAf2ARGB(float4 rgba)
{
    rgba.x = clamp(rgba.x, 0.f, 1.f);
    rgba.y = clamp(rgba.y, 0.f, 1.f);
    rgba.z = clamp(rgba.z, 0.f, 1.f);
    rgba.w = clamp(rgba.w, 0.f, 1.f);
    rgba = round(rgba * 255.f);
    return (uint(rgba.w)<<24) | (uint(rgba.x)<<16) | (uint(rgba.y)<<8) | uint(rgba.z);
}

inline uint RGBf2RGB(float3 rgb)
{
    rgb.x = clamp(rgb.x, 0.f, 1.f);
    rgb.y = clamp(rgb.y, 0.f, 1.f);
    rgb.z = clamp(rgb.z, 0.f, 1.f);
    rgb = round(rgb * 255.f);
    return uint(rgb.x)<<16 | uint(rgb.y)<<8 | uint(rgb.z);
}

inline uint RGBf2ARGB(float3 rgb)
{
    return 0xff000000 | RGBf2RGB(rgb);
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

inline uint PremultiplyAlphaAXXX(uint color)
{
    float4 c = ARGB2RGBAf(color);
    c.x *= c.w;
    c.y *= c.w;
    c.z *= c.w;
    return RGBAf2ARGB(c);
}

inline uint PremultiplyAlphaAXXX(uint color, float alpha)
{
    float4 c = ARGB2RGBAf(color);
    c.x *= c.w;
    c.y *= c.w;
    c.z *= c.w;
    c.w = alpha;
    return RGBAf2ARGB(c);
}

inline uint PremultiplyAlphaXXX(uint color, float preAlpha, float alpha)
{
    float3 c = RGB2RGBf(color);
    c.x *= preAlpha;
    c.y *= preAlpha;
    c.z *= preAlpha;
    return ALPHAF(alpha)|RGBf2RGB(c);
}

// Inverse of sRGB "gamma" function. (approx 2.2)
inline double inv_gam_sRGB(int ic) {
    double c = ic/255.0;
    if ( c <= 0.04045 )
        return c/12.92;
    else 
        return pow(((c+0.055)/(1.055)),2.4);
}

inline float3 inv_gam_sRGB(float3 c)
{
    return float3(inv_gam_sRGB(c.x), inv_gam_sRGB(c.y), inv_gam_sRGB(c.z));
}

// sRGB "gamma" function (approx 2.2)
inline int gam_sRGB(double v) {
    if(v<=0.0031308)
        v *= 12.92;
    else 
        v = 1.055*pow(v,1.0/2.4)-0.055;
    return int(v*255+.5);
}

inline float GetLumaRGBf(float3 c)
{
    float luma = dot(float3(0.299f, 0.587f, 0.114f), c);
    //float luma = gam_sRGB(dot(float3(0.299f, 0.587f, 0.114f), inv_gam_sRGB(c)));
    return luma;
}

inline float GetLumaRGB(uint color)
{
    return GetLumaRGBf(RGB2RGBf(color));
}

inline uint GetContrastWhiteBlack(uint color)
{
    return GetLumaRGB(color) > 0.5f ? 0x000000 : 0xffffff;
}

inline uint GetContrastWhiteBlack(float3 color)
{
    return GetLumaRGBf(color) > 0.5f ? 0x000000 : 0xffffff;
}

inline uint SetLumaRGB(uint color, float luma)
{
    float3 c = RGB2RGBf(color);
    float l = GetLumaRGBf(c);
    if (l < epsilon) {
        c = float3(luma);
    } else {
        c *= (luma / l);
    }
    //   ASSERT(isZero(GetLumaRGBf(c) - luma));
    return RGBf2RGB(c);
}

inline uint32 ARGB2ABGR(uint32 argb, float alpha)
{
    float fa = (float) (argb>>24) / 255.f;
    uint  a  = (uint) min(255.f, fa * alpha * 255.f);
    uint  r0b = (argb&0xff00ff);
    return (a << 24) | (r0b << 16)| (argb&0x00ff00)  | (r0b >> 16);
}

inline uint32 ARGB2ABGR(uint32 argb)
{
    uint  r0b = (argb&0xff00ff);
    return (r0b << 16) | (argb&0xff00ff00) | (r0b >> 16);
}

inline uint32 ARGB2RGBA(uint32 argb)
{
    return (argb<<8)|(argb>>24);
}
inline float4 ABGR2RGBAf(uint32 abgr)
{
    const float m = 1.f / 255.f;
    return m * float4((abgr)&0xff, (abgr>>8)&0xff, ((abgr>>16)&0xff), (abgr>>24)&0xff);
}

inline uint32 RGB2BGR(uint32 rgb)
{
    uint  r0b = (rgb&0xff00ff);
    return (r0b << 16) | (rgb&0x0000ff00) | (r0b >> 16);
}

inline uint RGBAf2ABGR(float4 rgba)
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
    return RGBAf2ARGB(lerp(ARGB2RGBAf(color), ARGB2RGBAf(color1), v));
}

inline uint lerpXXX(uint color, uint color1, float v)
{
    return RGBf2RGB(lerp(RGB2RGBf(color), RGB2RGBf(color1), v));
}

inline uint randlerpXXX(uint color, uint color1)
{
    return lerpXXX(color, color1, randrange(0.f, 1.f));
}

inline float3 hsvOfRgb(float3 rgb)
{
    // flush nans to zero...
    float3 hsv = glm::hsvColor(rgb);
    if (hsv.x != hsv.x) hsv.x = 0;
    if (hsv.y != hsv.y) hsv.y = 0;
    if (hsv.z != hsv.z) hsv.z = 0;
    return hsv;
}

inline float3 rgbOfHsv(float3 hsv)
{
    hsv.x = modulo(hsv.x, 360.f);
    hsv.y = clamp(hsv.y, 0.f, 1.f);
    hsv.z = clamp(hsv.z, 0.f, 1.f);
    return glm::rgbColor(hsv);
}

inline uint HSVf2RGB(const float3 &hsv) { return RGBf2RGB(rgbOfHsv(hsv)); }
inline float3 RGB2HSVf(uint rgb) { return hsvOfRgb(RGB2RGBf(rgb)); }

inline float3 colorIntensify(float3 color)
{
    float3 hsv = glm::hsvColor(color);
    // if (hsv.y > 0.2f)
        // hsv.y = max(0.7f, hsv.y); // saturation
    hsv.z = max(0.9f, hsv.z); // value
    return rgbOfHsv(hsv);
}

inline uint colorIntensify(uint color)
{
    return RGBf2RGB(colorIntensify(RGB2RGBf(color)));
}


inline uint clampColorValue(uint color)
{
    float3 hsv = hsvOfRgb(RGB2RGBf(color));
    hsv.z = clamp(hsv.z, 0.6f, 0.8f);
    return RGBf2RGB(rgbOfHsv(hsv));
}


inline uint colorChangeHue(uint color, float v)
{
    float3 hsv = hsvOfRgb(RGB2RGBf(color));
    hsv.x += v;
    return RGBf2RGB(rgbOfHsv(hsv));
}

inline uint colorChangeSaturation(uint color, float v)
{
    float3 hsv = hsvOfRgb(RGB2RGBf(color));
    hsv.y += v;
    return RGBf2RGB(rgbOfHsv(hsv));
}

inline uint colorChangeValue(uint color, float v)
{
    float3 hsv = hsvOfRgb(RGB2RGBf(color));
    hsv.z += v;
    return RGBf2RGB(rgbOfHsv(hsv));
}

inline uint SetValueRGB(uint color, float v)
{
    float3 hsv = hsvOfRgb(RGB2RGBf(color));
    hsv.z = v;
    return RGBf2RGB(rgbOfHsv(hsv));
}

inline uint colorContrastValue(uint color)
{
    float3 hsv = hsvOfRgb(RGB2RGBf(color));
    hsv.z = modulo(hsv.z + 0.5f, 1.f);
    return RGBf2RGB(rgbOfHsv(hsv));
}

inline uint colorChangeHSV(uint color, float3 hsv)
{
    float3 c = hsvOfRgb(RGB2RGBf(color));
    return RGBf2RGB(rgbOfHsv(c + hsv));
}

inline float hueDiff(uint a, uint b)
{
    return hsvOfRgb(RGB2RGBf(a)).x - hsvOfRgb(RGB2RGBf(b)).x;
}

inline uint Af2A(float a)
{
    return ((uint) (255 * a)) << 24;
}



#endif // RGB_H
