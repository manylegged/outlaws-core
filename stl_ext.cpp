
#include "StdAfx.h"
#include "stl_ext.h"

int findLeadingOne(uint v, int i)
{
    if (v&0xffff0000) { i += 16; v >>= 16; }
    if (v&0xff00)     { i +=  8; v >>=  8; }
    if (v&0xf0)       { i +=  4; v >>=  4; }
    if (v&0xc)        { i +=  2; v >>=  2; }
    if (v&0x2)        { return i + 1;      }
    if (v&0x1)        { return i;          }
    return -1;
}

int findLeadingOne(uint64 v)
{
    return (v&0xffffffff00000000L) ? findLeadingOne((uint) (v>>32), 32) : findLeadingOne((uint)v, 0);
}

void watch_ptr_base::unlink()
{
    if (next)
        next->prev = prev;
    if (prev)
        prev->next = next;
    next = NULL;
    prev = NULL;

    ptr  = NULL;
}

void watch_ptr_base::link(const Watchable* p)
{
    ptr = p;
    ASSERT(prev == NULL);
    ASSERT(next == NULL);
    if (p != NULL)
    {
        prev = &p->watch_list;
        next = p->watch_list.next;
        if (p->watch_list.next)
            p->watch_list.next->prev = this;
        p->watch_list.next = this;
    }
}


void Watchable::nullReferencesTo()
{
    for (watch_ptr_base* watcher=watch_list.next; watcher != NULL; watcher = watcher->next)
    {
        watcher->ptr = NULL;
        if (watcher->prev)
            watcher->prev->next = NULL;
        watcher->prev = NULL;
    }
}

std::mutex& _thread_name_mutex()
{
    static std::mutex m;
    return m;
}


OL_ThreadNames &_thread_name_map()
{
    static OL_ThreadNames *names = new OL_ThreadNames();
    return *names;
}

#if _WIN32
//
// Usage: SetThreadName (-1, "MainThread");
//
const DWORD MS_VC_EXCEPTION = 0x406D1388;

#pragma pack(push,8)
typedef struct tagTHREADNAME_INFO
{
    DWORD dwType; // Must be 0x1000.
    LPCSTR szName; // Pointer to name (in user addr space).
    DWORD dwThreadID; // Thread ID (-1=caller thread).
    DWORD dwFlags; // Reserved for future use, must be zero.
} THREADNAME_INFO;
#pragma pack(pop)

void SetThreadName(DWORD dwThreadID, const char* threadName)
{
    THREADNAME_INFO info;
    info.dwType = 0x1000;
    info.szName = threadName;
    info.dwThreadID = dwThreadID;
    info.dwFlags = 0;

    __try
    {
        RaiseException(MS_VC_EXCEPTION, 0, sizeof(info) / sizeof(ULONG_PTR), (ULONG_PTR*)&info);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
    }
}

#endif

static void myTerminateHandler()
{
    ReportMessage("terminate handler called");
    const char* message = "<no info>";
    std::exception_ptr eptr = std::current_exception();
    try {
        if (eptr) {
            std::rethrow_exception(eptr);
        }
    } catch(const std::exception& e) {
        message = e.what();
    }

    ASSERT_FAILED("Terminate Handler", "Exception: %s", message);
    OL_OnTerminate(message);
    exit(1);
}

void thread_setup(const char* name)
{
    std::set_terminate(myTerminateHandler);
    // random number generator per-thread
    random_device() = new std::mt19937(random_seed());

    uint64 tid = 0;
#if _WIN32
    tid = GetCurrentThreadId();
    SetThreadName(tid, name);
#elif __APPLE__
    pthread_threadid_np(pthread_self(), &tid);
    pthread_setname_np(name);
#else // linux
    static_assert(sizeof(pthread_t) <= sizeof(tid), "");
    tid = pthread_self();
    int status = 0;
    // 16 character maximum!
    if ((status = pthread_setname_np(pthread_self(), name)))
        ReportMessagef("pthread_setname_np(pthread_t, const char*) failed: %s", strerror(status));
#endif

    {
        std::lock_guard<std::mutex> l(_thread_name_mutex());
        _thread_name_map()[tid] = name;
    }

    ReportMessagef("Thread %#llx is named '%s'", tid, name);
}


#if OL_USE_PTHREADS

pthread_t thread_create(void *(*start_routine)(void *), void *arg)
{
    int            err = 0;
    pthread_attr_t attr;
    pthread_t      thread;

    err = pthread_attr_init(&attr);
    if (err)
        ReportMessagef("pthread_attr_init error: %s", strerror(err));
    err = pthread_attr_setstacksize(&attr, 8 * 1024 * 1024);
    if (err)
        ReportMessagef("pthread_attr_setstacksize error: %s", strerror(err));
    err = pthread_create(&thread, &attr, start_routine, arg);
    if (err)
        ReportMessagef("pthread_create error: %s", strerror(err));
    return thread;
}


void thread_join(pthread_t &thread)
{
    if (!thread)
        return;
    int status = pthread_join(thread, NULL);
    ASSERTF(status == 0, "pthread_join: %s", strerror(status));
}

#else

std::thread thread_create(void *(*start_routine)(void *), void *arg)
{
    return std::thread(start_routine, arg);
}

void thread_join(std::thread& thread)
{
    if (!thread.joinable())
        return;
    try {
        thread.join();
    } catch (std::exception &e) {
        ASSERT_FAILED("std::thread::join()", "%s", e.what());
    }
}

#endif


MemoryPool::MemoryPool(size_t sz, size_t cnt) : element_size(sz), count(cnt)
{
    do {
        pool = (char*)malloc(count * element_size);
        ReportMessagef("Allocating MemoryPool(%d, %d) %.1fMB: %s", 
                       (int)element_size, (int)count, 
                       (element_size * count) / (1024.0 * 1024.0),
                       pool ? "OK" : "FAILED");
        if (!pool)
            count /= 2;
    } while (count && !pool);
    first = (Chunk*) pool;
    for (int i=0; i<count-1; i++) {
        ((Chunk*) &pool[i * element_size])->next = (Chunk*) &pool[(i+1) * element_size];
    }
}

MemoryPool::~MemoryPool()
{
    free(pool);
    delete next;
}

bool MemoryPool::isInPool(const void *pt) const
{
    const char *ptr = (char*) pt;
    const size_t index = (ptr - pool) / element_size;
    if (index >= count)
        return next ? next->isInPool(pt) : false;
    ASSERT(pool + (index  * element_size) == ptr);
    return true;
}

void* MemoryPool::allocate()
{
    std::lock_guard<std::mutex> l(mutex);

    if (!first) {
        if (!next) {
            next = new MemoryPool(element_size, count);
        }
        return next->allocate();
    }

    Chunk *chunk = first;
    first = first->next;
    used++;
    return (void*) chunk;
}

void MemoryPool::deallocate(void *ptr)
{
    std::lock_guard<std::mutex> l(mutex);

    if (!isInPool(ptr))
    {
        ASSERT(next);
        if (next)
            next->deallocate(ptr);
        return;
    }

    Chunk *chunk = (Chunk*) ptr;
    chunk->next = first;
    first = chunk;
    used--;
}

