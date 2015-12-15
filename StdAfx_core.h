
//
// StdAfx_core.h
//
// Top level include for Outlaws platform abstraction layer

#ifndef STDAFX_CORE_H
#define STDAFX_CORE_H

#include "PlatformIncludes.h"
#include "Outlaws.h"

#include <cstdio>
#include <cstdlib>

#include <memory>
#include <thread>
#include <mutex>

#include "Str.h"

using std::deque;
using std::string;
using std::pair;
using std::make_pair;
using std::unique_ptr;
using std::shared_ptr;
using namespace std::rel_ops;
using std::vector;
using std::swap;

// last chance to catch log string before it hits the OS layer
void Report(string &&str);
void Report(const string &str);
void Report(const char* str);

inline void vReportf(const char *format, va_list vl)  __printflike(1, 0);

inline void vReportf(const char *format, va_list vl)
{
    Report(str_vformat(format, vl));
}

inline void Reportf(const char *format, ...)  __printflike(1, 2);

inline void Reportf(const char *format, ...)
{
    va_list vl;
    va_start(vl, format);
    vReportf(format, vl);
    va_end(vl);
}

#define TYPE_NAME(X) (str_demangle(typeid(X).name()).c_str())
#define TYPE_NAME_S(X) str_demangle(typeid(X).name())

// use pthreads instead of std::thread on mac because the default stack size is really small and
// there is no way to change it using std::thread
#define OL_USE_PTHREADS defined(__APPLE__)

#if TARGET_OS_IPHONE
#define IS_TABLET 1
#else
#define IS_TABLET 0
#endif

#define arraySize(X) (sizeof(X) / sizeof(X[0]))

#define SIZEOF_PTR(X) ((X) ? sizeof(*(X)) : 0)
#define SIZEOF_REC(X) ((X) ? (X)->getSizeof() : 0)
#define SIZEOF_IREC(X) ((X).getSizeof() - sizeof(X))
#define SIZEOF_VEC(X) (sizeof((X)[0]) * (X).capacity())
#define SIZEOF_PVEC(X) ((X) ? SIZEOF_VEC(*X) : 0)
#define SIZEOF_ARR(X, Y) ((X) ? (sizeof(X[0]) * (Y)) : 0)

#define UNUSED(x) (void)(x)

// assertions are enabled even in release builds
#define ASSERT_FAILED(X, Y, ...) OLG_OnAssertFailed(__FILE__, __LINE__, __func__, (X), (Y), ## __VA_ARGS__)
#define ASSERT(X) (__builtin_expect(!(X), 0) ? ASSERT_FAILED(#X, "") : 0)
#define ASSERTF(X, Y, ...) (__builtin_expect(!(X), 0) ? ASSERT_FAILED(#X, Y, __VA_ARGS__) : 0)
#define ASSERT_(X, FL, LN, FN, Y, ...) (__builtin_expect(!(X), 0) ? OLG_OnAssertFailed(FL, LN, FN, #X, (Y), ## __VA_ARGS__) : 0)

#define assert_eql(A, B) ASSERTF(A == B, "'%s' != '%s'", str_tocstr(A), str_tocstr(B))

#define CASE_STR(X) case X: return #X

#if defined(DEBUG) || defined(DEVELOP)
#  define WARN(X) Reportf X
template <int Y, typename T>
void DPRINTVAR1(const char* name, const T& X)
{
    static T _x{};
    static std::string _nm;
    if (X != _x && name != _nm) {
        Reportf("%s = %s", name, str_tostr(X).c_str());
        _x = X;
        _nm = name;
    }
}
#  define DPRINTVAR(X) DPRINTVAR1<__LINE__>(#X, X)
#  define IS_DEVEL (1)
#else
#  define DPRINTVAR(X) do{(void)sizeof(X);}while(0)
#  define WARN(X) do{(void) sizeof(X);}while(0)
#  define NDEBUG 1
#  define IS_DEVEL (0)
#  define RELEASE 1
#endif

// special assertion only enabled in debug builds
#if defined(DEBUG)
#  define DASSERT(X) ASSERT(X)
#  define IS_DEBUG (1)
#else
#  define DASSERT(X) do{(void) sizeof(X);}while(0)
#  define IS_DEBUG (0)
#endif


// base class for deferred deletion by main thread
struct IDeletable {

    virtual void onQueueForDelete() {}
    virtual ~IDeletable() {}
}; 

#if _MSC_VER
#define THREAD_LOCAL __declspec(thread)
#else
#define THREAD_LOCAL __thread
#endif

#include "Geometry.h"
#include "Tween.h"
#include "stl_ext.h"
#include "Rand.h"
#include "SpacialHash.h"
#include "SerialCore.h"
#include "Event.h"

// do interpolation between two world positions from render thread
struct RenderFloat2 {
    
    float2 last, current, render;

    void set(float2 next)
    {
        last = current = render = next;
    }
    
    void advanceUpdate(float2 next)
    {
        last = current;
        current = next;
    }

    void advanceRender(float interpolant)
    {
        render = lerp(last, current, interpolant);
    }
};

struct RenderAngle {
    
    float last=0.f, current=0.f, render=0.f;

    void set(float next)
    {
        last = current = render = next;
    }
    void advanceUpdate(float next)
    {
        last = current;
        current = next;
    }

    void advanceRender(float interpolant)
    {
        render = lerpAngles(last, current, interpolant);
    }
};

#endif // STDAFX_CORE_H
