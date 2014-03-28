
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

inline void vReportMessagef(const char *format, va_list vl)  __printflike(1, 0);

inline void vReportMessagef(const char *format, va_list vl)
{
    string buf = str_vformat(format, vl);
    OL_ReportMessage(buf.c_str());
}

inline void ReportMessagef(const char *format, ...)  __printflike(1, 2);

inline void ReportMessagef(const char *format, ...)
{
    va_list vl;
    va_start(vl, format);
    vReportMessagef(format, vl);
    va_end(vl);
}


// eat the first two numbers...
#define TYPE_NAME(X) (typeid(X).name())

#define arraySize(X) (sizeof(X) / sizeof(X[0]))

// assertions are enabled even in release builds
#define ASSERT(X) if (__builtin_expect(!(X), 0)) OLG_OnAssertFailed(__FILE__, __LINE__, __func__, #X, "")
#define ASSERTF(X, Y, ...) if (__builtin_expect(!(X), 0)) OLG_OnAssertFailed(__FILE__, __LINE__, __func__, #X, (Y), __VA_ARGS__)


#if !defined(DEBUG) && !defined(DEVELOP)
#  define DPRINTVAR(X) do{(void)sizeof(X);}while(0)
#  define WARN(X) do{(void) sizeof(X);}while(0)
#  define NDEBUG 1
#else
#  define WARN(X) ReportMessagef X
#  define DPRINTVAR(X) DebugPrintVar(#X, (X))
#endif

// special assertion only enabled in debug builds
#if !defined(DEBUG)
#  define DASSERT(X) do{(void) sizeof(X);}while(0)
#else
#  define DASSERT(X) ASSERT(X)
#endif


// base class for deferred deletion by main thread
struct IDeletable {

    virtual ~IDeletable() {}
}; 

#include "Geometry.h"
#include "Rand.h"
#include "stl_ext.h"
#include "SpacialHash.h"
#include "SerialCore.h"
#include "Event.h"

inline void DebugPrintVar(const char* str, int v) { ReportMessagef("%s = %d", str, v); }
inline void DebugPrintVar(const char* str, float v) { ReportMessagef("%s = %g", str, v); }
inline void DebugPrintVar(const char* str, float2 v) { ReportMessagef("%s = (%g, %g)", str, v.x, v.y); }
inline void DebugPrintVar(const char* str, float3 v) { ReportMessagef("%s = (%g, %g, %g)", str, v.x, v.y, v.z); }


#endif // STDAFX_CORE_H
