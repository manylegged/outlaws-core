
// Adapted from http://robertpenner.com/easing/
// BSD License http://robertpenner.com/easing_terms_of_use.html

#ifndef TWEEN_H
#define TWEEN_H

#include <cmath>

// t - time
// b - position at t0
// c - offset from t0 at t1
// d - t1

// return position at time t

typedef float (*ease_fun_t)(float t, float b, float c, float d);

inline float easeInBack(float t, float b=0.f, float c=1.f, float d=1.f)
{
    float s = 1.70158f;
    t /= d;
    return c*t*t*((s+1)*t - s) + b;
}

inline float easeOutBack(float t, float b=0.f, float c=1.f, float d=1.f)
{
    float s = 1.70158f;
    t = t/d-1;
    return c*(t*t*((s+1)*t + s) + 1) + b;
}

inline float easeInOutBack(float t, float b=0.f, float c=1.f, float d=1.f)
{
    d /= 2;
    if (t > d)
        return easeOutBack(t-d, b+(c-b)/2, c/2, d);
    return easeInBack(t, b, c/2, d);
}

inline float easeOutBounce(float t, float b=0.f, float c=1.f, float d=1.f)
{
    if ((t/=d) < (1/2.75f)) {
        return c*(7.5625f*t*t) + b;
    } else if (t < (2/2.75f)) {
        float postFix = t-=(1.5f/2.75f);
        return c*(7.5625f*(postFix)*t + .75f) + b;
    } else if (t < (2.5/2.75)) {
        float postFix = t-=(2.25f/2.75f);
        return c*(7.5625f*(postFix)*t + .9375f) + b;
    } else {
        float postFix = t-=(2.625f/2.75f);
        return c*(7.5625f*(postFix)*t + .984375f) + b;
    }
}

inline float easeInBounce(float t, float b=0.f, float c=1.f, float d=1.f)
{
    return c - easeOutBounce(d-t, 0, c, d) + b;
}


inline float easeInOutBounce(float t, float b=0.f, float c=1.f, float d=1.f)
{
    if (t < d/2) return easeInBounce(t*2, 0, c, d) * .5f + b;
    else return easeOutBounce(t*2-d, 0, c, d) * .5f + c*.5f + b;
}

inline float easeInCirc(float t, float b=0.f, float c=1.f, float d=1.f)
{
    t /= d;
    return -c * (std::sqrt(1 - t*t) - 1) + b;
}

inline float easeOutCirc(float t, float b=0.f, float c=1.f, float d=1.f)
{
    t = t/d-1;
    return c * std::sqrt(1 - t*t) + b;
}

inline float easeInOutCirc(float t, float b=0.f, float c=1.f, float d=1.f)
{
    if ((t/=d/2) < 1) return -c/2 * (std::sqrt(1 - t*t) - 1) + b;
    t -= 2;
    return c/2 * (std::sqrt(1 - t*t) + 1) + b;
}

inline float easeInCubic(float t, float b=0.f, float c=1.f, float d=1.f)
{
    t /= d;
    return c*t*t*t + b;
}

inline float easeOutCubic(float t, float b=0.f, float c=1.f, float d=1.f)
{
    t = t/d-1;
    return c*(t*t*t + 1) + b;
}

inline float easeInOutCubic(float t, float b=0.f, float c=1.f, float d=1.f)
{
    if ((t/=d/2) < 1) return c/2*t*t*t + b;
    t -= 2;
    return c/2*(t*t*t + 2) + b;
}

inline float easeInElastic(float t, float b=0.f, float c=1.f, float d=1.f)
{
    if (t==0) return b;  if ((t/=d)==1) return b+c;
    float p=d*.3f;
    float a=c;
    float s=p/4;
    float postFix =a*std::pow(2,10*(t-=1)); // this is a fix, again, with post-increment operators
    return -(postFix * std::sin((t*d-s)*(2*M_PI)/p )) + b;
}

inline float easeOutElastic(float t, float b=0.f, float c=1.f, float d=1.f)
{
    if (t==0) return b;  if ((t/=d)==1) return b+c;
    float p=d*.3f;
    float a=c;
    float s=p/4;
    return (a*std::pow(2,-10*t) * std::sin( (t*d-s)*(2*M_PI)/p ) + c + b);
}

inline float easeInOutElastic(float t, float b=0.f, float c=1.f, float d=1.f)
{
    if (t==0) return b;  if ((t/=d/2)==2) return b+c;
    float p=d*(.3f*1.5f);
    float a=c;
    float s=p/4;

    if (t < 1) {
        float postFix = a*std::pow(2,10*(t-=1)); // postIncrement is evil
        return -.5f*(postFix* std::sin( (t*d-s)*(2*M_PI)/p )) + b;
    }
    float postFix = a*std::pow(2,-10*(t-=1)); // postIncrement is evil
    return postFix * std::sin( (t*d-s)*(2*M_PI)/p )*.5f + c + b;
}

inline float easeInExpo(float t, float b=0.f, float c=1.f, float d=1.f)
{
    return (t==0) ? b : c * std::pow(2, 10 * (t/d - 1)) + b;
}

inline float easeOutExpo(float t, float b=0.f, float c=1.f, float d=1.f)
{
    return (t==d) ? b+c : c * (-std::pow(2, -10 * t/d) + 1) + b;
}

inline float easeInOutExpo(float t, float b=0.f, float c=1.f, float d=1.f)
{
    if (t==0) return b;
    if (t==d) return b+c;
    if ((t/=d/2) < 1) return c/2 * std::pow(2, 10 * (t - 1)) + b;
    return c/2 * (-std::pow(2, -10 * --t) + 2) + b;
}

inline float easeNoneLinear(float t, float b=0.f, float c=1.f, float d=1.f)
{
    return c*t/d + b;
}

inline float easeInLinear(float t, float b=0.f, float c=1.f, float d=1.f)
{
    return c*t/d + b;
}

inline float easeOutLinear(float t, float b=0.f, float c=1.f, float d=1.f)
{
    return c*t/d + b;
}

inline float easeInOutLinear(float t, float b=0.f, float c=1.f, float d=1.f)
{
    return c*t/d + b;
}

inline float easeInQuad(float t, float b=0.f, float c=1.f, float d=1.f)
{
    t /= d;
    return c*t*t + b;
}

inline float easeOutQuad(float t, float b=0.f, float c=1.f, float d=1.f)
{
    t /= d;
    return -c *t*(t-2) + b;
}

inline float easeInOutQuad(float t, float b=0.f, float c=1.f, float d=1.f)
{
    if ((t/=d/2) < 1) return ((c/2)*(t*t)) + b;
    return -c/2 * (((t-2)*(t-1)) - 1) + b;
}

inline float easeInQuart(float t, float b=0.f, float c=1.f, float d=1.f)
{
    t /= d;
    return c*t*t*t*t + b;
}

inline float easeOutQuart(float t, float b=0.f, float c=1.f, float d=1.f)
{
    t = t/d-1;
    return -c * (t*t*t*t - 1) + b;
}

inline float easeInOutQuart(float t, float b=0.f, float c=1.f, float d=1.f)
{
    if ((t/=d/2) < 1) return c/2*t*t*t*t + b;
    t -= 2;
    return -c/2 * (t*t*t*t - 2) + b;
}

inline float easeInQuint(float t, float b=0.f, float c=1.f, float d=1.f)
{
    t /= d;
    return c*t*t*t*t*t + b;
}

inline float easeOutQuint(float t, float b=0.f, float c=1.f, float d=1.f)
{
    t = t/d-1;
    return c*(t*t*t*t*t + 1) + b;
}

inline float easeInOutQuint(float t, float b=0.f, float c=1.f, float d=1.f)
{
    if ((t/=d/2) < 1) return c/2*t*t*t*t*t + b;
    t -= 2;
    return c/2*(t*t*t*t*t + 2) + b;
}

inline float easeInSine(float t, float b=0.f, float c=1.f, float d=1.f)
{
    return -c * std::cos(t/d * (M_PI/2)) + c + b;
}

inline float easeOutSine(float t, float b=0.f, float c=1.f, float d=1.f)
{
    return c * std::sin(t/d * (M_PI/2)) + b;
}

inline float easeInOutSine(float t, float b=0.f, float c=1.f, float d=1.f)
{
    return -c/2 * (std::cos(M_PI*t/d) - 1) + b;
}


#define EASE_FLOAT2(NAME) inline float2 NAME(float t, float2 b, float2 c, float d=1.f) { \
        return float2(NAME(t, b.x, c.x, d), NAME(t, b.y, c.y, d)); }

#define EASE_FLOAT3(NAME) inline float3 NAME(float t, float3 b, float3 c, float d=1.f) { \
        return float3(NAME(t, b.x, c.x, d), NAME(t, b.y, c.y, d), NAME(t, b.z, c.z, d)); }

#define EASE_VEC(NAME) EASE_FLOAT2(NAME) EASE_FLOAT3(NAME)

EASE_VEC(easeInOutLinear)
EASE_VEC(easeInSine)
EASE_VEC(easeOutSine)
EASE_VEC(easeInOutSine)
EASE_VEC(easeInQuad)
EASE_VEC(easeOutQuad)
EASE_VEC(easeInOutQuad)
EASE_VEC(easeInCubic)
EASE_VEC(easeOutCubic)
EASE_VEC(easeInOutCubic)
EASE_VEC(easeInQuart)
EASE_VEC(easeOutQuart)
EASE_VEC(easeInOutQuart)
EASE_VEC(easeInQuint)
EASE_VEC(easeOutQuint)
EASE_VEC(easeInOutQuint)
EASE_VEC(easeInExpo)
EASE_VEC(easeOutExpo)
EASE_VEC(easeInOutExpo)
EASE_VEC(easeInCirc)
EASE_VEC(easeOutCirc)
EASE_VEC(easeInOutCirc)
EASE_VEC(easeInBack)
EASE_VEC(easeOutBack)
EASE_VEC(easeInOutBack)
EASE_VEC(easeInElastic)
EASE_VEC(easeOutElastic)
EASE_VEC(easeInOutElastic)
EASE_VEC(easeInBounce)
EASE_VEC(easeOutBounce)
EASE_VEC(easeInOutBounce)

#endif
