
//
// Geometry.h - general purpose geometry and math
// - polygon intersections and computational geometry
// - vector and numerical operations
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


#pragma once

#ifdef __clang__
#pragma clang diagnostic push
#endif

#if defined(__clang__) || defined(__GNUC__)
#pragma GCC diagnostic ignored "-Wshadow"
#endif

#ifdef assert
#undef assert
#define assert ASSERT
#endif

#define GLM_FORCE_RADIANS 1
#define GLM_FORCE_XYZW 1
#define GLM_PRECISION_HIGHP_INT 1
#define GLM_SWIZZLE 1
#define GLM_META_PROG_HELPERS 1
#include "../libs/glm/vec2.hpp"
#include "../libs/glm/vec3.hpp"
#include "../libs/glm/vec4.hpp"
#include "../libs/glm/mat2x2.hpp"
#include "../libs/glm/mat3x3.hpp"
#include "../libs/glm/mat4x4.hpp"
#include "../libs/glm/gtc/matrix_integer.hpp"
#include "../libs/glm/trigonometric.hpp"
#include "../libs/glm/exponential.hpp"
#include "../libs/glm/common.hpp"
#include "../libs/glm/geometric.hpp"
#include "../libs/glm/gtc/matrix_transform.hpp"
#include "../libs/glm/gtx/color_space.hpp"
#include "../libs/glm/gtc/random.hpp"
#include "../libs/glm/gtx/intersect.hpp"
#include "../libs/glm/gtx/rotate_vector.hpp"
#include "../libs/glm/gtc/quaternion.hpp"

#define SIMPLEX_HEADER_ONLY 1
#include "../libs/SimplexNoise/include/Simplex.h"
#undef SIMPLEX_HEADER_ONLY

#ifdef __clang__
#pragma clang diagnostic pop
#endif

extern template struct glm::tvec2<float>;
extern template struct glm::tvec2<double>;
extern template struct glm::tvec2<int>;
extern template struct glm::tvec3<float>;
extern template struct glm::tvec3<double>;
extern template struct glm::tvec3<int>;
extern template struct glm::tvec4<float>;
extern template struct glm::tvec4<double>;
extern template struct glm::tvec4<int>;

#include <cmath>
#include <algorithm>
#include <stdlib.h>
#include <string.h>
#include <cfloat>
#include "StrMath.h"

typedef unsigned char uchar;
typedef unsigned char uint8;
typedef unsigned short uint16;
typedef unsigned int uint32;
typedef unsigned long long uint64;
typedef uint32 uint;
typedef signed char int8;
typedef signed char schar;
typedef short int16;
typedef int int32;
typedef long long int64;

// ternary digit: -1, 0, or 1, (false, unknown, true)
struct trit {
    signed char val;
    
    trit() : val(0) {}
    trit(int v) : val(v > 0 ? 1 : v < 0 ? -1 : 0) {}
    trit(bool v) : val(v ? 1 : -1) {}
    
    friend trit operator!(trit a) { return trit(-a.val); }
    
    friend trit operator&&(trit a, trit b) { return (a.val < 0) ? a : b; }
    friend trit operator||(trit a, trit b) { return (a.val > 0) ? a : b; }
    friend trit operator==(trit a, trit b) { return a.val == b.val; }
    friend trit operator!=(trit a, trit b) { return a.val != b.val; }

    friend trit operator&&(trit a, int b) { return (a.val < 0) ? a : b; }
    friend trit operator||(trit a, int b) { return (a.val > 0) ? a : b; }
    friend trit operator==(trit a, int b) { return a.val == trit(b).val; }
    friend trit operator!=(trit a, int b) { return a.val != trit(b).val; }

    friend trit operator&&(int a, trit b) { return (a < 0) ? a : b; }
    friend trit operator||(int a, trit b) { return (a > 0) ? a : b; }
    friend trit operator==(int a, trit b) { return trit(a).val == b.val; }
    friend trit operator!=(int a, trit b) { return trit(a).val == b.val; }
    
    explicit operator bool() { return val > 0; }
};

typedef glm::vec2 float2;
typedef glm::vec3 float3;
typedef glm::vec4 float4;

using glm::vec2;
using glm::vec3;
using glm::vec4;

typedef glm::dvec2 double2;
typedef glm::dvec3 double3;
typedef glm::dvec4 double4;

typedef glm::ivec2 int2;
typedef glm::ivec3 int3;
typedef glm::ivec4 int4;

typedef glm::tvec2<uint> uint2;
typedef glm::tvec3<uint> uint3;
typedef glm::tvec4<uint> uint4;

typedef float2 f2;
typedef float3 f3;
typedef float4 f4;

typedef int2 i2;
typedef int3 i3;
typedef int4 i4;

typedef double2 d2;
typedef double3 d3;
typedef double4 d4;

using glm::length;
using glm::distance;
using glm::cross;
using glm::dot;
using glm::clamp;
using glm::reflect;
using glm::smoothstep;

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

using glm::mat2;
using glm::mat3;
using glm::mat4;

using glm::dmat2;
using glm::dmat3;
using glm::dmat4;

using std::sqrt;
using std::log;
using std::pow;
using std::exp;

using glm::quat;
using glm::dquat;
using glm::inverse;

// template <typename T>
// inline operator bool(const glm::tvec2<T> &v) { return v.x || v.y; }
// template <typename T>
// inline operator bool(const glm::tvec3<T> &v) { return v.x || v.y || v.z; }
// template <typename T>
// inline operator bool(const glm::tvec4<T> &v) { return v.x || v.y || v.z || v.w; }

#define VECTORIZE_R_T_P(I) glm::tvec##I<T>
#define VECTORIZE_R_T_C(I) glm::tvec##I<T>

#define VECTORIZE_R_int_P(I) glm::tvec##I<int>
#define VECTORIZE_R_int_C(I) glm::tvec##I<int>

#define VECTORIZE_R_and2(A, B) ((A) && (B))
#define VECTORIZE_R_and3(A, B, C) ((A) && (B) && (C))
#define VECTORIZE_R_and4(A, B, C, D) ((A) && (B) && (C) && (D))
#define VECTORIZE_R_and_P(I) bool
#define VECTORIZE_R_and_C(I) VECTORIZE_R_and##I

#define VECTORIZE_R_or2(A, B) ((A) || (B))
#define VECTORIZE_R_or3(A, B, C) ((A) || (B) || (C))
#define VECTORIZE_R_or4(A, B, C, D) ((A) || (B) || (C) || (D))
#define VECTORIZE_R_or_P(I) bool
#define VECTORIZE_R_or_C(I) VECTORIZE_R_or##I

#define VECTORIZE_A_V1_P(V, T) (const V &v)
#define VECTORIZE_A_V1_C(F, d) F(v.d)

#define VECTORIZE_A_V2_P(V, T) (const V &a, const V &b)
#define VECTORIZE_A_V2_C(F, d) F(a.d, b.d)

#define VECTORIZE_A_V3_P(V, T) (const V &a, const V &b, const V &c)
#define VECTORIZE_A_V3_C(F, d) F(a.d, b.d, c.d)

#define VECTORIZE_A_V1S1_P(V, T) (const V &a, const T &b)
#define VECTORIZE_A_V1S1_C(F, d) F(a.d, b)

#define VECTORIZE_A_V2S1_P(V, T) (const V &a, const V &b, const T &v)
#define VECTORIZE_A_V2S1_C(F, d) F(a.d, b.d, v)

#define VECTORIZE_1(R_P, F, P, R_C, C)                                  \
    template <typename T> R_P(2) F P(glm::tvec2<T>, T) { return R_C(2) (C(F, x), C(F, y)); } \
    template <typename T> R_P(3) F P(glm::tvec3<T>, T) { return R_C(3) (C(F, x), C(F, y), C(F, z)); } \
    template <typename T> R_P(4) F P(glm::tvec4<T>, T) { return R_C(4) (C(F, x), C(F, y), C(F, z), C(F, w)); }

#define VECTORIZE(R, F, A) VECTORIZE_1(VECTORIZE_R_##R##_P, F, VECTORIZE_A_##A##_P, VECTORIZE_R_##R##_C, VECTORIZE_A_##A##_C)


template <typename T> struct value_type_traits { typedef typename T::value_type type; };
template <> struct value_type_traits<float> { typedef float type; };
template <> struct value_type_traits<double> { typedef double type; };
template <> struct value_type_traits<int> { typedef int type; };


#ifdef _MSC_VER
#include <float.h>
namespace std {
    inline int isnan(float v) { return _isnan(v); }
    inline int isinf(float v) { return !_finite(v); }
	inline int isnan(double v) { return _isnan(v); }
	inline int isinf(double v) { return !_finite(v); }
}
#endif

#if IS_DEVEL || IS_DEBUG
template <typename T>
inline bool fpu_error(T x) { return (std::isinf(x) || std::isnan(x)); }
template <typename T>
inline bool fpu_error(const glm::tquat<T> &q) {
    return fpu_error(q.x) || fpu_error(q.y) || fpu_error(q.z) || fpu_error(q.w);
}

VECTORIZE(or, fpu_error, V1);
#else
template <typename T> inline bool fpu_error(T x) { return false; }
#endif

static const double epsilon = 0.0000001;
static const float epsilonf = 0.00001f;

///////////// round/ceil/floor floats to nearest multiple /////////////
template <typename T>
inline T round(T a, T v)
{
    if (abs(v) < T(epsilon))
        return a;
    return v * round(a / v);
}

VECTORIZE(T, round, V2);
VECTORIZE(T, round, V1S1);

inline int roundUp(int num, int mult) { return ((num + mult - 1) / mult) * mult; }
inline int roundDown(int num, int mult) { return (num / mult) * mult; }

inline uint roundUpPower2(uint v)
{
    uint i=1;
    while (i < v)
        i *= 2;
    return i;
}

inline float ceil(float a, float v)
{
    if (fabsf(v) < epsilon)
        return a;
    return v * ceil(a / v);
}

inline float floor(float a, float v)
{
    if (fabsf(v) < epsilon)
        return a;
    return v * floor(a / v);
}

inline float2 floor(float2 a, float v) { return f2(floor(a.x, v), floor(a.y, v)); }
inline float2 ceil(float2 a, float v) { return f2(ceil(a.x, v), ceil(a.y, v)); }

template <typename R=int, typename T, typename = std::enable_if_t<std::is_floating_point<T>::value>>
inline R floor_int(T f)
{
    DASSERT(std::numeric_limits<R>::lowest() < f && f < std::numeric_limits<R>::max());
    const R i = (R)f;
    return (f < T(0.0) && f != i) ? i - 1 : i;
}

template <typename R=int, typename T, typename = std::enable_if_t<std::is_floating_point<T>::value>>
inline R ceil_int(T f)
{
    DASSERT(std::numeric_limits<R>::lowest() < f && f < std::numeric_limits<R>::max());
    const R i = R(f);
    return (f >= T(0.0) && f != i) ? i + 1 : i;
}

template <typename R=int, typename T, typename = std::enable_if_t<std::is_floating_point<T>::value>>
inline R round_int(T f)
{
    DASSERT(std::numeric_limits<R>::lowest() < f && f < std::numeric_limits<R>::max());
    return (f >= T(0.0)) ? (f + T(0.49999997)) : (f - T(0.50000003));
}

VECTORIZE(int, floor_int, V1);
VECTORIZE(int, ceil_int, V1);
VECTORIZE(int, round_int, V1);

inline float2 angleToVector(float angle) { return float2(std::cos(angle), std::sin(angle)); }
inline float vectorToAngle(float2 vec) { return std::atan2(vec.y, vec.x); }

inline float2 a2v(float angle) { return float2(std::cos(angle), std::sin(angle)); }
inline float v2a(float2 vec) { return std::atan2(vec.y, vec.x); }
inline double2 a2v(double angle) { return double2(std::cos(angle), std::sin(angle)); }
inline double v2a(double2 vec) { return std::atan2(vec.y, vec.x); }
template <typename T> inline T v2a(const glm::rvec2<T> &r) { return v2a(glm::tvec2<T>(r)); }

inline float v2a(float x, float y) { return std::atan2(y, x); }
inline double v2a(double x, double y) { return std::atan2(y, x); }

// return [-1, 1] indicating how closely the angles are aligned
inline float dotAngles(float a, float b)
{
    //return dot(angleToVector(a), angleToVector(b));
    return cos(a-b);
}

#define M_PIf float(M_PI)
#define M_PI_2f float(M_PI_2)
#define M_PI_4f float(M_PI_4)
#define M_TAU (2.0 * M_PI)
#define M_TAUf float(2.0 * M_PI)
#define M_SQRT2f float(M_SQRT2)
#define M_SQRT3 (1.73205080756887729352744634151)
#define M_SQRT3f (float(M_SQRT3))


template <typename T> inline T squared(const T& val) { return val * val; }
template <typename T> inline T cubed(const T& val) { return val * val * val; }

template <typename T> inline
T sign(T val)
{
    return (T(0) < val) - (val < T(0));
}

template <typename T>
inline int sign_int(T val, T threshold=epsilon)
{
    return (val > threshold) ? 1 : (val < -threshold) ? -1 : 0;
}

VECTORIZE(T, sign, V1);
VECTORIZE(int, sign_int, V1);

template <typename T> T mean(const T& a, const T& b) { return 0.5 * (a + b); }
template <typename T> T mean(const T& a, const T& b, const T& c) { return (1.0/3.0) * (a + b + c); }


inline int distance(int a, int b)
{
    return abs(a - b);
}

static const double kGoldenRatio = 1.61803398875;
static const float kGoldenRatiof = 1.61803398875f;

inline float2 toGoldenRatioY(float y) { return float2(y * kGoldenRatiof, y); }
inline float2 toGoldenRatioX(float x) { return float2(x, x / kGoldenRatiof); }

inline float cross(float2 a, float2 b) { return a.x * b.y - a.y * b.x; }
// return f3(a.y*b.z - a.z*b.y, a.z*b.x - a.x*b.z, a.x*b.y - a.y*b.x);

inline int clamp(int v, int mn, int mx) { return min(max(v, mn), mx); }
inline float clamp(float v, float mn=0.f, float mx=1.f) { return min(max(v, mn), mx); }
inline double clamp(double v, double mn=0.0, double mx=1.0) { return min(max(v, mn), mx); }

inline float length(float x, float y) { return sqrt(x*x + y*y); }
inline double length(double x, double y) { return sqrt(x*x + y*y); }

template <typename V>
inline auto lengthSqr(const V &a) { return dot(a, a); }

template <typename V>
inline auto distanceSqr(const V &a, const V &b) { return lengthSqr(a-b); }


template <typename V>
inline V clamp_length(const V &v, const typename V::value_type &mx)
{
    const typename V::value_type len2 = lengthSqr(v);
    return (len2 > mx * mx) ? v * (mx / sqrt(len2)) : v;
}

template <typename V>
inline V clamp_length(const V &v, const typename V::value_type &mn, const typename V::value_type &mx)
{
    const typename V::value_type len2 = lengthSqr(v);
    return (len2 < mn * mn) ? v * (mn / sqrt(len2)) :
        (len2 > mx * mx) ? v * (mx / sqrt(len2)) : v;
}

inline float2 clamp_length(float2 v) { return clamp_length(v, 0.f, 1.f); }

inline float clamp_mag(float v, float mn, float mx)
{
    const float vm = abs(v);
    return ((vm < mn) ? ((v > 0.f) ? mn : -mn) :
            (vm > mx) ? ((v > 0.f) ? mx : -mx) : v);
}

inline float2 clamp_aspect(float2 size, float minWH, float maxWH)
{
    ASSERT(maxWH > minWH);
    return min(size, float2(size.y * maxWH, size.x / minWH));
}

// return vector V clamped to fit inside origin centered rectangle with radius RAD
// direction of V does not change
float2 clamp_rect(float2 v, float2 rad);

// return center of circle as close to POS as possible and with radius RAD, that fits inside of AABBox defined by RCENTER and RRAD
float2 circle_in_rect(float2 pos, float rad, float2 rcenter, float2 rrad);

inline float max_dim(float2 v) { return max(v.x, v.y); }
inline float min_dim(float2 v) { return min(v.x, v.y); }
inline float max_dim(const float3 &v) { return max(v.x, max(v.y, v.z)); }
inline float min_dim(const float3 &v) { return min(v.x, max(v.y, v.z)); }

inline bool nearZero(float v)  { return fabsf(v) <= M_SQRT2f * epsilonf; }
inline bool nearZero(double v) { return fabs(v) <= M_SQRT2 * epsilon; }

VECTORIZE(and, nearZero, V1);

template <typename T>
inline bool nearEqual(const T& a, const T& b) { return nearZero(a-b); }


// modulo (%), but result is [0-y) even for negative numbers
template <typename T>
inline T modulo_i(T x, T y)
{
    ASSERT(y > 0);

    if (x >= 0)
        return x % y;

    const T m = x - y * (x / y);
    return ((m < 0) ? y+m :
            (m == y) ? 0 : m);
}

inline int modulo(int x, size_t y) { return modulo_i<int>(x, y); }
inline size_t modulo(size_t x, size_t y) { return modulo_i<int64>(x, y); }
inline int modulo(uint x, uint y) { return modulo_i<int>(x, y); }

// adapted from http://stackoverflow.com/questions/4633177/c-how-to-wrap-a-float-to-the-interval-pi-pi
// floating point modulo function. Output is always [0-y)
template <typename T>
inline T modulo_f(T x, T y)
{
    const double m = x - y * floor(x / y);

    return ((y > 0) ?
            ((m >= y) ? 0 :
             (m < 0) ? ((y+m == y) ? 0 : y+m) :
             m) :
            ((m <= y) ? 0 : 
             (m > 0) ? ((y+m == y) ? 0 : y+m) :
             m));
}
inline float modulo(float x, float y) { return modulo_f<float>(x, y); }
inline double modulo(double x, double y) { return modulo_f<double>(x, y); }

VECTORIZE(T, modulo, V2);
VECTORIZE(T, modulo, V1S1);

inline float min_abs(float a, float b) { return (fabsf(a) <= fabsf(b)) ? a : b; }
inline float max_abs(float a, float b) { return (fabsf(a) >= fabsf(b)) ? a : b; }

VECTORIZE(T, min_abs, V2);
VECTORIZE(T, max_abs, V2);

// glm defines these types but using like 12 nested functions and its really slow in debug mode
VECTORIZE(T, max, V2);
VECTORIZE(T, min, V2);

inline float2 rotate90(float2 v)  { return float2(-v.y, v.x); }
inline float2 rotateN90(float2 v) { return float2(v.y, -v.x); }

inline float2 rotate90(float2 v, int i)
{
    i = modulo(i, 4);
    while (i--)
        v = f2(-v.y, v.x);
    return v;
}

inline float2 rot_ccw(float2 v) { return float2(-v.y, v.x); }
inline float2 rot_cw(float2 v) { return float2(v.y, -v.x); }


// return shortest signed difference between angles [0, pi]
inline float distanceAngles(float a, float b)
{
    // float e = dotAngles(a, b + M_PI_2f);
    // if (dotAngles(a, b) < 0.f)
        // e = std::copysign(2.f, e) - e;
    // return M_PI_2f * e;

    return modulo(b - a + 1.5f * M_TAUf, M_TAUf) - M_PIf;
}

// return shortest signed difference between orientation vectors [0, pi]
inline float distanceRots(float2 a, float2 b)
{
    // float e = dot(a, rotate90(b));
    // if (dot(a, b) < 0.f)
        // e = std::copysign(2.f, e) - e;
    // return M_PI_2f * e;
    return distanceAngles(v2a(a), v2a(b));
}


// prevent nans
inline float2 normalize(const float2 &a)
{
    if (nearZero(a)) {
        ASSERT_FAILED("l < epsilon", "{%f, %f}", a.x, a.y);
        return a;
    } else {
        return glm::normalize(a);
    }
}

inline double2 normalize(const double2 &a) { return glm::normalize(a); }
inline float2 normalize_orzero(const float2 &a) { return nearZero(a) ? float2() : glm::normalize(a); }
inline float3 normalize_orzero(const float3 &a) { return nearZero(a) ? float3() : glm::normalize(a); }

inline float2 pow(float2 v, float e) { return float2(std::pow(v.x, e), std::pow(v.y, e)); }
inline float3 pow(const float3 &v, float e) { return float3(std::pow(v.x, e), std::pow(v.y, e), std::pow(v.z, e)); }

VECTORIZE(T, log1p, V1);

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

static const double kToRadians = M_PI / 180.0;
static const double kToDegrees = 180.0 / M_PI;

inline float todegrees(float radians) { return 360.f / (2.f * M_PIf) * radians; }
inline float toradians(float degrees) { return 2.f * M_PIf / 360.0f * degrees; }


// rotate vector v by angle a
template <typename T>
inline glm::tvec2<T> rotate(glm::tvec2<T> v, T a)
{
    T cosa = std::cos(a);
    T sina = std::sin(a);
    return glm::tvec2<T>(cosa * v.x - sina * v.y, sina * v.x + cosa * v.y);
}

// rotate counterclockwise
template <typename T>
inline glm::tvec2<T> rotate(const glm::tvec2<T> &v, const glm::tvec2<T> &a)
{
    return glm::tvec2<T>(a.x * v.x - a.y * v.y, a.y * v.x + a.x * v.y);
}

// rotate clockwise
template <typename T>
inline glm::tvec2<T> rotateN(const glm::tvec2<T> &v, const glm::tvec2<T> &a)
{
    return glm::tvec2<T>(a.x * v.x + a.y * v.y, -a.y * v.x + a.x * v.y);
}

inline float2 swapXY(float2 v)    { return float2(v.y, v.x); }
inline float2 justY(float2 v)     { return float2(0.f, v.y); }
inline float2 justY(float v=1.f)  { return float2(0.f, v); }
inline float2 justX(float2 v)     { return float2(v.x, 0.f); }
inline float2 justX(float v=1.f)  { return float2(v, 0.f); }
inline float3 justZ(float v=1.f)  { return float3(0.f, 0.f, v); }

inline float2 justQ(int q, float v=1.f)
{
    float2 f;
    f[q] = v;
    return f;
}

template <typename T> T flipX(T v) { v.x = -v.x; return v; }
template <typename T> T flipY(T v) { v.y = -v.y; return v; }
template <typename T> T flipZ(T v) { v.z = -v.z; return v; }

template <typename T> T invert_x(T v) { v.x = -v.x; return v; }
template <typename T> T invert_y(T v) { v.y = -v.y; return v; }
template <typename T> T invert_z(T v) { v.z = -v.z; return v; }


#define JUST2(V, T)                                      \
    inline V V##x(T v) { return V(v, 0); }               \
    inline V V##x(const V &v) { return V(v.x, 0); }      \
    inline V V##y(T v) { return V(0, v); }               \
    inline V V##y(const V &v) { return V(0, v.y); }

#define JUST3(V, T)                                              \
    inline V V##x(T v) { return V(v, 0, 0); }                    \
    inline V V##x(const V &v) { return V(v.x, 0, 0); }           \
    inline V V##y(T v) { return V(0, v, 0); }                    \
    inline V V##y(const V &v) { return V(0, v.y, 0); }           \
    inline V V##z(T v) { return V(0, v, 0); }                    \
    inline V V##z(const V &v) { return V(0, v.y, 0); }

JUST2(f2, float)
JUST2(d2, double)
JUST2(i2, int)

JUST3(f3, float)
JUST3(d3, double)
JUST3(i3, int)


template <typename T> inline T unorm2snorm(T v) { return T(2) * (v - T(0.5)); }
template <typename T> inline T snorm2unorm(T v) { return T(0.5) * (v + T(1.0)); }

VECTORIZE(T, unorm2snorm, V1);
VECTORIZE(T, snorm2unorm, V1);

inline float unorm_sin(float v) { return snorm2unorm(sin(v)); }

template <typename T>
inline T multiplyComponent(T v, int i, float x)
{
    v[i] *= x;
    return v;
}

inline float logerp(float v, float tv, float s)
{ 
    if (fabsf(v) < 0.00000001)
        return 0;
    else
        return v * pow(tv/v, s);
}

VECTORIZE(T, logerp, V2S1)

inline float logerp1(float from, float to, float v) { return pow(from, 1-v) * pow(to, v); }
VECTORIZE(T, logerp1, V2S1);

template <typename T>
inline bool isBetween(const T &val, const T &lo, const T &hi) 
{
    return lo <= val && val <= hi;
}

template <typename T>
inline T lerp(const T &from, const T &to, typename value_type_traits<T>::type v)
{
    typedef typename value_type_traits<T>::type U;
    return from * (U(1) - v) + to * v;
}

template <typename T>
inline T clamp_lerp(const T &from, const T &to, typename value_type_traits<T>::type v)
{
    return lerp(from, to, clamp(v));
}

template <typename T, typename U = typename T::value_type>
inline U lerp(const T &array, U v)
{
    const U f = floor(v);
    const U n = ceil(v);
    return (f == n) ? array[f] : lerp(array[f], array[n], v-f);
}


inline float lerpAngles(float a, float b, float v)
{
    return vectorToAngle(lerp(angleToVector(a), angleToVector(b), v));
}

template <typename T>
inline glm::tvec3<T> slerp(const glm::tvec3<T> &p0, const glm::tvec3<T> &p1, const T& t)
{
    T omega = atan2(length(cross(p0, p1)), dot(p0, p1));
    // T omega = acos(dot(p0, p1));
    if (nearZero(abs(omega) - T(1)))
        return p0;              // pointing in the same direction
    
    T so = sin(omega);

    T t1 = sin((T(1) - t) * omega) / so;
    T t2 = sin(t * omega) / so;
    
    return t1 * p0 + t2 * p1;
}

// lambda is 0-infinity (bigger tweens faster)
// use this for frame-rate independant lerping
// (exponential tweening)
template <typename T>
inline T tween_towards(const T& a, const T& b, typename value_type_traits<T>::type lambda)
{
    typedef typename value_type_traits<T>::type U;
    return lerp(a, b, U(1) - exp(-lambda));
}

template <typename T>
inline T scale_dt(T lambda) { return T(1) - exp(-lambda); }


inline float normalize(float v) { return sign(v); }
inline double normalize(double v) { return sign(v); }

// move A MAXV towards B
// (linear tweening)
template <typename T>
inline T move_towards(const T& a, const T& b, typename value_type_traits<T>::type lambda)
{
    typedef typename value_type_traits<T>::type U;
    T delta = b - a;
    U len = length(delta);
    return (abs(len) <= lambda) ? b : a + delta/len * lambda;
}


// template <typename T>
// inline T slerp(T p0, T p1, double t)
// {
//     const double omega = acos(dot(p0, p1));
//     const double so = sin(omega);
//     return sin((1.0-t)*omega) / so * p0 + sin(t*omega)/so * p1;
// }

// arc distance between two points on a sphere
inline double sdistance(double3 p0, double3 p1)
{
    const double omega = acos(dot(p0, p1));
    return M_TAU * omega;
}


// return 0-1 depending how close value is to inputs zero and one
// inv_lerp(a, b, lerp(a, b, v)) == v
template <typename T>
inline T inv_lerp(T zero, T one, T val)
{
    const T denom = one - zero;
    if (nearZero(denom))
        return T(0);
    return (val - zero) / denom;
}

template <typename T>
inline glm::tvec2<T> inv_lerp(glm::tvec2<T> zero, glm::tvec2<T> one, glm::tvec2<T> val)
{
    const glm::tvec2<T> denom = one - zero;
    return (nearZero(denom.x) ? glm::tvec2<T>(inv_lerp(zero.y, one.y, val.y)) :
            nearZero(denom.y) ? glm::tvec2<T>(inv_lerp(zero.x, one.x, val.x)) :
            (val - zero) / denom);
}

template <typename T>
inline glm::tvec3<T> inv_lerp(glm::tvec3<T> zero, glm::tvec3<T> one, glm::tvec3<T> val)
{
    const glm::tvec3<T> denom = one - zero;
    if (nearZero(denom.x)) {
        glm::tvec2<T> yz = inv_lerp(glm::tvec2<T>(zero.y, zero.z), glm::tvec2<T>(one.y, one.z), glm::tvec2<T>(val.y, val.z));
        return glm::tvec3<T>((yz.x + yz.y) / 2.f, yz.x, yz.y);
    } else if (nearZero(denom.y)) {
        glm::tvec2<T> xz = inv_lerp(glm::tvec2<T>(zero.x, zero.z), glm::tvec2<T>(one.x, one.z), glm::tvec2<T>(val.x, val.z));
        return glm::tvec3<T>(xz.x, (xz.x + xz.y) / 2.f, xz.y);
    } else if (nearZero(denom.z)) {
        glm::tvec2<T> xy = inv_lerp(glm::tvec2<T>(zero.x, zero.y), glm::tvec2<T>(one.x, one.y), glm::tvec2<T>(val.x, val.y));
        return glm::tvec3<T>(xy.x, xy.y, (xy.x + xy.y) / 2.f);
    } else {
        return (val - zero) / denom;
    }
}

// return 0-1 depending how close value is to inputs zero and one (clamps if outside)
// inv_lerp(a, b, lerp(a, b, v)) == v, for 0 < v < 1
template <typename T>
inline float inv_lerp_clamp(T zero, T one, T val)
{
    const bool inv = (zero > one);
    if (inv)
        swap(zero, one);
    const float unorm = (val >= one) ? 1.0 :
                        (val <= zero) ? 0.0 : (val - zero) / (one - zero);
    return inv ? 1.0 - unorm : unorm;
}

// reduced VAL to 0.0 within FADELEN of either START or END (or beyond), and 1.0 if in the middle
inline float smooth_clamp(float start, float end, float val, float fadelen)
{
    ASSERT(end - start > fadelen);
    val = clamp(start, end, val);
    return min(1.f, min((val - start) / fadelen,
                        (end - val) / fadelen));
}

// reduced VAL to 0.0 within FADESTART of START or FADEEND of END (or beyond), and 1.0 if in the middle
inline float smooth_clamp(float start, float end, float val, float fadestart, float fadeend)
{
    ASSERT(end - start > fadeend);
    val = clamp(start, end, val);
    float ret = 1.f;
    if (val < start + fadestart && fadestart > epsilon)
        ret = min(ret, (val - start) / fadestart);
    if (val > end - fadeend && fadeend > epsilon)
        ret = min(ret, (end - val) / fadeend);
    return clamp(ret);
}

// cardinal spline, interpolating with 't' between y1 and y2 with control points y0 and y3 and tension 'c'
// c = 0 is a Catmull-Rom spline
// c = 1 is smoother
template <typename T, typename R>
inline T cardinal(const T& y0, const T& y1, const T& y2, const T& y3, R t, R c=R(0.0))
{
    const R t2 = t * t;
    const R t3 = t2 * t;
    const R h1 = 2.0 * t3 - 3.0 * t2 + 1.0;
    const R h2 = -2.0 * t3 + 3.0 * t2;
    const R h3 = t3 - 2.0 * t2 + t;
    const R h4 = t3 - t2;
    const T m1 = c * (y2 - y0);
    const T m2 = c * (y3 - y1);
    const T r  = m1 * h3 + y1 * h1 + y2 * h2 + m2 * h4;
    return r;
}

template <typename T, typename R>
inline T cardinal(const T *vec, R t, R c=R(0))
{
    return cardinal(vec[0], vec[1], vec[2], vec[3], t, c);
}

template <typename T, uint N, typename R>
inline T cardinal_array(T (&array)[N], R t, R c=R(0))
{
    t *= R(N - 1);
    const uint f = floor_int(t);
    if (f + 1 >= N /*t < epsilon */) {
        return array[f];
    }
    // ASSERT(N > 1);
    ASSERT(f + 1 < N);
    return cardinal(array[(f > 0) ? f-1 : 0],
                    array[f], 
                    array[f+1],
                    array[(f + 2 < N) ? f+2 : f+1],
                    t-R(f), c);
}

// normalized sigmoid (s-shape) function
// larger k is closer to a straight line (y = x)
// http://dinodini.wordpress.com/2010/04/05/normalized-tunable-sigmoid-functions/
template <typename T>
inline T signorm(T x, T k=0.2f)
{
    T y = 0;
    if (x > T(0.5)) {
        x -= T(0.5);
        k = T(-1) - k;
        y = T(0.5);
    }
    return y + (T(2) * x * k) / (T(2) * (T(1) + k - T(2) * x) );
}

// http://en.wikipedia.org/wiki/Smoothstep
inline float smootherstep(float edge0, float edge1, float x)
{
    // Scale, and clamp x to 0..1 range
    x = clamp((x - edge0)/(edge1 - edge0), 0.f, 1.f);
    // Evaluate polynomial
    return x*x*x*(x*(x*6 - 15) + 10);
}

// map unorm to a smooth bell curve shape (0->0, 0.5->1, 1->0)
inline float bellcurve(float x)
{
    return 0.5f * (-cos(M_TAUf * x) + 1);
}

// perlin/simplex noise, range is [-1 to 1]
float snoise(const float2 &v);
float snoise(const float3 &v);


template <typename T>
float fractal_noise(T v, int octaves=6)
{
    float x = 0.f;
    float a = 0.5f;
    for (int i=0; i<octaves; i++)
    {
        x += a * snoise(v);
        v *= (typename T::value_type) 2.0;
        a *= 0.5f;
    }
    return x;
}

// 
inline float gaussian(float x, float stdev=1.f)
{
    const double sqrt_2pi = 2.5066282746310002;
    return (float)exp(-(x * x) / (2.0 * stdev * stdev)) / (stdev * sqrt_2pi);
}


// x is -2.5 to 1
// y is   -1 to 1
inline int mandelbrot(double x0, double y0, int max_iteration)
{
    int i=0;
    for (double x=0, y=0; x*x + y*y < 4.0 && i < max_iteration; i++)
    {
        double xtemp = x*x - y*y + x0;
        y = 2.0*x*y + y0;
        x = xtemp;
    }
    return i;
}

inline float lnorm(float val, float low, float high)
{
    if (low >= high)
        return 1.f;
    val = clamp(val, low, high);
    return (val - low) / (high - low);
}

inline float parabola(float x, float roota, float rootb) { return (x-roota) * (x-rootb); }

///////////////////////////////////////////// intersection

template <typename T>
inline bool isInRange(T p, T mn, T mx) { return mn <= p && p < mx; }

VECTORIZE(and, isInRange, V3);

inline float spreadCircleToCircle(float2 c0, float r0, float2 c1, float r1)
{
    const float2 c1p = c1 + r1 * normalize(rotate90(c1 - c0));
    return fabsf(vectorToAngle(c1 - c0) - vectorToAngle(c1p - c0));
}

inline bool intersectPointCircle(const float2 &p, const float2 &c, float r)
{
    const float2 x = p-c;
    return x.x * x.x + x.y * x.y <= (r*r);
}

inline bool intersectPointCircle(const float2 &x, float r)
{
    return x.x * x.x + x.y * x.y <= (r*r);
}

inline bool intersectPointSphere(const float3 &p, const float3 &c, float r)
{
    const float3 x = p-c;
    return dot(x, x) <= (r*r);
}

inline bool intersectSphereSphere(const float3 &p, float pr, const float3 &c, float cr)
{
    const float3 x = p-c;
    const float r = pr + cr;
    return dot(x, x) <= (r*r);
}

inline bool intersectPointRing(const float2 &p, const float2 &c, float minr, float maxr)
{
    const float2 x = p-c;
    const float v = x.x * x.x + x.y * x.y;
    return (minr*minr) <= v && v <= (maxr*maxr);
}

inline bool intersectCircleCircle(const float2 &p, float pr, const float2 &c, float cr, float *dist=NULL)
{
    if (!intersectPointCircle(p, c, pr+cr))
        return false;
    if (dist)
        *dist = distance(p, c) - (pr + cr);
    return true;
}

// intersect two circles, returning number of intersections with points in RA and RB
int intersectCircleCircle(float2 *ra, float2 *rb, const float2 &p, float pr, const float2 &c, float cr);

// on intersection, return t where intersection point is lerp(a1, a2, t)
// else return 0
float intersectSegmentSegment(float2 a1, float2 a2, float2 b1, float2 b2);
float intersectLineLine(float2 a1, float2 a2, float2 b1, float2 b2);

bool intersectSegmentSegment(float2 *o, float2 a1, float2 a2, float2 b1, float2 b2);

// return count, up to two points in OUTP
int intersectPolySegment(float2 *outp, const float2 *points, int npoints, float2 sa, float2 sb);

// assume clockwise winding
bool intersectPolyPoint(const float2 *points, int npoints, float2 ca);

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


// positive if pd is below the plane pa, pb, pc (counterclockwise)
inline float orient(const f3& pa, const f3 &pb, const f3 &pc, const f3 &pd)
{
    float adx = pa.x - pd.x;
    float bdx = pb.x - pd.x;
    float cdx = pc.x - pd.x;
    float ady = pa.y - pd.y;
    float bdy = pb.y - pd.y;
    float cdy = pc.y - pd.y;
    float adz = pa.z - pd.z;
    float bdz = pb.z - pd.z;
    float cdz = pc.z - pd.z;

    return adx * (bdy * cdz - bdz * cdy)
        + bdx * (cdy * adz - cdz * ady)
        + cdx * (ady * bdz - adz * bdy);
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
	
	return float(-area/2.0);
}

// moment of intertia of polygon
float momentForPoly(float mass, int numVerts, const float2 *verts, float2 offset);


inline double regpoly_apothem(int n, double R=1.0) { return R * cos(M_PI / n); }
inline double regpoly_circumradius(int n, double r=1.0) { return r / cos(M_PI / n); }
inline double regpoly_radius_from_side(int n, double s) { return s / (2.0 * sin(M_PI / n)); }
// inline double regpoly_area(int n, double R) { return 0.5 * n * R * R * sin(M_TAU / n); }
inline double regpoly_area(int n, double R=1.0, double R1=0.0) { return 0.5 * n * R * (R1 ? R1 : R) * sin(M_TAU / n); }
inline double regpoly_perimeter(int n, double R=1.0) { return n * 2.0 * R * sin(M_PI / n); }


// return the point on line segment a, b closest to p
float2 closestPointOnSegment(float2 a, float2 b, float2 p);

inline float distancePointSegment(float2 p, float2 a, float2 b)
{
    // FIXME optimize
    return distance(p, closestPointOnSegment(a, b, p));
}

// ray starting at point a and extending in direction dir
float2 closestPointOnRay(const float2 &a, float2 usegv, float2 p);

// return true if line segment a, b and circle c, r intersect
 bool intersectSegmentCircle(float2 a, float2 b, float2 c, float r, float *dist=NULL);

// if line segment a, b and circle c, r intersect, return # of intersections
int intersectSegmentCircle(float2 *ra, float2 *rb, const float2 &a, const float2 &b, const float2 &c, float r);
int intersectSegmentSphere(float3 *ra, float3 *rb, const float3 &a, const float3 &b, const float3 &c, float r);
int intersectSegmentSphere(double3 *ra, double3 *rb, const double3 &a, const double3 &b, const double3 &c, double r);

int intersectSegmentPlane(const d3& a, const d3& b, const d3 &p, const d3& n, d3 *I=NULL);
float intersectSegmentPlane(const f3 &p, const f3 &q, const f4 &n);

// return *signed* distance between a point and a plane
inline float distancePointPlane(float3 ap, float3 bp, float3 bn)
{
    return dot(bn, ap - bp);
}

inline bool intersectStadiumCircle(float2 a, float2 b, float r, float2 c, float cr)
{
    float2 closest = closestPointOnSegment(a, b, c);
    return intersectCircleCircle(closest, r, c, cr);
}


float intersectRayCircle(float2 E, float2 d, float2 C, float r, float2 *N=NULL);
float2 intersectRayCircle2(float2 E, float2 d, float2 C, float r, float2 *N=NULL);

// modified from http://stackoverflow.com/questions/1073336/circle-line-collision-detection
// ray is at point E in direction d
// circle is at point C with radius r
bool intersectRayCircle(float2 *o, float2 E, float2 d, float2 C, float r);

float intersectRaySegment(float2 ap, float2 ad, float2 b1, float2 b2);
float intersectSegmentRay(float2 b1, float2 b2, float2 ap, float2 ad);

inline float intersectRayTriangle(float2 orig, float2 dir, float2 sa, float2 sb, float2 sc)
{
    return min(intersectRaySegment(orig, dir, sa, sb),
               min(intersectRaySegment(orig, dir, sb, sc),
                   intersectRaySegment(orig, dir, sc, sa)));
}

// a and b are the center of each rectangle, and ar and br are the distance from the center to each edge
inline bool intersectRectangleRectangle(const float2 &a, const float2 &ar, const float2 &b, const float2 &br)
{
    const float2 delt = abs(a - b);
    return (delt.x <= (ar.x + br.x) &&
            delt.y <= (ar.y + br.y));
}

bool _intersectCircleRectangle1(const float2 &circleDistance, float circleR, const float2& rectR);

inline bool intersectCircleRectangle(const float2 &circle, float circleR, const float2 &rectP, const float2 &rectR)
{
    const float2 circleDistance = abs(circle - rectP);
    return _intersectCircleRectangle1(circleDistance, circleR, rectR);
}

inline bool intersectPointRectangle(float2 p, float2 b, float2 br)
{
    const float2 v = b - p;
    return v.x > -br.x && v.y > -br.y && v.x <= br.x && v.y <= br.y;
}

inline bool intersectPointRectangleCorners(float2 p, float2 a, float2 b)
{
    float2 mn = min(a, b);
    float2 mx = max(a, b);
    return p.x >= mn.x && p.y >= mn.y && p.x <= mx.x && p.y <= mx.y;
}

inline bool containedCircleInRectangle(const float2 &circle, float circleR, const float2 &rectP, const float2 &rectR)
{
    return intersectPointRectangle(circle, rectP, rectR - float2(circleR));
}

struct Rect2d {
    float2 pos;
    float2 rad;
};

template <typename T>
inline bool is_empty(const glm::tvec2<T>& v) { return !v.x && !v.y; }
template <typename T>
inline bool is_empty(const glm::tvec3<T>& v) { return !v.x && !v.y && !v.z; }
template <typename T>
inline bool is_empty(const glm::tvec4<T>& v) { return !v.x && !v.y && !v.z && !v.w; }

template <typename V>
struct AABB_ {
    typedef typename V::value_type value_type;
    typedef V                      vector_type;

    enum { components = vector_type::components };
    
    V mn, mx;

    AABB_() {}
    AABB_(const V &n) : mn(n), mx(n) {}
    AABB_(const V &n, const V &x) : mn(min(n, x)), mx(max(n, x)) {}
    AABB_(const V &n, value_type x) : mn(n - x), mx(n + x) {}

    template <typename U>
    explicit AABB_(const AABB_<U> &bb) : mn(bb.mn), mx(bb.mx) {}

    static AABB_ from_center_rad(const V& c, const V& r) { return AABB_(c-r, c+r); }

    // explicit operator AABB2() const { return AABB2(f2(mn), f2(mx)); }

    bool operator==(const AABB_ &o) const { return mn == o.mn && mx == o.mx; }

    // index like it has 6 (or 4) floats
    // value_type &operator[](int i) { return (&mn)[i]; }
    // const value_type &operator[](int i) const { return (&mn)[i]; }
    
    V getRadius() const { return value_type(0.5) * (mx-mn); }
    V getCenter() const { return value_type(0.5) * (mx+mn); }
    value_type getBRadius() const { return length(getRadius()); }
    bool   empty() const { return is_empty(mn) && is_empty(mx); }

    V radius() const { return value_type(0.5) * (mx-mn); }
    V center() const { return value_type(0.5) * (mx+mn); }
    V size() const { return (mx - mn); } // width x height
    value_type width() const { return mx.x-mn.x; }
    value_type height() const { return mx.y-mn.y; }

    value_type center(int i) const { return value_type(0.5) * (mx[i]+mn[i]); }

    value_type area() const
    {
        value_type v = 1;
        for (int i=0; i<components; i++)
            v *= mx[i] - mn[i];
        return v;
    }
    
    void setRadius(const V &rad)
    {
        V c = center();
        mn = c - rad;
        mx = c + rad;
    }
    
    void translate(const V &v)
    {
        mn += v;
        mx += v;
    }

    AABB_ translated(const V &v) const
    {
        AABB_ x = *this;
        x.translate(v);
        return x;
    }

    AABB_ operator+(const AABB_ &box) const { return AABB_(min(mn, box.mn), max(mx, box.mx)); }
    AABB_ &operator+=(const AABB_ &box) { extend(box); return *this; }

    void start(const V &pt)
    {
        if (!is_empty(mn) || !is_empty(mx))
            return;
        mn = pt;
        mx = pt;
    }

    void reset()
    {
        mn = V(0);
        mx = V(0);
    }
    
    void extend(const V &pt)
    {
        start(pt);
        mx = max(mx, pt);
        mn = min(mn, pt);
    }

    void extend(const V &pt, const V &rad)
    {
        start(pt);
        mx = max(mx, pt + rad);
        mn = min(mn, pt - rad);
    }

    void extend(const V &pt, value_type rad)
    {
        start(pt);
        mx = max(mx, pt + V(rad));
        mn = min(mn, pt - V(rad));
    }

    void extend(const AABB_ &bb)
    {
        start(bb.mn);
        mx = max(mx, bb.mx);
        mn = min(mn, bb.mn);
    }

    void expand(value_type v) { setRadius(radius() + v); }
    AABB_ expanded(value_type v) const { AABB_ x = *this; x.expand(v); return x; }
    void scale(value_type v) { setRadius(radius() * v); }
    AABB_ scaled(value_type v) const { AABB_ x = *this; x.scale(v); return x; }

    friend bool fpu_error(const AABB_ &bb) { return fpu_error(bb.mn) || fpu_error(bb.mx); }
};

typedef AABB_<f3> AABB3;
typedef AABB_<f3> AABB;
typedef AABB_<f2> AABB2;

extern template struct AABB_<f3>;
extern template struct AABB_<f2>;

template <typename V, typename T = typename V::value_type>
struct BSphere_ {
    V pos;
    T rad = T();

    BSphere_(V p=V(), T r=T()) : pos(p), rad(r) {}
    BSphere_(AABB_<V> bb) : pos(bb.center()), rad(length(bb.radius())) {}

    BSphere_ operator*(float v) const { return BSphere_(pos, rad * v); }
    BSphere_ operator+(float v) const { return BSphere_(pos, rad + v); }
    friend BSphere_ operator*(float v, const BSphere_ &b) { return BSphere_(b.pos, v * b.rad); }
    friend BSphere_ operator+(float v, const BSphere_ &b) { return BSphere_(b.pos, v + b.rad); }
    
    explicit operator AABB_<V>() { return AABB_<V>(pos - V(rad), pos + V(rad)); }
};

template <typename T>
inline bool intersectSphereSphere(const BSphere_<T> &a, const BSphere_<T> &b)
{
    return intersectSphereSphere(a.pos, a.rad, b.pos, b.rad);
}

typedef BSphere_<f3> BSphere;
typedef BSphere_<f2> BCircle;

extern template struct BSphere_<f3>;
extern template struct BSphere_<f2>;

bool intersectAABB(const AABB3 &a, const AABB3 &b, float *dist=NULL);

// true if a completely contains b (a.extend(b) == a)
inline bool containsAABB(const AABB3 &a, const AABB3 &b)
{
    return a.mn.x <= b.mn.x && a.mn.y <= b.mn.y && a.mn.z <= b.mn.z &&
        a.mx.x >= b.mx.x && a.mx.y >= b.mx.y && a.mx.z >= b.mx.z;
}

// bool intersectSphereAABB(const float3 &p, float r, const AABB3 &b, float *dist=NULL);

bool intersectAABB(const AABB2 &a, const AABB2 &b, float *dist=NULL);

inline bool containsAABB(const AABB2 &a, const AABB2 &b)
{
    return a.mn.x <= b.mn.x && a.mn.y <= b.mn.y && 
        a.mx.x >= b.mx.x && a.mx.y >= b.mx.y;
}

template <typename V>
inline bool intersectPointAABB(const V &p, const AABB_<V> &b, typename V::value_type *dist=NULL)
{
    for (int i=0; i<V::components; i++)
        if (!(b.mn[i] <= p[i] && p[i] <= b.mx[i]))
            return false;
	if (dist)
    {
        typedef typename V::value_type T;
        V d = abs(p - b.center()) - b.radius();
        *dist = min(max_dim(d), T(0)) + length(max(d, V(0)));
    }
    return true;
}

bool intersectCircleAABB(const float2 &p, float r, const AABB2 &b, float *dist=NULL);
bool intersectSphereAABB(const float3 &p, float r, const AABB3 &b, float *dist=NULL);


// return AABB that intersects both A and B
template <typename B>
B intersectionAABB(const B& a, const B& b) { return B(max(a.mn, b.mn), min(a.mx, b.mx)); }

template <typename V>
struct Ray {
    V pos, dir;
    Ray() {}
    Ray(const V &p, const V &d) : pos(p), dir(d) {}
    
    template <typename U>
    Ray(const Ray<U> &r) : pos(r.pos), dir(r.dir) {}
};

template <typename V>
struct Segment { V a, b; };

float intersectRayTriangle(const float3& orig, const float3& dir,
                           const float3& v0, const float3& v1, const float3& v2);
bool intersectRayAABB(const float3& orig, const float3 &dir, const AABB3 &b, float* dist=NULL);
bool intersectRayAABB(const float2& orig, const float2 &dir, const AABB2 &b, float* dist=NULL);
bool intersectRaySphere(const f3 &orig, const f3 &dir, const f3 &center, float radius, float *dist=NULL);

template <typename V>
inline bool intersectRaySphere(const Ray<V> &ray, const V &center, typename V::value_type rad, float *dist=NULL)
{
    return intersectRaySphere(ray.pos, ray.dir, center, rad, dist);
}

template <typename V>
inline bool intersectRaySphere(const Ray<V> &ray, const BSphere_<V> &bs, float *dist=NULL)
{
    return intersectRaySphere(ray.pos, ray.dir, bs.pos, bs.rad, dist);
}

template <typename V>
inline bool intersectRaySphere(const V &pos, const V &dir, const BSphere_<V> &bs, float *dist=NULL)
{
    return intersectRaySphere(pos, dir, bs.pos, bs.rad, dist);
}

float2 intersectRaySphere2(const f3 &orig, const f3 &dir, const f3 &center, float radius);


inline float4 plane(const f3 &P, const f3 &N)
{
    return f4(N, -dot(P, N));
}

template <typename T>
inline glm::tvec4<T> normalize_plane(const glm::tvec4<T> &v)
{
    return v / length(v.xyz());
}

template <typename T>
inline glm::tvec4<T> normalize_plane(const glm::tvec3<T> &P, const glm::tvec3<T> &N)
{
    glm::tvec4<T> V(N, -dot(P, N));
    return V / length(N.xyz());
}

bool intersectRayPlane(const f3& orig, const f3 &dir, const f4 &N, float *dist);

// plane is ax + by + cz + d = 0
struct Frustum { float4 planes[6]; };

bool intersectSpherePlane(const float3 &c, float r, const float4 &p);

// 1 if triangle is totally inside plane
// 0 if triangle intersects plane
// -1 if triangle is totally outside plane
int intersectTrianglePlane(const f3* tri, const float4 &plane);

// a point is "inside" the plane if it is in the direction of the plane normal from the plane
bool intersectPointPlane(const f3 &bb, const float4 &plane);
bool intersectAABBPlane(const AABB &bb, const float4 &plane);
bool intersectAABBFrustum(const AABB &box, const Frustum &fru);
bool intersectSphereFrustum(const float3 &p, float r, const Frustum &fru);

inline bool intersectPointFrustum(const float3 &p, const Frustum &fru)
{
    return intersectSphereFrustum(p, 0.f, fru);
}

bool intersectSegmentFrustum(const float3 &a, const float3 &b, const Frustum &f);

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


inline bool intersectSegmentAABB(float2 a, float2 b, const AABB2 &bb, float *dist=NULL)
{
    float v = intersectBBSegmentV(bb.mn.x, bb.mn.y, bb.mx.x, bb.mx.y, a, b);
    if (!(0.f <= v && v <= 1.f))
        return false;
    if (dist)
        *dist = v;
    return true;
}

// inline bool intersectSegmentRectangle(const float2 &a, const float2 &b, const float2 &c, const float2 &r)
// {
//     return intersectPointRectangle(a, c, r) ||
//         intersectPointRectangle(b, c, r) ||
//         intersectSegmentSegment(a, b, c + flipX(r), c + r) ||
//         intersectSegmentSegment(a, b, c + r, c + flipY(r)) ||
//         intersectSegmentSegment(a, b, c + flipY(r), c -r) ||
//         intersectSegmentSegment(a, b, c - r, c + flipX(r));
// }


float2 rectangleEdge(float2 rpos, float2 rrad, float2 dir);

// from http://www.blackpawn.com/texts/pointinpoly/
bool intersectPointTriangle(float2 P, float2 A, float2 B, float2 C);

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


// toroidally wrapped 2d space

// shortest vector pointing from q to p (p-q)
inline float2 toroidalDelta(const float2 &p, const float2 &q, const float2 &w)
{
    DASSERT(w.x > 0 && w.y > 0 &&
            intersectPointRectangleCorners(p, float2(0.f), w) &&
            intersectPointRectangleCorners(q, float2(0.f), w));

    const float2 v = p - q;

    return min_abs(v, min_abs(v-w, v+w));
}

// closest mapped position for POS relative to VIEW
inline float2 toroidalPosition(const float2 &pos, const float2 &view, const float2 &w)
{
    const float2 delta = toroidalDelta(pos, modulo(view, w), w);
    return view + delta;
}

inline float toroidalDistanceSqr(const float2 &p, const float2 &q, const float2 &w)
{
    const float2 d = toroidalDelta(p, q, w);
    return d.x * d.x + d.y * d.y;
}

inline float toroidalDistance(const float2 &p, const float2 &q, const float2 &w)
{
    return sqrt(toroidalDistanceSqr(p, q, w));
}

inline bool toroidalIntersectPointCircle(const float2 &p, const float2 &c, float r, const float2 &w)
{
    return toroidalDistanceSqr(p, c, w) <= (r*r);
}

inline bool toroidalIntersectCircleCircle(const float2 &p, const float pr,
                                          const float2 &c, float cr,
                                          const float2 &w)
{
    const float r = pr + cr;
    return toroidalDistanceSqr(p, c, w) <= (r*r);
}

inline bool toroidalIntersectCircleRectangle(const float2 &p, const float pr,
                                             const float2 &c, const float2 &cr,
                                             const float2 &w)
{
    const float2 delt = abs(toroidalDelta(p, c, w));
    return _intersectCircleRectangle1(delt, pr, cr);
}

inline bool toroidalIntersectRectangleRectangle(const float2 &p, const float2 &pr,
                                                const float2 &c, const float2 &cr,
                                                const float2 &w)
{
    const float2 delt = abs(toroidalDelta(p, c, w));
    return delt.x <= (pr.x + cr.x) && delt.y <= (pr.y + cr.y);
}


////////////////////////////////////////////////////////////////////////////



// http://en.wikipedia.org/wiki/False_position_method#Numerical_analysis
template <typename Fun>
static double findRootRegulaFalsi(Fun fun, double x_lo, double x_hi,
                                  double error=epsilon, int iterations=50)
{
    double y_hi = fun(x_hi);
    double y_lo = fun(x_lo);

    ASSERT(y_lo <= 0 && 0 <= y_hi);
    ASSERT(error > 0);

    double x = (y_hi < -y_lo) ? x_hi : x_lo;
    double y = (y_hi < -y_lo) ? y_hi : y_lo;

    while (fabs(y) > error && --iterations)
    {
        x = x_hi - (y_hi * (x_hi - x_lo)) / (y_hi - y_lo);
        y = fun(x);
        ((y > 0) ? x_hi : x_lo) = x;
        ((y > 0) ? y_hi : y_lo) = y;
    }
    ASSERT(iterations);
    return x;
}


// return x where f(x) = 0
// funp is the derivative of fun
template <typename Fun, typename FunP>
double findRootNewton(Fun fun, FunP funp, double x0,
                      double error=epsilon, int iterations=50)
{
    double x = x0;
    double y = fun(x);
    
    while (abs(y) > error && --iterations)
    {
        y = fun(x);
        double dy = funp(x);
        // if (abs(dy) < epsilon)
            // dy = fun(x)
        x = x - y / dy;
    }
    ASSERT(iterations);
    return x;
}


// return number of real solutions, with values in r0 (smaller) and r1 (larger)
// x where a^2x + bx + c = 0
int quadraticFormula(double* r0, double* r1, double a, double b, double c);

bool mathRunTests();

struct SlopeLine {
    float slope = 0.f;
    float y_int = 0.f;
    float eval(float x) const { return slope * x + y_int; }
};

// least squares
struct LinearRegression {
    double2 sum;                // sum of values
    double2 sumx;               // sum of x * values
    int     count = 0;

    void insert(float2 p);
    SlopeLine calculate() const;
};

struct Variance {
    int n = 0;
    double sumweight=0.0, mean=0.0, m2=0.0;

    void insert(double val, double weight=1.0);
    double variance() const;
    double stddev() const { return sqrt(variance()); }
};

struct Variance3D {
    Variance x, y, z;

    void insert(d3 val, double w=1)
    {
        x.insert(val.x);
        y.insert(val.y);
        z.insert(val.z);
    }
    d3 mean()     const { return d3(x.mean, y.mean, z.mean); }
    d3 variance() const { return d3(x.variance(), y.variance(), z.variance()); }
    d3 stddev()   const { return d3(x.stddev(), y.stddev(), z.stddev()); }
};

struct VarianceWindow {
    int                size = 100;
    std::deque<double> data;
    double sumv=0.0, sumv2=0.0, mean=0.0, variance=0.0;

    void insert(double newv);
};


// http://petrocket.blogspot.com/2010/04/sphere-to-cube-mapping.html
inline double3 cube2sphere(const double3 &c)
{
    return c * sqrt(d3(1.0 - c.y * c.y * 0.5 - c.z * c.z * 0.5 + c.y * c.y * c.z * c.z / 3.0,
                    1.0 - c.z * c.z * 0.5 - c.x * c.x * 0.5 + c.z * c.z * c.x * c.x / 3.0,
                    1.0 - c.x * c.x * 0.5 - c.y * c.y * 0.5 + c.x * c.x * c.y * c.y / 3.0));
}

double3 sphere2cube(const double3 &s);

// x, then y, then z
dmat2 eul2mat(double a);
dmat3 eul2mat(const d3 &eul);
d3 mat2eul(const dmat3 &m);

dmat3 rotate(const d3 &axis, const double &angle);
d3 rotate_vec(const d3 &axis, const double &angle);

// rotate such that A goes to B
// use plane with A, B, and origin, if possible
template <typename T>
inline glm::tmat4x4<T> rotateV2V(const glm::tvec3<T> &a, const glm::tvec3<T> &b)
{
    typedef glm::tvec3<T> V;
    
    const V pa = normalize(a);
    const V pb = normalize(b);
    if (nearZero(pa - pb))
        return glm::tmat4x4<T>(1.0);
    else if (nearZero(pa + pb)) {
        // opposite directions, pick arbitrary axis
        V ax = V(1.0, 0.0, 0.0);
        if (dot(ax, pa) > 0.9)
            ax = V(0.0, 1.0, 0.0);
        return glm::rotate((T)M_PI, ax);
    } else
        return glm::rotate(acos(dot(pa, pb)), cross(pa, pb));
}

// http://lolengine.net/blog/2014/02/24/quaternion-from-two-vectors-final
template <typename T>
glm::tquat<T> rotate_between(glm::tvec3<T> u, glm::tvec3<T> v)
{
    typedef glm::tvec3<T> V3;
    
	// u = normalize(u);
	// v = normalize(v);

    T norm_u_norm_v = sqrt(dot(u, u) * dot(v, v));
    T real_part = norm_u_norm_v + dot(u, v);
    V3 w;

    if (real_part < 1.e-6f * norm_u_norm_v)
    {
        /* If u and v are exactly opposite, rotate 180 degrees
         * around an arbitrary orthogonal axis. Axis normalisation
         * can happen later, when we normalise the quaternion. */
        real_part = 0.0f;
        w = abs(u.x) > abs(u.z) ? V3(-u.y, u.x, 0)
                                : V3(0, -u.z, u.y);
    }
    else
    {
        /* Otherwise, build quaternion the standard way. */
        w = cross(u, v);
    }

    return normalize(quat(real_part, w.x, w.y, w.z));
}

inline float2 rotate_between(float2 u, float2 v)
{
    // return a2v(v2a(v) - v2a(u));
    return rotateN(v, u);
}

float rand_normal(float mean, float stddev);
VECTORIZE(T, rand_normal, V1S1);

// https://en.wikipedia.org/wiki/Barycentric_coordinate_system
inline f2 barycentric_to_cartesian(const f3 &lambda, const f2 &a, const f2 &b, const f2 &c)
{
	return glm::mat3x2(a, b, c) * lambda;
}

inline f3 barycentric_to_cartesian(const f3 &lambda, const f3 &a, const f3 &b, const f3 &c)
{
    return mat3(a, b, c) * lambda;
}

inline f3 cartesian_to_barycentric(const f2 &uv, const f2 &a, const f2 &b, const f2 &c)
{
    f2 l = glm::inverse(mat2(a - c, b - c)) * (uv - c);
    return f3(l, 1.f - l.x - l.y);
}

inline f3 barycentric_clamp(f3 lambda)
{
    if (lambda.z > 0.0) {
        lambda.xy() = clamp((f2)lambda.xy(), f2(0.f), f2(1.f));
        lambda.z = 1.f - lambda.x - lambda.y;
    } else {
        lambda.xz() = clamp((f2)lambda.xz(), f2(0.f), f2(1.f));
        lambda.y = 1.f - lambda.x - lambda.z;
    }
    return lambda;
}

inline bool barycentric_inside(const f3 &lambda)
{
    return (lambda.x >= 0.0 && lambda.y >= 0.0 && lambda.x + lambda.y < 1.0);
}



inline double asin_clamp(double x)
{
    return ((x <= -1.0) ? -M_PI_2 :
            (x >= 1.0) ? M_PI_2 :
            std::asin(x));
}

inline float asin_clamp(float x)
{
    return ((x <= -1.f) ? -M_PI_2f :
            (x >= 1.f) ? M_PI_2f :
            std::asin(x));
}

// NORM or normalized coordinates are in rectangular space centered at the origin
// +Z is the north pole
// +X is the prime meridian/equator
// length is up

// SPHERE or spherical coordinates are latitude/longitude/up in radians
// they may also be local, as a velocity on the surface.
// X is east, Y is north, Z is up
// vectors in spherical coordinates are called HORIZONTAL

d2 norm2lnglat(const d3 &pos);
string fmt_lnglat(d2 ll);

inline d3 lnglat2norm(d2 ll)
{
    const d2 l = kToRadians * ll;
    return d3(a2v(l.x) * cos(l.y), sin(l.y));
}

inline d3 lnglat2up(d2 ll)
{
    const d2 l = kToRadians * ll;
    return d3(-a2v(l.x) * sin(l.y), abs(cos(l.y)));
}

inline f3 norm2sphere(f3 pos)
{
    const float len = length(pos);
    pos /= len;
    return f3(v2a(f2(pos.x, pos.y)), asin_clamp(pos.z), len);
}


// { [-pi, pi], [-pi/2, pi/2], n }
d3 norm2sphere(d3 pos);

inline d3 sphere2norm(const d3 &l) { return l.z * d3(a2v(l.x) * cos(l.y), sin(l.y)); } 
inline d3 sphere2norm(const d2 &l) { return d3(a2v(l.x) * cos(l.y), sin(l.y)); }

inline d2 norm2uv(const d3 &pos) { return (d2(norm2sphere(pos)) + d2(M_PI, M_PI_2)) / d2(M_TAU, M_PI); }
inline d3 uv2norm(const d2 &uv) { return sphere2norm((uv * d2(M_TAU, M_PI)) - d2(M_PI, M_PI_2)); }
inline f2 norm2uv(const f3 &pos) { return (f2(norm2sphere(pos)) + f2(M_PIf, M_PI_2f)) / f2(M_TAUf, M_PIf); }
