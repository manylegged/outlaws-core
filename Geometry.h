
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
    template <typename T>
    inline int isnan(T v) { return _isnan(v); }
    template <typename T>
    inline int isinf(T v) { return !_finite(v); }
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
inline T lerp(const T &from, const T &to, float v) { return (1.f - v) * from + v * to; }

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

// map unorm to a smooth bell curve shape (0->0, 0.5->1, 1->0)
inline float bellcurve(float x)
{
    return 0.5f * (-cos(M_TAOf * x) + 1);
}

struct Circle { float2 pos; float radius; };

inline float lnorm(float val, float low, float high)
{
    if (low >= high)
        return 1.f;
    val = clamp(val, low, high);
    return (val - low) / (high - low);
}

inline float parabola(float x, float roota, float rootb) { return (x-roota) * (x-rootb); }



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
inline int intersectCircleCircle(float2 *ra, float2 *rb, const float2 &p, float pr, const float2 &c, float cr)
{
    if (!intersectCircleCircle(p, pr, c, cr))
        return 0;
    float2 c2  = c - p;         // position of c relative to p
    float dist = length(c2);
    if (dist < epsilon && fabsf(pr - cr) < epsilon) {
        // coincident
        return 3;
    } else if (dist < max(pr, cr) - min(pr, cr)) {
        // fully contained
        return 0;
    } else if (fabsf(dist - (pr + cr)) < epsilon) {
        // just touching
        *ra = p + (pr/dist) * c2;
        return 1;
    }

    // with p at origin and c at (dist, 0)
    float x = (dist*dist - cr*cr + pr*pr) / (2.f * dist);
    float y = sqrt(pr*pr - x*x); // plus and minus
    
    float2 ydir   = rotate90(c2 / dist);
    float2 center = p + (x / dist) * c2;

    *ra = center + ydir * y;
    *rb = center - ydir * y;
    return 2;
}

inline bool intersectSegmentSegment(float2 a1, float2 a2, float2 b1, float2 b2)
{
	const float div = (b2.y - b1.y) * (a2.x - a1.x) - (b2.x - b1.x) * (a2.y - a1.y);
	if (fabsf(div) < 0.00001)
		return false; // parallel
    
	const float ua = ((b2.x - b1.x) * (a1.y - b1.y) - (b2.y - b1.y) * (a1.x - b1.x)) / div;
	const float ub = ((a2.x - a1.x) * (a1.y - b1.y) - (a2.y - a1.y) * (a1.x - b1.x)) / div;
    
	return (0 <= ua && ua <= 1) && (0 <= ub && ub <= 1);
}
inline bool intersectSegmentSegment(float2 *o, float2 a1, float2 a2, float2 b1, float2 b2)
{
	const float div = (b2.y - b1.y) * (a2.x - a1.x) - (b2.x - b1.x) * (a2.y - a1.y);
	if (std::fabs(div) < 0.00001)
		return false; // parallel
    
	const float ua = ((b2.x - b1.x) * (a1.y - b1.y) - (b2.y - b1.y) * (a1.x - b1.x)) / div;
	const float ub = ((a2.x - a1.x) * (a1.y - b1.y) - (a2.y - a1.y) * (a1.x - b1.x)) / div;
    
	bool intersecting = (0 <= ua && ua <= 1) && (0 <= ub && ub <= 1);
    if (intersecting && o)
        *o = lerp(a1, a2, ua);
    return intersecting;
}

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

/*               Return a positive value if the point pd lies inside the     */
/*               circle passing through pa, pb, and pc; a negative value if  */
/*               it lies outside; and zero if the four points are cocircular.*/
/*               The points pa, pb, and pc must be in counterclockwise       */
/*               order, or the sign of the result will be reversed.          */
inline float incircle(float2 pa, float2 pb, float2 pc, float2 pd)
{
    double adx, ady, bdx, bdy, cdx, cdy;
    double abdet, bcdet, cadet;
    double alift, blift, clift;
    
    adx = pa.x - pd.x;
    ady = pa.y - pd.y;
    bdx = pb.x - pd.x;
    bdy = pb.y - pd.y;
    cdx = pc.x - pd.x;
    cdy = pc.y - pd.y;
    
    abdet = adx * bdy - bdx * ady;
    bcdet = bdx * cdy - cdx * bdy;
    cadet = cdx * ady - adx * cdy;
    alift = adx * adx + ady * ady;
    blift = bdx * bdx + bdy * bdy;
    clift = cdx * cdx + cdy * cdy;
    
    return alift * bcdet + blift * cadet + clift * abdet;
}


// Graham's scan
inline int convexHull(vector<float2> &points)
{
    if (points.size() < 3)
        return 0;
    if (points.size() == 3)
        return 3;
        
    float minY  = std::numeric_limits<float>::max();
    int   minYI = -1;
    
    for (int i=0; i<points.size(); i++)
    {
        if (points[i].y < minY) {
            minY = points[i].y;
            minYI = i;
        }
    }
    swap(points[0], points[minYI]);
    const float2 points0 = points[0];
    std::sort(points.begin() + 1, points.end(),
              [points0](float2 a, float2 b)
              {
                  return vectorToAngle(a - points0) < vectorToAngle(b - points0);
              });
    vector<float2>::iterator nend = std::unique(points.begin(), points.end(),
                                                [](float2 a, float2 b) { return isZero(a - b); });
    
    const size_t N = std::distance(points.begin(), nend);
    long M = 1;
    for (int i=1; i<N; i++)
    {
        while (orient(points[M-1], points[M], points[i]) < 0)
        {
            if (M > 1)
                M--;
            else if (i == N-1)
                return 0;
            else
                i++;
        }
        M++;
        swap(points[M], points[i]);
    }
    if (M < 2)
        return 0;
    return M+1;
}

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
inline bool intersectRayCircle(float2 *o, float2 E, float2 d, float2 C, float r)
{
    float2 f = E - C;
    d = normalize(d) * (r + length(f));

    float a = dot(d, d);
    float b = 2 * dot(f, d);
    float c = dot(f, f) - r * r;

    float discriminant = b*b-4*a*c;
    if (discriminant < 0)
        return false;

    // ray didn't totally miss sphere, so there is a solution to the equation.

    discriminant = sqrt( discriminant );

    // either solution may be on or off the ray so need to test both t1 is always the smaller value,
    // because BOTH discriminant and a are nonnegative.
    float t1 = (-b - discriminant)/(2*a);
    float t2 = (-b + discriminant)/(2*a);

    if( t1 >= 0 && t1 <= 1 )
    {
        // t1 is an intersection, and if it hits, it's closer than t2 would be
        if (o)
            *o = E + t1 * d;
        return true ;
    }

    // here t1 didn't intersect so we are either started inside the sphere or completely past it
    if( t2 >= 0 && t2 <= 1 )
    {
        if (o)
            *o = E + t2 * d;
        return true ;
    }

    return false;
}

inline bool intersectRaySegment(float2 rpt, float2 rdir, float2 sa, float2 sb)
{
    float t = ((rdir.x * rpt.y + rdir.y * (sa.x - rpt.x)) - (rdir.x * sb.y)) / 
              (rdir.y * (sa.x + sb.x) - rdir.x * (sa.y + sb.y));
    return 0.f <= t && t <= 1.f;
}

// a and b are the center of each rectangle, and ar and br are the distance from the center to each edge
inline bool intersectRectangleRectangle(const float2 &a, const float2 &ar, const float2 &b, const float2 &br)
{
    return (fabsf(a.x - b.x) < (ar.x + br.x) &&
            fabsf(a.y - b.y) < (ar.y + br.y));
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
    return fabsf(p.x - b.x) < br.x && fabsf(p.y - b.y) < br.y;
}

inline bool containedCircleInRectangle(const float2 &circle, float circleR, const float2 &rectP, const float2 &rectR)
{
    return intersectPointRectangle(circle, rectP, rectR - float2(circleR));
}

inline float intersectBBSegmentV(float bbl, float bbb, float bbr, float bbt, float2 a, float2 b)
{
	float idx = 1.0f/(b.x - a.x);
	float tx1 = (bbl == a.x ? -INFINITY : (bbl - a.x)*idx);
	float tx2 = (bbr == a.x ?  INFINITY : (bbr - a.x)*idx);
	float txmin = min(tx1, tx2);
	float txmax = max(tx1, tx2);
	
	float idy = 1.0f/(b.y - a.y);
	float ty1 = (bbb == a.y ? -INFINITY : (bbb - a.y)*idy);
	float ty2 = (bbt == a.y ?  INFINITY : (bbt - a.y)*idy);
	float tymin = min(ty1, ty2);
	float tymax = max(ty1, ty2);
	
	if(tymin <= txmax && txmin <= tymax){
		float minv = max(txmin, tymin);
		float maxv = min(txmax, tymax);
		
		if(0.0 <= maxv && minv <= 1.0) 
            return (minv > 0.f) ? minv : min(maxv, 1.f);
	}
	
	return INFINITY;
}

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
inline bool intersectPointTriangle(float2 P, float2 A, float2 B, float2 C)
{
    // Compute vectors        
    float2 v0 = C - A;
    float2 v1 = B - A;
    float2 v2 = P - A;

    // Compute dot products
    float dot00 = dot(v0, v0);
    float dot01 = dot(v0, v1);
    float dot02 = dot(v0, v2);
    float dot11 = dot(v1, v1);
    float dot12 = dot(v1, v2);

    // Compute barycentric coordinates
    float invDenom = 1 / (dot00 * dot11 - dot01 * dot01);
    float u = (dot11 * dot02 - dot01 * dot12) * invDenom;
    float v = (dot00 * dot12 - dot01 * dot02) * invDenom;

    // Check if point is in triangle
    return (u >= 0) && (v >= 0) && (u + v < 1);
}

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
inline int quadraticFormula(double* r0, double* r1, double a, double b, double c)
{
    if (isZero(a)) {
        *r0 = isZero(b) ? (-c) : (-c / b);
        return 1;
    }
    
    const double discr = b*b - 4*a*c;
    if (discr < 0) {
        return 0;
    } else if (discr == 0) {
        *r0 = -b / (2*a);
        return 1;
    }
    
    const double sqrt_discr = sqrt(discr);
    *r0 = (-b - sqrt_discr) / (2*a);
    *r1 = (-b + sqrt_discr) / (2*a);
    return 2;
}



#endif
