
//
// Geometry.h - general purpose geometry and math
// - polygon intersections and computational geometry
// - vector and numerical operations
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


#ifndef Outlaws_Geometry_h
#define Outlaws_Geometry_h

#ifdef __clang__
#pragma clang diagnostic push
#pragma GCC diagnostic ignored "-Wshadow"
#endif

#ifdef __GNUC__
#pragma GCC diagnostic ignored "-Wshadow"
#endif

#define GLM_FORCE_ONLY_XYZW 1
#include "../glm/glm.hpp"
#include "../glm/gtc/matrix_transform.hpp"
#include "../glm/gtx/color_space.hpp"
#include "../glm/gtc/random.hpp"
//#include "../glm/gtc/quaternion.hpp"

#ifdef __clang__
#pragma clang diagnostic pop
#endif

#include <cmath>
#include <algorithm>
#include <stdlib.h>
#include <string.h>
#include <random>

typedef unsigned char uint8;
typedef unsigned short uint16;
typedef unsigned int uint32;
typedef unsigned long long uint64;
typedef uint32 uint;
typedef char int8;
typedef short int16;
typedef int int32;
typedef long long int64;

// ternary digit: -1, 0, or 1, (false, unknown, true)
class trit {
    int val;
public:
    trit() : val(0) {}
    trit(int v) : val(v > 0 ? 1 : v < 0 ? -1 : 0) {}
    trit(bool v) : val(v ? 1 : -1) {}
    friend trit operator!(trit a) { return trit(-a.val); }
    friend trit operator&&(trit a, trit b) { return (a.val > 0) ? b : a; }
    friend trit operator||(trit a, trit b) { return (a.val > 0) ? a : b; }
    friend trit operator==(trit a, trit b) { return ((a.val && b.val) ? trit(a.val == b.val) : trit(0)); }
    friend trit operator!=(trit a, trit b) { return ((a.val && b.val) ? trit(a.val != b.val) : trit(0)); }
    explicit operator bool() { return val > 0; }
};

typedef glm::vec2 float2;
typedef glm::vec3 float3;
typedef glm::vec4 float4;

typedef glm::dvec2 double2;
typedef glm::dvec3 double3;
typedef glm::dvec4 double4;

typedef glm::ivec2 int2;
typedef glm::ivec3 int3;
typedef glm::ivec4 int4;

using glm::length;
using glm::distance;
using glm::cross;
using glm::dot;
using glm::clamp;
using glm::reflect;

using glm::asin;
using glm::acos;
using glm::sin;
using glm::cos;
using glm::ceil;
using glm::floor;
using glm::abs;
using glm::min;
using glm::max;
using glm::round;

using glm::mat3;
using glm::mat2;

#ifdef _MSC_VER
#include <float.h>
namespace std {
    inline int isnan(float v) { return _isnan(v); }
    inline int isinf(float v) { return !_finite(v); }
	inline int isnan(double v) { return _isnan(v); }
	inline int isinf(double v) { return !_finite(v); }
}
#endif
template <typename T>
inline bool fpu_error(T x) { return (std::isinf(x) || std::isnan(x)); }


inline float round(float a, float v) { return v * round(a / v); }
inline float2 round(float2 a, float v) { return float2(round(a.x, v), round(a.y, v)); }

inline double round(double a, double v) { return v * round(a / v); }
inline double2 round(double2 a, double v) { return double2(round(a.x, v), round(a.y, v)); }

inline float2 angleToVector(float angle) { return float2(std::cos(angle), std::sin(angle)); }
inline float vectorToAngle(float2 vec) { return std::atan2(vec.y, vec.x); }

// return [-1, 1] indicating how closely the angles are alligned
inline float compareAngles(float a, float b)
{
    return dot(angleToVector(a), angleToVector(b));
}

static const double kGoldenRatio = 1.61803398875;

inline float2 toGoldenRatioY(float y) { return float2(y * kGoldenRatio, y); }
inline float2 toGoldenRatioX(float x) { return float2(x, x / kGoldenRatio); }

inline float cross(float2 a, float2 b) { return a.x * b.y - a.y * b.x; }

inline int clamp(int v, int mn, int mx) { return min(max(v, mn), mx); }
inline float clamp(float v, float mn, float mx) { return min(max(v, mn), mx); }
inline float2 clamp(float2 v, float2 mn, float2 mx) { return float2(clamp(v.x, mn.x, mx.x),
                                                                    clamp(v.y, mn.y, mx.y)); }

inline float2 clamp_length(float2 v, float mn, float mx)
{
    const float len = length(v);
    if (len < mn)
        return mn * (v / len);
    else if (len > mx)
        return mx * (v / len);
    else
        return v;
}

// return vector V clamped to fit inside origin centered rectangle with radius RAD
float2 clamp_rect(float2 v, float2 rad);

// return center of circle as close to POS as possible and with radius RAD, that fits inside of AABBox defined by RCENTER and RRAD
float2 circle_in_rect(float2 pos, float rad, float2 rcenter, float2 rrad);

inline float max_dim(float2 v) { return max(v.x, v.y); }
inline float min_dim(float2 v) { return min(v.x, v.y); }

static const float epsilon = 0.0001f;

inline bool isZero(float2 v) { return fabsf(v.x) < epsilon && fabsf(v.y) < epsilon; }
inline bool isZero(float3 v) { return fabsf(v.x) < epsilon && fabsf(v.y) < epsilon && fabsf(v.z) < epsilon; }
inline bool isZero(float v)  { return fabsf(v) < epsilon; }

// prevent nans
inline float2 normalize(float2 a)
{
    float l = length(a);
    if (l < epsilon) {
        ASSERT(0);
        return a;
    } else {
        return a / l;
    }
}

inline double2 normalize(double2 a) { return glm::normalize(a); }

inline float2 normalize_orzero(float2 a)
{
    if (isZero(a)) {
        return float2(0);
    } else {
        float l = length(a);
        return a / l;
    }
}

inline float2 maxlen(float max, float2 v)
{
    float l = length(v);
    return (l > max) ? (max * (v / l)) : v;
}

inline float2 minlen(float min, float2 v)
{
    float l = length(v);
    return (l < min) ? (min * (v / l)) : v;
}

inline float lengthSqr(const float2 &a) { return a.x * a.x + a.y * a.y; }
inline float distanceSqr(const float2 &a, const float2& b) { return lengthSqr(a-b); }


#define M_PIf float(M_PI)
#define M_PI_2f float(M_PI_2)
#define M_PI_4f float(M_PI_4)
#define M_TAOf float(2 * M_PI)
#define M_SQRT2f float(M_SQRT2)

inline float todegrees(float radians) { return 360.f / (2.f * M_PIf) * radians; }
inline float toradians(float degrees) { return 2.f * M_PIf / 360.0f * degrees; }


// rotate vector v by angle a
template <typename T>
inline glm::detail::tvec2<T> rotate(glm::detail::tvec2<T> v, T a)
{
    T cosa = cos(a);
    T sina = sin(a);
    return glm::detail::tvec2<T>(cosa * v.x - sina * v.y, sina * v.x + cosa * v.y);
}

template <typename T>
inline glm::detail::tvec2<T> rotate(const glm::detail::tvec2<T> &v, const glm::detail::tvec2<T> &a)
{
    return glm::detail::tvec2<T>(a.x * v.x - a.y * v.y, a.y * v.x + a.x * v.y);
}

inline float2 rotate90(float2 v)  { return float2(-v.y, v.x); }
inline float2 rotateN90(float2 v) { return float2(v.y, -v.x); }
inline float2 swapXY(float2 v)    { return float2(v.y, v.x); }
inline float2 flipY(float2 v)     { return float2(v.x, -v.y); }
inline float2 flipX(float2 v)     { return float2(-v.x, v.y); }
inline float3 flipY(float3 v)     { return float3(v.x, -v.y, v.z); }
inline float3 flipX(float3 v)     { return float3(-v.x, v.y, v.z); }
inline float2 justY(float2 v)     { return float2(0.f, v.y); }
inline float2 justX(float2 v)     { return float2(v.x, 0.f); }

template <typename T>
inline T multiplyComponent(T v, int i, float x)
{
    v[i] *= x;
    return v;
}

inline float sign(float a) { return ((a < -epsilon) ? -1.f : 
                                     (a > epsilon)  ? 1.f : 0.f); }

inline float logerp(float v, float tv, float s)
{ 
    if (fabsf(v) < 0.00000001)
        return 0;
    else
        return v * pow(tv/v, s);
}
inline float2 logerp(float2 v, float2 tv, float s)
{
    return float2(logerp(v.x, tv.x, s), logerp(v.y, tv.y, s));
}
inline float4 logerp(float4 v, float4 tv, float s)
{ 
    return float4(logerp(v.x, tv.x, s),
                  logerp(v.y, tv.y, s),
                  logerp(v.z, tv.z, s),
                  logerp(v.w, tv.w, s));
}

inline float logerp1(float from, float to, float v) { return pow(from, 1-v) * pow(to, v); }
inline float2 logerp1(float2 from, float2 to, float v) { return float2(pow(from.x, 1-v) * pow(to.x, v), pow(from.y, 1-v) * pow(to.y, v)); }

template <typename T>
inline T lerp(const T &from, const T &to, float v) 
{
    return (1.f - v) * from + v * to; 
}

template <typename T>
inline T clamp_lerp(const T &from, const T &to, float v) 
{
    v = clamp(v, 0.f, 1.f);
    return (1.f - v) * from + v * to; 
}

template <typename T>
inline T lerp(T *array, float v) 
{
    const uint f = floor(v);
    const uint n = ceil(v);
    if (f == n)
        return array[f];
    else
        return lerp(array[f], array[n], v-f);
}

template <typename T>
inline T lerp(const vector<T> &vec, float v) 
{
    const uint f = floor(v);
    const uint n = ceil(v);
    ASSERT(n < vec.size());
    if (f == n)
        return vec[f];
    else
        return lerp(vec[f], vec[n], v-f);
}

inline float lerpAngles(float a, float b, float v)
{
    return vectorToAngle(lerp(angleToVector(a), angleToVector(b), v));
}

// return 0-1 depending how close value is to inputs zero and one
inline float inv_lerp(float zero, float one, float val)
{
    bool inv = false;
    if (zero > one) {
        inv = true;
        swap(zero, one);
    }
    float unorm;
    if (val >= one)
        unorm = 1.f;
    else if (val <= zero)
        unorm = 0.f;
    else
        unorm = (val - zero) / (one - zero);
    return inv ? 1.f - unorm : unorm;
}

// cardinal spline, interpolating with 't' between y1 and y2 with control points y0 and y3 and tension 'c'
template <typename T>
inline T cardinal(const T& y0, const T& y1, const T& y2, const T& y3, float t, float c)
{
    const float t2  = t * t;
    const float t3  = t2 * t;
    const float h1  = 2.f * t3 - 3.f * t2 + 1.f;
    const float h2  = -2.f * t3 + 3.f * t2;
    const float h3  = t3 - 2.f * t2 + t;
    const float h4  = t3 - t2;
    //const T     dy  = y2 - y1;
    //const T     dy1 = y1 - y0;
    //const T     dy2 = y3 - y2;
    const T     m1  = c * (y2 - y0);// * 2.f * dy / (dy1 + dy);
    const T     m2  = c * (y3 - y1);// * 2.f * dy / (dy + dy2);
    const T     r   = m1 * h3 + y1 * h1 + y2 * h2 + m2 * h4;
    return r;
}

template <typename T>
inline T cardinal(T *array, uint size, float v, float c)
{
    const uint  f = floor(v);
    const float t = v - f;
    if (f + 1 >= size /*t < epsilon */) {
        return array[f];
    }
    ASSERT(size > 1);
    ASSERT(f + 1 < size);
    return cardinal(array[(f > 0) ? f-1 : 0],
                    array[f], 
                    array[f+1],
                    array[(f + 2 < size) ? f+2 : f+1],
                    t, c);
}

// normalized sigmoid (s-shape) function
// adapted to unorm from http://dinodini.wordpress.com/2010/04/05/normalized-tunable-sigmoid-functions/
inline float signorm(float x, float k)
{
    float y = 0;
    if (x > 0.5) {
        x -= 0.5;
        k = -1 - k;
        y = 0.5f;
    }
    return y + (2.f * x * k) / (2.f * (1 + k - 2.f * x) );
}

// http://en.wikipedia.org/wiki/Smoothstep
inline float smootherstep(float edge0, float edge1, float x)
{
    // Scale, and clamp x to 0..1 range
    x = clamp((x - edge0)/(edge1 - edge0), 0.0, 1.0);
    // Evaluate polynomial
    return x*x*x*(x*(x*6 - 15) + 10);
}

// map unorm to a smooth bell curve shape (0->0, 0.5->1, 1->0)
inline float bellcurve(float x)
{
    return 0.5f * (-cos(M_TAOf * x) + 1);
}

struct BCircle { 
    float2 pos; 
    float radius;
};

inline float lnorm(float val, float low, float high)
{
    if (low >= high)
        return 1.f;
    val = clamp(val, low, high);
    return (val - low) / (high - low);
}

inline float parabola(float x, float roota, float rootb) { return (x-roota) * (x-rootb); }

// like sin, but 0 to 1 instead of -1 to 1
inline float unorm_sin(float a)
{
    return 0.5f * (1.f + sin(a));
}


///////////////////////////////////////////// intersection

struct AABBox {
    
    float2 mn, mx;

    float2 getRadius() const { return 0.5f * (mx-mn); }
    float2 getCenter() const { return 0.5f * (mx+mn); }

    void start(float2 pt) 
    {
        if (mn != mx)
            return;
        mn = pt;
        mx = pt;
    }

    void reset()
    {
        mn = float2(0);
        mx = float2(0);
    }
    
    void insertPoint(float2 pt)
    {
        start(pt);
        mx = max(mx, pt);
        mn = min(mn, pt);
    }

    void insertCircle(float2 pt, float rad)
    {
        start(pt);
        mx = max(mx, pt + float2(rad));
        mn = min(mn, pt - float2(rad));
    }

    void insertRect(float2 pt, float2 rad)
    {
        start(pt);
        mx = max(mx, pt + rad);
        mn = min(mn, pt - rad);
    }

    void insertRectCorners(float2 p0, float2 p1)
    {
        start(p0);
        mx = max(mx, max(p0, p1));
        mn = min(mn, min(p0, p1));
    }

    void insertAABBox(const AABBox &bb)
    {
        start(bb.getCenter());
        mx = max(mx, bb.mx);
        mn = min(mn, bb.mn);
    }
};

inline bool intersectPointCircle(const float2 &p, const float2 &c, float r)
{
    const float2 x = p-c;
    return x.x * x.x + x.y * x.y <= (r*r);
}

inline bool intersectPointRing(const float2 &p, const float2 &c, float minr, float maxr)
{
    const float2 x = p-c;
    const float v = x.x * x.x + x.y * x.y;
    return (minr*minr) <= v && v <= (maxr*maxr);
}

inline bool intersectCircleCircle(const float2 &p, float pr, const float2 &c, float cr)
{
    return intersectPointCircle(p, c, pr+cr);
}

inline float distanceCircleCircleSqr(float2 ap, float ar, float2 bp, float br)
{
    const float2 x = ap-bp;
    return (x.x * x.x + x.y * x.y) - ((ar+br)*(ar+br));
}

// intersect two circles, returning number of intersections with points in RA and RB
int intersectCircleCircle(float2 *ra, float2 *rb, const float2 &p, float pr, const float2 &c, float cr);

bool intersectSegmentSegment(float2 a1, float2 a2, float2 b1, float2 b2);
bool intersectSegmentSegment(float2 *o, float2 a1, float2 a2, float2 b1, float2 b2);

// distance from point P to closest point on line A B
// FIXME could probably do this with only one sqrt...
inline float perpendicularDistance(float2 a, float2 b, float2 p)
{
    float2 segv    = b - a;
    float2 ptv     = p - a;
    float  seglen  = length(segv);
    float2 usegv   = segv / seglen;
    float  proj    = dot(ptv, usegv);
    float2 closest = a + proj * usegv;
    return distance(closest, p);
}

// orient and incircle are adapted from Jonathan Richard Shewchuk's "Fast Robust Geometric Predicates"

/*               Return a positive value if the points pa, pb, and pc occur  */
/*               in counterclockwise order; a negative value if they occur   */
/*               in clockwise order; and zero if they are collinear.  The    */
/*               result is also a rough approximation of twice the signed    */
/*               area of the triangle defined by the three points.           */
inline float orient(float2 p1, float2 p2, float2 p3)
{
    return (p2.x - p1.x)*(p3.y - p1.y) - (p2.y - p1.y)*(p3.x - p1.x);
}

// p1 is implicitly 0, 0
inline float orient2(float2 p2, float2 p3)
{
    return p2.x * p3.y - p2.y * p3.x;
}

/*               Return a positive value if the point pd lies inside the     */
/*               circle passing through pa, pb, and pc; a negative value if  */
/*               it lies outside; and zero if the four points are cocircular.*/
/*               The points pa, pb, and pc must be in counterclockwise       */
/*               order, or the sign of the result will be reversed.          */
float incircle(float2 pa, float2 pb, float2 pc, float2 pd);

// Graham's scan
int convexHull(vector<float2> &points);

inline float areaForPoly(const int numVerts, const float2 *verts)
{
	double area = 0.0;
	for(int i=0; i<numVerts; i++){
		area += cross(verts[i], verts[(i+1)%numVerts]);
	}
	
	return -area/2.0;
}


// ported into c++ from python source at http://doswa.com/blog/2009/07/13/circle-segment-intersectioncollision/
// return the point on line segment a, b closest to p
inline float2 closestPointOnSegment(float2 a, float2 b, float2 p)
{
    float2 segv = b - a;
    float2 ptv = p - a;
    float seglen = length(segv);
    float2 usegv = segv / seglen;
    float proj = dot(ptv, usegv);
    
    if (proj <= 0)
        return a;
    else if (proj >= seglen)
        return b;
    else
        return a + proj * usegv;
}

// ray starting at point a and extending in direction dir
inline float2 closestPointOnRay(const float2 &a, float2 usegv, float2 p)
{
    if (isZero(usegv))
        return a;
    usegv = normalize(usegv);
    const float proj = dot(p - a, usegv);
    
    if (proj <= 0)
        return a;
    else
        return a + proj * usegv;
}

// return true if line segment a, b and circle c, r intersect
inline bool intersectSegmentCircle(float2 a, float2 b, float2 c, float r)
{
    float2 closest = closestPointOnSegment(a, b, c);
    return intersectPointCircle(closest, c, r);
}

// return true if line segment a, b and circle c, r intersect
inline bool intersectSegmentCircle(float2* otp, float2 a, float2 b, float2 c, float r)
{
    *otp = closestPointOnSegment(a, b, c);
    return intersectPointCircle(*otp, c, r);
}

inline bool intersectStadiumCircle(float2 a, float2 b, float r, float2 c, float cr)
{
    float2 closest = closestPointOnSegment(a, b, c);
    return intersectCircleCircle(closest, r, c, cr);
}


inline bool intersectRayCircle(const float2 &a, float2 d, const float2 &c, float r)
{
    float2 closest = closestPointOnRay(a, d, c);
    return intersectPointCircle(closest, c, r);
}

// modified from http://stackoverflow.com/questions/1073336/circle-line-collision-detection
// ray is at point E in direction d
// circle is at point C with radius r
bool intersectRayCircle(float2 *o, float2 E, float2 d, float2 C, float r);

inline bool intersectRaySegment(float2 rpt, float2 rdir, float2 sa, float2 sb)
{
    float t = ((rdir.x * rpt.y + rdir.y * (sa.x - rpt.x)) - (rdir.x * sb.y)) / 
              (rdir.y * (sa.x + sb.x) - rdir.x * (sa.y + sb.y));
    return 0.f <= t && t <= 1.f;
}

// a and b are the center of each rectangle, and ar and br are the distance from the center to each edge
inline bool intersectRectangleRectangle(const float2 &a, const float2 &ar, const float2 &b, const float2 &br)
{
    return (fabsf(a.x - b.x) <= (ar.x + br.x) &&
            fabsf(a.y - b.y) <= (ar.y + br.y));
}

inline bool intersectCircleRectangle(const float2 &circle, float circleR, const float2 &rectP, const float2 &rectR)
{
    const float2 circleDistance(fabsf(circle.x - rectP.x),
                                fabsf(circle.y - rectP.y));

    if (circleDistance.x > (rectR.x + circleR)) { return false; }
    if (circleDistance.y > (rectR.y + circleR)) { return false; }

    if (circleDistance.x <= (rectR.x)) { return true; } 
    if (circleDistance.y <= (rectR.y)) { return true; }

    return intersectPointCircle(circleDistance, rectR, circleR);
}

inline bool intersectPointRectangle(float2 p, float2 b, float2 br)
{
    const float2 v = b - p;
    return v.x > -br.x && v.y > -br.y && v.x <= br.x && v.y <= br.y;
}

inline bool containedCircleInRectangle(const float2 &circle, float circleR, const float2 &rectP, const float2 &rectR)
{
    return intersectPointRectangle(circle, rectP, rectR - float2(circleR));
}

float intersectBBSegmentV(float bbl, float bbb, float bbr, float bbt, float2 a, float2 b);

inline bool intersectBBSegment(float l, float b, float r, float t, float2 ss, float2 se)
{
    float v = intersectBBSegmentV(l, b, r, t, ss, se);
    return 0 <= v && v <= 1.f;
}

inline bool intersectRectangleSegment(float2 rc, float2 rr, float2 ss, float2 se)
{
    float v = intersectBBSegmentV(rc.x-rr.x, rc.y-rr.y, rc.x+rr.x, rc.y+rr.y, ss, se);
    return 0 <= v && v <= 1.f;
}

// from http://www.blackpawn.com/texts/pointinpoly/
bool intersectPointTriangle(float2 P, float2 A, float2 B, float2 C);

// quad points must have clockwise winding
inline bool intersectPointQuad(float2 P, float2 A, float2 B, float2 C, float2 D)
{
    ASSERT(orient(A, B, C) >= 0 && 
           orient(B, C, D) >= 0 &&
           orient(C, D, A) >= 0);

    return (orient(P, A, B) >= 0 &&
            orient(P, B, C) >= 0 &&
            orient(P, C, D) >= 0 &&
            orient(P, D, A) >= 0);
}

// angle must be acute
inline bool intersectSectorPointAcute(float2 ap, float ad, float aa, float2 cp)
{
    return orient2(angleToVector(ad + aa), cp - ap) <= 0.f &&
        orient2(angleToVector(ad - aa), cp - ap) >= 0.f;
}

// sector starting at AP, +- angle AA from angle AD (infinite radius)
inline bool intersectSectorPoint1(float2 ap, float ad, float aa, float2 cp)
{
    if (aa > M_PIf/2.f) {
        return !intersectSectorPointAcute(ap, ad + M_PIf, M_PIf - aa, cp);
    } else {
        return intersectSectorPointAcute(ap, ad, aa, cp);
    }
}

// sector starting at AP, radius AR, +- angle AA from angle AD
inline bool intersectSectorPoint(float2 ap, float ar, float ad, float aa, float2 cp)
{
    return intersectPointCircle(cp, ap, ar) && intersectSectorPoint1(ap, ad, aa, cp);
}

// sector position, radius, direction, angle
bool intersectSectorCircle(float2 ap, float ar, float ad, float aa, float2 cp, float cr);

////////////////////////////////////////////////////////////////////////////



// http://en.wikipedia.org/wiki/False_position_method#Numerical_analysis
template <typename Fun>
static double findRootRegulaFalsi(const Fun& fun, double lo, double hi, double error)
{
    double fun_hi = fun(hi);
    double fun_lo = fun(lo);

    ASSERT(fun_lo <= 0 && 0 <= fun_hi);
    ASSERT(error > 0);

    double est;
    double fun_est;

    if (fun_hi < -fun_lo) {
        est     = hi;
        fun_est = fun_hi;
    } else {
        est     = lo;
        fun_est = fun_lo;
    }

    while (fabsf(fun_est) > error)
    {
        est     = hi - (fun_hi * (hi - lo)) / (fun_hi - fun_lo);
        fun_est = fun(est);
        if (fun_est > 0) {
            hi     = est;
            fun_hi = fun_est;
        } else {
            lo     = est;
            fun_lo = fun_est;
        }
    }
    return est;
}

// return number of real solutions, with values in r0 (smaller) and r1 (larger)
// x where a^2x + bx + c = 0
int quadraticFormula(double* r0, double* r1, double a, double b, double c);


#endif
