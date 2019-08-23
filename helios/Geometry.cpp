//
// Geometry.cpp - general purpose geometry and math
//

// Copyright (c) 2013-2016 Arthur Danskin
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
#include "Geometry.h"

#define SIMPLEX_CPP_ONLY 1
#undef SIMPLEX_H_INCLUDED
#include "../libs/SimplexNoise/include/Simplex.h"

#include "Test.h"

template struct glm::tvec2<float>;
template struct glm::tvec2<double>;
template struct glm::tvec2<int>;
template struct glm::tvec3<float>;
template struct glm::tvec3<double>;
template struct glm::tvec3<int>;
template struct glm::tvec4<float>;
template struct glm::tvec4<double>;
template struct glm::tvec4<int>;

template struct AABB_<f3>;
template struct AABB_<f2>;

template struct BSphere_<f3>;
template struct BSphere_<f2>;

#define MYINF std::numeric_limits<float>::max()

// intersect two circles, returning number of intersections with points in RA and RB
int intersectCircleCircle(float2 *ra, float2 *rb, const float2 &p, float pr, const float2 &c, float cr)
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

float2 intersectRayCircle2(float2 E, float2 d, float2 C, float r, float2 *N)
{
    float2 f = E - C;
    float dscale = ((r + length(f) + epsilonf) / length(d));
    d *= dscale;

    float a = dot(d, d);
    float b = 2 * dot(f, d);
    float c = dot(f, f) - r * r;

    float discriminant = b*b-4*a*c;
    if (discriminant < 0)
        return f2(0.f);
    discriminant = sqrt( discriminant );

    float t1 = (-b - discriminant)/(2*a);
    float t2 = (-b + discriminant)/(2*a);

    if (0.f <= t1 && t1 <= 1.f) {
        if (N)
            *N = normalize(E + d * t1 * dscale - C);
    } else
        t1 = 0.f;

    if (0.f <= t2 && t2 <= 1.f) {
        if (N)
            *N = normalize(C - E + d * t2 * dscale);
    } else
        t2 = 0.f;
    
    return f2(t1, t2) * dscale;
}

static float2 intersectRaySegment2(float2 ap, float2 ad, float2 b1, float2 b2)
{
    const float div = (b2.y - b1.y) * ad.x - (b2.x - b1.x) * ad.y;
    if (std::fabs(div) < epsilonf)
        return f2(); // parallel
    const float ua = ((b2.x - b1.x) * (ap.y - b1.y) - (b2.y - b1.y) * (ap.x - b1.x)) / div;
    if (epsilonf >= ua)                // do not intersect if ray starts inside segment
        return f2();
    const float ub = (ad.x * (ap.y - b1.y) - ad.y * (ap.x - b1.x)) / div;
    if (!(0.f <= ub && ub <= 1.f))
        return f2();
    return f2(ua, ub);
}

float intersectRaySegment(float2 ap, float2 ad, float2 b1, float2 b2)
{
    return intersectRaySegment2(ap, ad, b1, b2).x;
}

float intersectSegmentRay(float2 b1, float2 b2, float2 ap, float2 ad)
{
    return intersectRaySegment2(ap, ad, b1, b2).y;
}

bool intersectSegmentSegment(float2 *o, float2 a1, float2 a2, float2 b1, float2 b2)
{
    const float div = (b2.y - b1.y) * (a2.x - a1.x) - (b2.x - b1.x) * (a2.y - a1.y);
    if (std::fabs(div) < epsilonf)
        return false; // parallel

    const float ua = ((b2.x - b1.x) * (a1.y - b1.y) - (b2.y - b1.y) * (a1.x - b1.x)) / div;
    const float ub = ((a2.x - a1.x) * (a1.y - b1.y) - (a2.y - a1.y) * (a1.x - b1.x)) / div;

    bool intersecting = (0 <= ua && ua <= 1) && (0 <= ub && ub <= 1);
    if (intersecting && o)
        *o = lerp(a1, a2, ua);
    return intersecting;
}

// on intersection, return t where intersection point is lerp(a1, a2, t)
// else return 0
float intersectSegmentSegment(float2 a1, float2 a2, float2 b1, float2 b2)
{
    const float div = (b2.y - b1.y) * (a2.x - a1.x) - (b2.x - b1.x) * (a2.y - a1.y);
    if (std::fabs(div) < epsilonf)
        return 0.f; // parallel
    const float ua = ((b2.x - b1.x) * (a1.y - b1.y) - (b2.y - b1.y) * (a1.x - b1.x)) / div;
    if (!(0.f <= ua && ua <= 1.f))
        return 0.f;
    const float ub = ((a2.x - a1.x) * (a1.y - b1.y) - (a2.y - a1.y) * (a1.x - b1.x)) / div;
    if (!(0.f <= ub && ub <= 1.f))
        return 0.f;
    return ua;
}

float intersectLineLine(float2 a1, float2 a2, float2 b1, float2 b2)
{
    const float div = (b2.y - b1.y) * (a2.x - a1.x) - (b2.x - b1.x) * (a2.y - a1.y);
    if (std::fabs(div) < epsilonf)
        return 0.f; // parallel
    const float ua = ((b2.x - b1.x) * (a1.y - b1.y) - (b2.y - b1.y) * (a1.x - b1.x)) / div;
    return ua;
}

int intersectPolySegment(float2 *outp, const float2 *points, int npoints, float2 sa, float2 sb)
{
    int count = 0;
    for (int i=1; i<npoints; i++) {
        if (intersectSegmentSegment(&outp[count], points[i-1], points[i], sa, sb))
            count++;
    }
    if (intersectSegmentSegment(&outp[count], points[npoints-1], points[0], sa, sb))
        count++;
    return count;
}

bool intersectPolyPoint(const float2 *points, int npoints, float2 ca)
{
    for (int i=0; i<npoints; i++)
    {
        if (orient(ca, points[i], points[(i+1) % npoints]) > 0)
            return false;
    }
    return true;
}

// http://stackoverflow.com/questions/1073336/circle-line-segment-collision-detection-algorithm
template <typename V, typename T=typename V::value_type>
int intersectSegmentCircle1(V *ra, V *rb, const V &sa, const V &sb, const V &ca, T cr)
{
    const V d = sb - sa;
    const V f = sa - ca;

    const T a = dot(d, d);
    const T b = 2*dot(f, d);
    const T c = dot(f, f) - cr*cr;

    T discriminant = b*b-4*a*c;
    if (discriminant < 0)
        return 0;
    discriminant = sqrt(discriminant);

    // either solution may be on or off the ray so need to test both
    // t1 is always the smaller value, because BOTH discriminant and
    // a are nonnegative.
    const T t1 = (-b - discriminant)/(2*a);
    const T t2 = (-b + discriminant)/(2*a);

    // 3x HIT cases:
    //          -o->             --|-->  |            |  --|->
    // Impale(t1 hit,t2 hit), Poke(t1 hit,t2>1), ExitWound(t1<0, t2 hit),

    // 3x MISS cases:
    //       ->  o                     o ->              | -> |
    // FallShort (t1>1,t2>1), Past (t1<0,t2<0), CompletelyInside(t1<0, t2>1)

    int hits = 0;

    if (0 <= t1 && t1 <= 1)
    {
        if (ra)
            *ra = sa + d * t1;
        hits++;
    }

    if (0 <= t2 && t2 <= 1)
    {
        V *rt = (hits ? rb : ra);
        if (rt)
            *rt = sa + d * t2;
        hits++;
    }

    return hits;
}

#define DEF_SEG_CIRCLE(N, V, T)                                  \
    int N(V *ra, V *rb, const V &sa, const V &sb, const V &ca, T cr) { \
        return intersectSegmentCircle1(ra, rb, sa, sb, ca, cr); }

DEF_SEG_CIRCLE(intersectSegmentCircle, float2, float);
DEF_SEG_CIRCLE(intersectSegmentSphere, float3, float);
DEF_SEG_CIRCLE(intersectSegmentSphere, double3, double);


/*               Return a positive value if the point pd lies inside the     */
/*               circle passing through pa, pb, and pc; a negative value if  */
/*               it lies outside; and zero if the four points are cocircular.*/
/*               The points pa, pb, and pc must be in counterclockwise       */
/*               order, or the sign of the result will be reversed.          */
float incircle(float2 pa, float2 pb, float2 pc, float2 pd)
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
int convexHull(vector<float2> &points)
{
    if (points.size() < 3)
        return 0;
    if (points.size() == 3)
        return 3;

    float minY  = MYINF;
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
                                                [](float2 a, float2 b) { return nearZero(a - b); });

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

float momentForPoly(float mass, int numVerts, const float2 *verts, float2 offset)
{
    double sum1 = 0.0f;
    double sum2 = 0.0f;
    for (int i=0; i<numVerts; i++)
    {
        double2 v1 = verts[i] + offset;
        double2 v2 = verts[(i+1)%numVerts] + offset;

        double a = cross(v2, v1);
        double b = dot(v1, v1) + dot(v1, v2) + dot(v2, v2);

        sum1 += a*b;
        sum2 += a;
    }

    return (mass*sum1)/(6.0f*sum2);
}


// ported into c++ from python source at http://doswa.com/blog/2009/07/13/circle-segment-intersectioncollision/
float2 closestPointOnSegment(float2 a, float2 b, float2 p)
{
    float2 segv   = b - a;
    float2 ptv    = p - a;
    float  seglen = length(segv);

    if (seglen < epsilonf)
        return a;

    float2 usegv  = segv / seglen;
    float  proj   = dot(ptv, usegv);

    if (proj <= 0)
        return a;
    else if (proj >= seglen)
        return b;
    else
        return a + proj * usegv;
}


float2 closestPointOnRay(const float2 &a, float2 usegv, float2 p)
{
    if (nearZero(usegv))
        return a;
    usegv = normalize(usegv);
    const float proj = dot(p - a, usegv);

    if (proj <= 0)
        return a;
    else
        return a + proj * usegv;
}

bool intersectSegmentCircle(float2 a, float2 b, float2 c, float r, float *dist)
{
    float2 closest = closestPointOnSegment(a, b, c);
    if (!intersectPointCircle(closest, c, r))
        return false;
    if (dist)
        *dist = distance(closest, c) - r;
    return true;
}





// modified from http://stackoverflow.com/questions/1073336/circle-line-collision-detection
// ray is at point E in direction d
// circle is at point C with radius r
bool intersectRayCircle(float2 *o, float2 E, float2 d, float2 C, float r)
{
    float t = intersectRayCircle(E, d, C, r);
    if (t == 0.f)
        return false;
    if (o) {
        // d = normalize(d) * (r + length(E-C));
        *o = E + t * d;
    }
    return t;
}

float intersectRayCircle(float2 E, float2 d, float2 C, float r, float2 *N)
{
    float2 f = E - C;
    float dscale = ((r + length(f) + epsilonf) / length(d));
    d *= dscale;
    // d *= ((r + length(f)) / dlen);
    // d = normalize(d) * (r + length(f));

    float a = dot(d, d);
    float b = 2 * dot(f, d);
    float c = dot(f, f) - r * r;

    float discriminant = b*b-4*a*c;
    if (discriminant < 0)
        return 0.f;

    // ray didn't totally miss sphere, so there is a solution to the equation.

    discriminant = sqrt( discriminant );

    // either solution may be on or off the ray so need to test both t1 is always the smaller value,
    // because BOTH discriminant and a are nonnegative.
    float t1 = (-b - discriminant)/(2*a);
    float t2 = (-b + discriminant)/(2*a);

    // t1 is an intersection, and if it hits, it's closer than t2 would be
    // here t1 didn't intersect so we are either started inside the sphere or completely past it
    float t = ( t1 > epsilonf && t1 <= 1.f ) ? t1 :
              ( t2 >= 0.f && t2 <= 1.f ) ? t2 : 0.f;

    if (t && N)
        *N = E + t * d - C;
    return t * dscale;
}

bool _intersectCircleRectangle1(const float2 &circleDistance, float circleR, const float2& rectR)
{
    if (circleDistance.x > (rectR.x + circleR) ||
        circleDistance.y > (rectR.y + circleR))
    {
        return false;
    }

    if (circleDistance.x <= rectR.x ||
        circleDistance.y <= rectR.y)
    {
        return true;
    }

    return intersectPointCircle(circleDistance, rectR, circleR);
}


float intersectBBSegmentV(float bbl, float bbb, float bbr, float bbt, float2 a, float2 b)
{
    float idx = 1.0f/(b.x - a.x);
    float tx1 = (bbl == a.x ? -MYINF : (bbl - a.x)*idx);
    float tx2 = (bbr == a.x ?  MYINF : (bbr - a.x)*idx);
    float txmin = min(tx1, tx2);
    float txmax = max(tx1, tx2);

    float idy = 1.0f/(b.y - a.y);
    float ty1 = (bbb == a.y ? -MYINF : (bbb - a.y)*idy);
    float ty2 = (bbt == a.y ?  MYINF : (bbt - a.y)*idy);
    float tymin = min(ty1, ty2);
    float tymax = max(ty1, ty2);

    if(tymin <= txmax && txmin <= tymax){
        float minv = max(txmin, tymin);
        float maxv = min(txmax, tymax);

        if(0.0 <= maxv && minv <= 1.0)
            return (minv > 0.f) ? minv : min(maxv, 1.f);
    }

    return MYINF;
}

bool intersectPointTriangle(float2 P, float2 A, float2 B, float2 C)
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

bool intersectSectorCircle(float2 ap, float ar, float ad, float aa, float2 cp, float cr)
{
    if (!intersectCircleCircle(ap, ar, cp, cr))
        return false;
    if (intersectPointCircle(ap, cp, cr)         || // sector center inside circle
        intersectSectorPoint(ap, ar, ad, aa, cp) || // circle center inside sector
        (aa >= M_PIf))                              // sector is complete circle
        return true;
    else if (aa < epsilon)           // sector is a line
        return intersectSegmentCircle(ap, ap + ar * angleToVector(ad), cp, cr);

    const float2 a0 = ap + ar * angleToVector(ad + aa);
    if (intersectSegmentCircle(ap, a0, cp, cr)) // circle intersects left edge
        return true;
    const float2 a1 = ap + ar * angleToVector(ad - aa);
    if (intersectSegmentCircle(ap, a1, cp, cr)) // circle intersects right edge
        return true;

    float2 r[2];
    const int points = intersectCircleCircle(&r[0], &r[1], ap, ar, cp, cr);
    for (uint i=0; i<points; i++) {
        if (intersectSectorPoint1(ap, ad, aa, r[i])) // circle intersects arc
            return true;
    }
    return false;
}

int quadraticFormula(double* r0, double* r1, double a, double b, double c)
{
    if (nearZero(a)) {
        *r0 = nearZero(b) ? (-c) : (-c / b);
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


float2 clamp_rect(float2 v, float2 rad)
{
    if (intersectPointRectangle(v, f2(), rad))
        return v;
    float x = intersectBBSegmentV(-rad.x, -rad.y, rad.x, rad.y, float2(0.f), v);
    return (0 <= x && x <= 1.f) ? x * v : v;
}


float2 circle_in_rect(float2 pos, float rad, float2 rcenter, float2 rrad_)
{
    float2 npos = pos;
    const float2 rrad = rrad_ - float2(rad);
    for (uint i=0; i<2; i++) {
        if (rrad[i] <= 0)
            npos[i] = rcenter[i];
        else if (npos[i] < rcenter[i] - rrad[i])
            npos[i] = rcenter[i] - rrad[i];
        else if (npos[i] > rcenter[i] + rrad[i])
            npos[i] = rcenter[i] + rrad[i];
    }

    ASSERT(containedCircleInRectangle(npos, rad-epsilon, rcenter, rrad_));
    return npos;
}

float2 rectangleEdge(float2 rpos, float2 rrad, float2 dir)
{
    float2 pos;
    const float2 end = rpos + (rrad.x + rrad.y) * normalize(dir);
    if (intersectSegmentSegment(&pos, rpos + rrad, rpos + flipY(rrad), rpos, end))
        return pos;
    if (intersectSegmentSegment(&pos, rpos + rrad, rpos + flipX(rrad), rpos, end))
        return pos;
    if (intersectSegmentSegment(&pos, rpos - rrad, rpos - flipY(rrad), rpos, end))
        return pos;
    if (intersectSegmentSegment(&pos, rpos - rrad, rpos - flipX(rrad), rpos, end))
        return pos;
    return rpos;
}

// from http://geomalgorithms.com/a05-_intersect-1.html
int intersectSegmentPlane(const d3& a, const d3& b, const d3 &p, const d3& n, d3 *I)
{
    const d3 u = b - a;
    const d3 w = a - p;

    const double D = dot(n, u);
    const double N = -dot(n, w);

    if (abs(D) < epsilon) {           // segment is parallel to plane
        if (N == 0)                      // segment lies in plane
            return 2;
        else
            return 0;                    // no intersection
    }
// they are not parallel
// compute intersect param
    const double sI = N / D;
    if (sI < 0 || sI > 1)
        return 0;                        // no intersection

    if (I)
        *I = a + sI * u;     // compute segment intersect point
    return 1;
}

float intersectSegmentPlane(const f3 &p, const f3 &q, const f4 &n)
{
    f3 a = q - p;
    float d = dot(n.xyz(), a);
    return (abs(d) < epsilon) ? 0.f : -(dot(n.xyz(), p) + n.w) / d;
}

bool intersectSegmentFrustum(const float3 &a, const float3 &b, const Frustum &f)
{
    for (int i=0; i<arraySize(f.planes); i++)
    {
        if (!intersectPointPlane(a, f.planes[i]) &&
            !intersectPointPlane(b, f.planes[i]) &&
            !intersectSegmentPlane(a, b, f.planes[i]))
            return false;
    }
    return true;
}


// https://en.wikipedia.org/wiki/M%C3%B6ller%E2%80%93Trumbore_intersection_algorithm
// orig and dir defines the ray. v0, v1, v2 defines the triangle.
// returns the distance from the ray origin to the intersection or 0.
float intersectRayTriangle(const float3& orig, const float3& dir,
                           const float3& v0, const float3& v1, const float3& v2)
{
    float3 e1 = v1 - v0;
    float3 e2 = v2 - v0;
    // Calculate planes normal vector
    float3 pvec = cross(dir, e2);
    float det = dot(e1, pvec);

    // Ray is parallel to plane
    if (det < 1e-8 && det > -1e-8) {
        return 0;
    }

    float inv_det = 1 / det;
    float3 tvec = orig - v0;
    float u = dot(tvec, pvec) * inv_det;
    if (u < 0 || u > 1) {
        return 0;
    }

    float3 qvec = cross(tvec, e1);
    float v = dot(dir, qvec) * inv_det;
    if (v < 0 || u + v > 1) {
        return 0;
    }
    return dot(e2, qvec) * inv_det;
}


// http://psgraphics.blogspot.com/2016/02/new-simple-ray-box-test-from-andrew.html

template <typename V, typename T=typename V::value_type>
bool intersectRayAABB_(const V& orig, const V &dir, const AABB_<V> &b, T *dist)
{
    T tmin = 0.f;
    T tmax = std::numeric_limits<T>::max();

    for (int a = 0; a < V::components; a++)
    {
        T invD = T(1.0) / dir[a];
        T t0 = (b.mn[a] - orig[a]) * invD;
        T t1 = (b.mx[a] - orig[a]) * invD;
        if (invD < T(0.0))
            std::swap(t0, t1);
        tmin = max(tmin, t0);
        tmax = min(tmax, t1);
        if (tmax <= tmin)
            return false;
    }
    if (dist)
        *dist = tmin;
    return true;
}

bool intersectRayAABB(const float3& orig, const float3 &dir, const AABB3 &b, float *dist)
{
    return intersectRayAABB_(orig, dir, b, dist);
}

bool intersectRayAABB(const float2& orig, const float2 &dir, const AABB2 &b, float* dist)
{
    return intersectRayAABB_(orig, dir, b, dist);
}


// https://www.scratchapixel.com/lessons/3d-basic-rendering/minimal-ray-tracer-rendering-simple-shapes/ray-sphere-intersection
static bool solveQuadratic(const float &a, const float &b, const float &c, float &x0, float &x1)
{
    float discr = b * b - 4 * a * c;
    if (discr < 0) return false;
    else if (discr == 0) x0 = x1 = - 0.5 * b / a;
    else {
        float q = (b > 0) ?
            -0.5 * (b + sqrt(discr)) :
            -0.5 * (b - sqrt(discr));
        x0 = q / a;
        x1 = c / q;
    }
    if (x0 > x1) std::swap(x0, x1);

    return true;
}

bool intersectRaySphere(const f3 &orig, const f3 &dir, const f3 &center, float radius, float *dist)
{
    float t0, t1; // solutions for t if the ray intersects
    const float radius2 = radius * radius;
#if 0
    // geometric solution
    float3 L = center - orig;
    float tca = dot(L, dir);
    // if (tca < 0) return false;
    float d = dot(L, L) - tca * tca;
    if (d > radius2)
        return false;
    float thc = sqrt(radius2 - d);
    t0 = tca - thc;
    t1 = tca + thc;
#else
    // analytic solution
    float3 L = orig - center;
    float a = dot(dir, dir);
    float b = 2 * dot(dir, L);
    float c = dot(L, L) - radius2;
    if (!solveQuadratic(a, b, c, t0, t1))
        return false;
#endif
    if (t0 > t1) std::swap(t0, t1);

    if (t0 < 0) {
        t0 = t1; // if t0 is negative, let's use t1 instead
        if (t0 < 0)
            return false; // both t0 and t1 are negative
    }

    if (dist)
        *dist = t0;

    return true;
}

float2 intersectRaySphere2(const f3 &orig, const f3 &dir, const f3 &center, float radius)
{
    float2 t;                   // solutions for t if the ray intersects
    float3 L = orig - center;
    float a = dot(dir, dir);
    float b = 2 * dot(dir, L);
    float c = dot(L, L) - radius * radius;
    if (!solveQuadratic(a, b, c, t.x, t.y))
        return t;
    t.x = max(t.x, 0.f);
    t.y = max(t.y, 0.f);
    if (t.x > t.y)
        swap(t.x, t.y);
    return t;
}


// a point is "inside" the plane if it is in the direction of the plane normal from the plane
bool intersectPointPlane(const f3 &bb, const float4 &p)
{
    return dot(p.xyz(), bb) >= -p.w;
}

int intersectTrianglePlane(const f3* tri, const float4 &plane)
{
    bool a = intersectPointPlane(tri[0], plane);
    bool b = intersectPointPlane(tri[1], plane);
    bool c = intersectPointPlane(tri[2], plane);
    return (a && b && c) ? 1 :
        (!a && !b && !c) ? -1 : 0;
}

// a point is "inside" the plane if it is in the direction of the plane normal from the plane
bool intersectAABBPlane(const AABB &bb, const float4 &p)
{
    float3 pvert = f3((p.x > 0.f) ? bb.mx.x : bb.mn.x,
                      (p.y > 0.f) ? bb.mx.y : bb.mn.y,
                      (p.z > 0.f) ? bb.mx.z : bb.mn.z);
    return dot(p.xyz(), pvert) >= -p.w;
}

// the frustum planes point inwards
bool intersectAABBFrustum(const AABB &bb, const Frustum &f)
{
    for (int i=0; i<arraySize(f.planes); i++)
    {
        if (!intersectAABBPlane(bb, f.planes[i]))
            return false;
    }
    return true;
}

// point/plane distance is positive if we are inside the plane
bool intersectSpherePlane(const float3 &c, float r, const float4 &p)
{
    // DASSERT(nearZero(length(p.xyz()) - 1.f));
    return dot(p, f4(c, 1.f)) + r >= 0;
}


bool intersectSphereFrustum(const float3 &c, float r, const Frustum &f)
{
    for (int i=0; i<arraySize(f.planes); i++)
    {
        if (!intersectSpherePlane(c, r, f.planes[i]))
            return false;
    }
    return true;
}

bool intersectCircleAABB(const float2 &p, float r, const AABB2 &b, float *dist)
{
    float2 b0 = clamp(p, b.mn, b.mx);
    bool res = intersectPointCircle(b0, p, r);
	if (res && dist)
    {
        f2 d = abs(p - b.center()) - b.radius();
        *dist = min(max_dim(d), 0.f) + length(max(d, f2())) - r;
    }
    return res;
}

bool intersectSphereAABB(const float3 &p, float r, const AABB3 &b, float *dist)
{
    float3 b0 = clamp(p, b.mn, b.mx);
    bool res = intersectPointSphere(b0, p, r);
	if (res && dist)
    {
        f3 d = abs(p - b.center()) - b.radius();
        *dist = min(max_dim(d), 0.f) + length(max(d, f3())) - r;
    }
    return res;
}


bool intersectAABB(const AABB3 &a, const AABB3 &b, float *dist)
{
    bool res = (a.mx.x >= b.mn.x && a.mn.x <= b.mx.x &&
                a.mx.y >= b.mn.y && a.mn.y <= b.mx.y &&
                a.mx.z >= b.mn.z && a.mn.z <= b.mx.z);
    if (res && dist)
        *dist = max_dim(abs(a.getCenter() - b.getCenter()) - (a.getRadius() + b.getRadius()));
    return res;
}

bool intersectAABB(const AABB2 &a, const AABB2 &b, float *dist)
{
    bool res = (a.mx.x >= b.mn.x && a.mn.x <= b.mx.x &&
                a.mx.y >= b.mn.y && a.mn.y <= b.mx.y);
    if (res && dist)
    {
        *dist = max_dim(abs(a.getCenter() - b.getCenter()) - (a.getRadius() + b.getRadius()));
    }
    
    return res;
}


bool intersectRayPlane(const f3& orig, const f3 &dir, const f4 &N, float *dist)
{
    float d = dot(dir, N.xyz());
    if (abs(d) < epsilonf)
        return false;
    float t = -(dot(orig, N.xyz()) + N.w) / d;
    if (dist)
        *dist = t;
    return t > epsilonf;
}


// ported from glsl https://github.com/ashima/webgl-noise

using glm::vec2;
using glm::vec3;
using glm::vec4;
using glm::fract;

template <typename T>
T mod289(const T &x)
{
    return x - floor(x * (1.f / 289.f)) * 289.f;
}

template <typename T>
static T permute(const T &x)
{
    return mod289(((x*34.f)+1.f)*x);
}

static vec4 taylorInvSqrt(const vec4 &r)
{
    return 1.79284291400159f - 0.85373472095314f * r;
}

// output range is about -1 to 1
float snoise(const vec2 &v)
{
    const vec4 C = vec4(0.211324865405187f,  // (3.0-sqrt(3.0))/6.0
                        0.366025403784439f,  // 0.5*(sqrt(3.0)-1.0)
                        -0.577350269189626f,  // -1.0 + 2.0 * C.x
                        0.024390243902439f); // 1.0 / 41.0
    // First corner
    vec2 i  = floor(v + dot(v, C.yy()) );
    vec2 x0 = v -   i + dot(i, C.xx());

    // Other corners
    vec2 i1;
    //i1.x = step( x0.y, x0.x ); // x0.x > x0.y ? 1.0 : 0.0
    //i1.y = 1.0 - i1.x;
    i1 = (x0.x > x0.y) ? vec2(1.f, 0.f) : vec2(0.f, 1.f);
    // x0 = x0 - 0.0 + 0.0 * C.xx ;
    // x1 = x0 - i1 + 1.0 * C.xx ;
    // x2 = x0 - 1.0 + 2.0 * C.xx ;
    vec4 x12 = x0.xyxy() + C.xxzz();
    x12.x -= i1.x;
    x12.y -= i1.y;

    // Permutations
    i = mod289(i); // Avoid truncation effects in permutation
    vec3 p = permute( permute( i.y + vec3(0.f, i1.y, 1.f ))
                      + i.x + vec3(0.f, i1.x, 1.f ));

    vec3 m = max(0.5f - vec3(dot(x0,x0), dot(x12.xy(),x12.xy()), dot(x12.zw(),x12.zw())), vec3(0.f));
    m = m*m ;
    m = m*m ;

    // Gradients: 41 points uniformly over a line, mapped onto a diamond.
    // The ring size 17*17 = 289 is close to a multiple of 41 (41*7 = 287)

    vec3 x = 2.f * fract(p * C.www()) - 1.f;
    vec3 h = abs(x) - 0.5f;
    vec3 ox = floor(x + 0.5f);
    vec3 a0 = x - ox;

    // Normalise gradients implicitly by scaling m
    // Approximation of: m *= inversesqrt( a0*a0 + h*h );
    m *= 1.79284291400159f - 0.85373472095314f * ( a0*a0 + h*h );

    // Compute final noise value at P
    vec3 g;
    g.x  = a0.x  * x0.x  + h.x  * x0.y;
    vec2 g_yz = a0.yz() * x12.xz() + h.yz() * x12.yw();
    g.y = g_yz.x;
    g.z = g_yz.y;
    return 130.f * dot(m, g);
}

float snoise(const vec3 &v)
{
    const vec2  C = vec2(1.0/6.0, 1.0/3.0) ;
    const vec4  D = vec4(0.0, 0.5, 1.0, 2.0);

// First corner
    vec3 i  = floor(v + dot(v, vec3(C.y)) );
    vec3 x0 =   v - i + dot(i, vec3(C.x)) ;

// Other corners
    vec3 g = step(vec3(x0.y, x0.z, x0.x), x0);
    vec3 l = vec3(1.0f) - g;
    vec3 i1 = min( g, vec3(l.z, l.x, l.y) );
    vec3 i2_ = max( g, vec3(l.z, l.x, l.y) );

    //   x0 = x0 - 0.0 + 0.0 * C.xxx;
    //   x1 = x0 - i1  + 1.0 * C.xxx;
    //   x2 = x0 - i2_  + 2.0 * C.xxx;
    //   x3 = x0 - 1.0 + 3.0 * C.xxx;
    vec3 x1 = x0 - i1 + vec3(C.x);
    vec3 x2 = x0 - i2_ + vec3(C.y); // 2.0*C.x = 1/3 = C.y
    vec3 x3 = x0 - vec3(D.y);      // -1.0+3.0*C.x = -0.5 = -D.y

// Permutations
    i = mod289(i);
    vec4 p = permute( permute( permute(
        i.z + vec4(0.0, i1.z, i2_.z, 1.0 ))
      + i.y + vec4(0.0, i1.y, i2_.y, 1.0 ))
      + i.x + vec4(0.0, i1.x, i2_.x, 1.0 ));

// Gradients: 7x7 points over a square, mapped onto an octahedron.
// The ring size 17*17 = 289 is close to a multiple of 49 (49*6 = 294)
    float n_ = 0.142857142857; // 1.0/7.0
    vec3  ns = n_ * vec3(D.w, D.y, D.z) - vec3(D.x, D.z, D.x);

    vec4 j = p - 49.0f * floor(p * ns.z * ns.z);  //  mod(p,7*7)

    vec4 x_ = floor(j * ns.z);
    vec4 y_ = floor(j - 7.0f * x_ );    // mod(j,N)

    vec4 x = x_ *ns.x + vec4(ns.y);
    vec4 y = y_ *ns.x + vec4(ns.y);
    vec4 h = vec4(1.0f) - abs(x) - abs(y);

    vec4 b0 = vec4( x.x, x.y, y.x, y.y );
    vec4 b1 = vec4( x.z, x.w, y.z, y.w );

    //vec4 s0 = vec4(lessThan(b0,0.0))*2.0 - 1.0;
    //vec4 s1 = vec4(lessThan(b1,0.0))*2.0 - 1.0;
    vec4 s0 = floor(b0)*2.0f + vec4(1.0f);
    vec4 s1 = floor(b1)*2.0f + vec4(1.0f);
    vec4 sh = -step(h, vec4(0.0f));

    vec4 a0 = vec4(b0.x, b0.z, b0.y, b0.w) + vec4(s0.x, s0.z, s0.y, s0.w)*vec4(sh.x, sh.x, sh.y, sh.y) ;
    vec4 a1 = vec4(b1.x, b1.z, b1.y, b1.w) + vec4(s1.x, s1.z, s1.y, s1.w)*vec4(sh.z, sh.z, sh.w, sh.w) ;

    vec3 p0 = vec3(a0.x, a0.y, h.x);
    vec3 p1 = vec3(a0.z, a0.w, h.y);
    vec3 p2 = vec3(a1.x, a1.y, h.z);
    vec3 p3 = vec3(a1.z, a1.w, h.w);

//Normalise gradients
    vec4 norm = taylorInvSqrt(vec4(dot(p0,p0), dot(p1,p1), dot(p2, p2), dot(p3,p3)));
    p0 *= norm.x;
    p1 *= norm.y;
    p2 *= norm.z;
    p3 *= norm.w;

// Mix final noise value
    vec4 m = max(vec4(0.6f) - vec4(dot(x0,x0), dot(x1,x1), dot(x2,x2), dot(x3,x3)), 0.0f);
    m = m * m;
    return 42.0f * dot( m*m, vec4( dot(p0,x0), dot(p1,x1),
                                   dot(p2,x2), dot(p3,x3) ) );
}


#include "RGB.h"

DEF_TEST(math)
{
    DASSERT(distanceRgb(0xff0000, 0xff0000) == 0.f);
    DASSERT(distanceRgb(0xff0000, 0x00ff00) > 0.7f);
    DASSERT(distanceRgb(0xff0000, 0xffffff) > 0.9f);
    DASSERT(distanceRgb(0xff0000, 0x000000) > 0.9f);

    for (float x=-10.f; x<10.f; x += 0.1f)
    {
        DASSERT(floor_int(x) == (int) floor(x));
        DASSERT(ceil_int(x) == (int) ceil(x));
    }

    const f3 origins[] = { f3(), f3(100.f, 100.f, 100.f), f3(-100.f, -100.f, -100.f) };
    const f3 dirs[] = { f3(1, 0, 0), f3(-1, 0, 0), f3(0, 1, 0),
                        f3(0, -1, 0), f3(0, 0, 1), f3(0, 0, -1),
                        f3(1, 1, 1), f3(1, 1, -1), f3(1, -1, -1), f3(1, -1, 1),
                        f3(-1, 1, 1), f3(-1, -1, 1), f3(-1, -1, -1), f3(-1, 1, -1) };
    for (int i=0; i<arraySize(origins); i++)
    {
        const f3   o  = origins[i];
        const AABB bb = AABB(o - 1.f, o + 1.f);
        ASSERT(intersectSphereAABB(o, 1.f, bb));
        for (int j=0; j<arraySize(dirs); j++)
        {
            f3 r = dirs[j];
            f3 p = o + 0.5f * r;
            ASSERT(intersectAABB(p, bb));
            ASSERT(intersectSphereAABB(p, 0.5f, bb));
            p = o + 2.001f * r;
            ASSERT(!intersectSphereAABB(p, 1.f, bb));
            ASSERT(!intersectAABB(AABB(p - 1.f, p + 1.f), bb));
            ASSERT(!intersectAABB(AABB(p), bb));
            ASSERT(intersectRayAABB(p, -r, bb));
            ASSERT(!intersectRayAABB(p, r, bb));
        }
    }

    float angles[] = { 0, 1, 2, 3, 4, 5, 6 };
    for_ (an, angles) {
        TEST_EQL(distanceAngles(an, an), 0.f);
        TEST_EQL(distanceAngles(an, an + 2.f), 2.f);
        TEST_EQL(distanceAngles(an, an - 2.f), -2.f);
        TEST_EQL(distanceAngles(an, an + 4.f), 4.f - M_TAUf);

        TEST_EQL(distanceRots(a2v(an), a2v(an)), 0.f);
        TEST_EQL(distanceRots(a2v(an), a2v(an + 2.f)), 2.f);
        TEST_EQL(distanceRots(a2v(an), a2v(an - 2.f)), -2.f);
        TEST_EQL(distanceRots(a2v(an), a2v(an + 4.f)), 4.f - M_TAUf);
    }
}

/////////////////// Rand.h ////////////////////////////////////////////////

int& random_seed()
{
    static int seed = 0;
    return seed;
}


std::mt19937 *&my_random_device()
{
// static std::default_random_engine e1;
    static THREAD_LOCAL std::mt19937 *e1 = NULL;
    if (!e1)
        e1 = new std::mt19937(random_seed());
    return e1;
}


float2 randpolar_uniform(float minradius, float maxradius)
{
    if (maxradius < epsilon)
        return float2(0.f);
    const float r = maxradius - minradius;
    if (r < epsilon)
        return randpolar(minradius);
    float2 pos;
    do { pos = float2(randrange(-r, r), randrange(-r, r));
    } while (!intersectPointCircle(pos, float2(0), r));
    pos += normalize(pos) * minradius;
    return pos;
}

// Box-Muller transform
// adapted from http://en.literateprograms.org/Box-Muller_transform_(C)#chunk def:Apply Box-Muller transform on x, y
float rand_normal(float mean, float stddev)
{
    static THREAD_LOCAL float n2 = 0.0;
    static THREAD_LOCAL int n2_cached = 0;
    if (n2_cached)
    {
        n2_cached = 0;
        return n2*stddev + mean;
    }

    float x, y, r;
    do {
        x = randrange(-1.f, 1.f);
        y = randrange(-1.f, 1.f);
        r = x*x + y*y;
    } while (r == 0.0 || r > 1.0);

    float d = sqrtf(-2.f*logf(r)/r);
    float n1 = x*d;
    n2 = y*d;
    n2_cached = 1;
    float result = n1*stddev + mean;
    return result;
}

float2 randcircle()
{
    float x, y, r;
    do {
        x = randrange(-1.f, 1.f);
        y = randrange(-1.f, 1.f);
        r = x*x + y*y;
    } while (r == 0.0 || r > 1.0);
    
    return f2(x, y);
}


void LinearRegression::insert(float2 p)
{
    sum += d2(p);
    sumx += double(p.x) * d2(p);
    count++;
}

SlopeLine LinearRegression::calculate() const
{
    SlopeLine sl;
    if (count)
    {
        double denom = (count * sumx.x - sum.x*sum.x);
        if (nearZero(denom))
            denom = epsilon;
        sl.slope = (count * sumx.y - sum.x * sum.y) / denom;
        sl.y_int = (sum.y/count) - sl.slope * (sum.x/count);
    }
    ASSERT(!fpu_error(sl.slope) && !fpu_error(sl.y_int));
    return sl;
}

void Variance::insert(double val, double weight)
{
    const double sw = weight + sumweight;
    const double delta = val - mean;
    mean += delta * weight / sw;
    m2 += weight * delta * (val - mean);
    sumweight = sw;
    n++;
}

double Variance::variance() const
{
    if (n < 2)
        return 0;
    return (n * m2) / (sumweight * (n-1));
}

// https://www.dsprelated.com/showthread/comp.dsp/97276-1.php
void VarianceWindow::insert(double newv)
{
    double oldv = 0.0;
    if (data.size() >= size)
    {
        oldv = data.front();
        data.pop_front();
    }
    data.push_back(newv);
    sumv += newv - oldv;
    sumv2 += newv * newv - oldv * oldv;
    double N = data.size();
    mean = sumv / N;
    if (N > 1)
        variance = (N * sumv2 - (sumv * sumv)) / (N * (N - 1.0));
}


static const double isqrt2 = 0.70710676908493042;

static double3 cubify(const double3 &s)
{
    const double xx2 = s.x * s.x * 2.0;
    const double yy2 = s.y * s.y * 2.0;

    double2 v = d2(xx2 - yy2, yy2 - xx2);

    double ii = v.y - 3.0;
    ii *= ii;

    const double isqrt = -sqrt(max(0.0, ii - 12.0 * xx2)) + 3.0;

    v = sqrt(max(d2(0.0), v + isqrt));
    v *= isqrt2;

    return sign(s) * d3(v, 1.0);
}

double3 sphere2cube(const double3 &s)
{
    const double3 f = abs(s);

    if (f.y >= f.x && f.y >= f.z) {
        const double3 c = cubify(d3(s.x, s.z, s.y));
        return d3(c.x, c.z, c.y);
    } else if (f.x >= f.z) {
        const double3 c = cubify(d3(s.y, s.z, s.x));
        return d3(c.z, c.x, c.y);
    } else {
        return cubify(s);
    }
}


dmat2 eul2mat(double a)
{
    double cosa = cos(a);
    double sina = sin(a);
    return dmat2(cosa, sina, -sina, cosa);
}


// http://planning.cs.uiuc.edu/node102.html
// yaw (alpha, z axis), pitch (beta, y axis), roll (gamma, x axis)
// (alpha, beta, gamma) is first roll, then pitch, finally yaw
dmat3 eul2mat(const d3 &eul)
{
#if 0
    static const i2 kEul2MatOffset[3][4] = {{ i2(1, 1), i2(2, 1), i2(1, 2), i2(2, 2) },
                                         { i2(0, 0), i2(2, 0), i2(0, 2), i2(2, 2) },
                                         { i2(0, 0), i2(1, 0), i2(0, 1), i2(1, 1) } };
    static const i2 kEul2MatRotOffset[4] = { i2(0, 0), i2(0, 1), i2(1, 0), i2(1, 1) };

    dmat3 m(1.0);
    for (int i=0; i<3; i++)
    {
        dmat2 r = eul2mat(eul[i]);
        dmat3 n;
        for (int j=0; j<4; j++)
            n[kEul2MatOffset[i][j].y][kEul2MatOffset[i][j].x] = r[kEul2MatRotOffset[j].y][kEul2MatRotOffset[j].x];
        m = m * n;
    }
    return xyz[2] * xyz[1] * xyz[0];
#else
    d2 alpha = a2v(eul.x);
    d2 beta = a2v(eul.y);
    d2 gamma = a2v(eul.z);
    return dmat3(alpha.x * beta.x, alpha.y * beta.x, -beta.y,
                 alpha.x * beta.y * gamma.y - alpha.y * gamma.x, alpha.y * beta.y * gamma.y + alpha.x * gamma.x, beta.x * gamma.y,
                 alpha.x * beta.y * gamma.x + alpha.y * gamma.y, alpha.y * beta.y * gamma.x - alpha.x * gamma.y, beta.x * gamma.x);
#endif
}

d3 mat2eul(const dmat3 &m)
{
    double m21 = m[2][1];
    double m22 = m[2][2];
    return d3(atan2(m21, m22),
              atan2(-m[2][0], sqrt(m21 * m21 + m22 * m22)),
              atan2(m[1][0], m[0][0]));
}

dmat3 rotate(const d3 &k, const double &angle)
{
    d3 i, j;
    if (abs(dot(k, d3(1, 0, 0))) < 0.9)
    {
        j = cross(k, d3(1, 0, 0));
        i = cross(j, k);
    }
    else
    {
        i = cross(k, d3(0, 1, 0));
        j = cross(i, k);
    }
    return dmat3(eul2mat(angle)) * dmat3(i, j, k);
}

d3 rotate_vec(const d3 &axis, const double &angle)
{
    return dmat3(rotateV2V(d3(0.0, 0.0, 1.0), axis)) * d3(a2v(angle), 0.0);
}

d2 norm2lnglat(const d3 &pos)
{
    double lng = kToDegrees * v2a(d2(pos.x, pos.y));
    double lat = kToDegrees * asin_clamp(pos.z);
    return d2(lng, lat);
}

string fmt_lnglat(d2 ll)
{
    return str_format("%.3f %s, %.3f %s", abs(ll.x), ll.x >= 0.0 ? "E" : "W",
                      abs(ll.y), ll.y >= 0.0 ? "N" : "S");
}

d3 norm2sphere(d3 pos)
{
    const double len = length(pos);
    pos /= len;
    return d3(v2a(d2(pos.x, pos.y)), asin_clamp(pos.z), len);
}
