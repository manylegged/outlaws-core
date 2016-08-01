
//
// stl_ext.h - general purpose standard library type stuff.
// - smart pointers
// - stl container convenience wrappers
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

#ifndef STL_EXT_H
#define STL_EXT_H

#include <vector>
#include <map>
#include <set>
#include <algorithm>
#include <type_traits>

// c++11 ranged for loop
#define foreach(A, B) for (A : (B))

// c++17 simplified ranged for loop
#define for_(A, B) for (auto &&A : (B))


#define unless(X) if (!(X))

// lambda((int* a), *a + 3)
// #define lambda(X, Y) [&] X { return (Y); }

// lisp style multi-argument or_ and and_
template <typename T> T or_(const T& a, const T& b) { return a ? a : b; }
template <typename T> T or_(const T& a, const T& b, const T& c) { return a ? a : b ? b : c; }
template <typename T> T or_(const T& a, const T& b, const T& c, const T& d) { return a ? a : b ? b : c ? c : d; }
inline std::string or_(const std::string &a, const std::string &b) { return a.size() ? a : b; }
inline std::string or_(const std::string &a, const std::string &b, const std::string &c) { return a.size() ? a : b.size() ? b : c; }
inline const char* or_(const char* a, const char* b) { return str_len(a) ? a : b; }
inline const char* or_(const char* a, const char* b, const char* c) { return str_len(a) ? a : str_len(b) ? b : c; }
inline lstring or_(lstring a, lstring b) { return str_len(a) ? a : b; }
inline lstring or_(lstring a, lstring b, lstring c) { return str_len(a) ? a : str_len(b) ? b : c; }


template <typename S, typename T> 
T and_(const S& a, const T& b) { return a ? b : T(a); }

template <typename S, typename T> 
T and_(const S& a, const T& b, const T& c) { return a ? (b ? c : b) : T(a); }

// multi-argument min, max
template <typename T>
T min(const T& a, const T& b, const T& c) { return min(min(a, b), c); }

template <typename T>
T min(const T& a, const T& b, const T& c, const T& d) { return min(min(a, b), min(c, d)); }

template <typename T>
T max(const T& a, const T& b, const T& c) { return max(max(a, b), c); }

template <typename T>
T max(const T& a, const T& b, const T& c, const T& d) { return max(max(a, b), max(c, d)); }

// return bit index of leading 1 (0x80000000 == 0x80030100 == 31, 0x1 == 0, 0x0 == -1)
int findLeadingOne(uint v, int i=0);
int findLeadingOne(uint64 v);

template <typename T>
inline T setBits(T &bits, const T &flag, bool val)
{
    const T was = bits&flag;
    if (val)
        bits |= flag;
    else
        bits &= ~flag;
    return was;
}

template <typename T>
inline bool hasBits(const T& val, const T& bits)
{
    return (val&bits) == bits;
}

inline bool isPow2(int x)
{ 
    return (x&(x-1)) == 0; 
}

template <typename T>
std::unique_ptr<T> make_unique(T* val)
{
    std::unique_ptr<T> ptr(val);
    return ptr;
}

inline std::size_t hash_combine(const std::size_t &a, const std::size_t &b)
{
    return a ^ (b + 0x9e3779b9 + (a<<6) + (a>>2));
}

namespace std {
    
    template <typename K, typename V>
    struct hash< std::pair<K, V> > {
        std::size_t operator()(const std::pair<K, V>& pt) const
        {
            return hash_combine(std::hash<K>()(pt.first), std::hash<V>()(pt.second));
        }
    };

    template <typename T>
    struct hash< glm::tvec2<T> > {
        std::size_t operator()(const glm::tvec2<T>& pt) const
        {
            return hash_combine(std::hash<T>()(pt.x), std::hash<T>()(pt.y));
        }
    };

    template <typename T>
    struct hash< glm::tvec3<T> > {
        std::size_t operator()(const glm::tvec3<T>& pt) const
        {
            return hash_combine(std::hash<T>()(pt.x),
                                hash_combine(std::hash<T>()(pt.y),
                                             std::hash<T>()(pt.z)));
        }
    };


    // allow foreach(foo &, make_pair(array_of_foo, array_of_foo + size))

    template <typename T> T* begin(std::pair<T*, T*> const& p)
    {
        return p.first;
    }

    template <typename T> T* end(std::pair<T*, T*> const& p)
    {
        return p.second;
    }
}


template <typename T> bool copy_explicit_owner(const T* ptr) { return NULL; }
template <typename T> bool copy_refcount(const T* ptr, int delta) { return false; }

template <typename T>
bool copy_delete(T *v)
{
    if (!v || copy_explicit_owner(v) || copy_refcount(v, -1))
        return false;
    delete v;
    return true;
}

template <typename T>
inline void deleteNull(T*& v)
{
    copy_delete(v);
    v = NULL;
}

template <typename T> struct watch_ptr;

template <typename T>
inline void deleteNull(watch_ptr<T>& v)
{
    copy_delete(v.get());
    v = NULL;
}

// smart pointer supporting distributed and centralized ownership
// overload copy_explicit_owner for explicit/centralized ownership
// overload copy_refcount for reference counting
// otherwise copy the object around and delete on destruction
template <typename T>
class copy_ptr final {

    T* m_ptr = NULL;

    template <typename U>
    void assign1(const T* v, typename std::enable_if<!std::is_const<U>::value>::type* = 0) { *m_ptr = *v; }
    template <typename U>
    void assign1(const T* v, typename std::enable_if<std::is_const<U>::value>::type* = 0) { }

public:

    ~copy_ptr() { reset(); }

    // default construct to null
    copy_ptr() NOEXCEPT { }
    copy_ptr(std::nullptr_t) NOEXCEPT { }
    //copy_ptr(int) { }

    // take ownership
    explicit copy_ptr(T* &&t) NOEXCEPT : m_ptr(t) { t = NULL; }
    copy_ptr& operator=(T* &&t) NOEXCEPT { reset(t); t = NULL; return *this; }
    copy_ptr(copy_ptr&& o) NOEXCEPT : m_ptr(o.m_ptr) { o.m_ptr = NULL; }
    copy_ptr& operator=(copy_ptr&& o) NOEXCEPT { reset(o.m_ptr); o.m_ptr = NULL; return *this; }
    copy_ptr& reset(T* v=NULL)
    {
        if (m_ptr == v)
            return *this;
        copy_delete(m_ptr);
        m_ptr = v;
        return *this;
    }

    // copy data when assigning unless data has explicit owner
    copy_ptr(const copy_ptr& o) { assign(o.m_ptr); }
    explicit copy_ptr(const T *t) { assign(t); }
    copy_ptr& operator=(const copy_ptr& o) { return assign(o.m_ptr); }
    copy_ptr& assign(typename std::remove_const<T>::type *v)
    {
        if (m_ptr == v)
            return *this;
        else if (!v || copy_explicit_owner(v) || (std::is_const<T>::value && copy_refcount(v, +1)))
            return reset(v);
        else
            return assign(const_cast<const T *>(v));
    }
    copy_ptr& assign(const T* v)
    {
        if (m_ptr == v)
            ;
        else if (!std::is_const<T>::value && m_ptr && !copy_explicit_owner(m_ptr))
            assign1<T>(v);
        else
            reset(new T(*v));
        return *this;
    }
    
    // forward assignment from pointee type - copy
    copy_ptr& operator=(const T& o)
    {
        ASSERT(!copy_explicit_owner(&o)); // technically fine, but we should be setting by pointer
        if (m_ptr)
            *m_ptr = o;
        else
            m_ptr = new T(o);
        return *this;
    }

    // forward assignment from pointee type - move
    copy_ptr& operator=(T&& o) NOEXCEPT
    {
        ASSERT(!copy_explicit_owner(&o)); // technically fine, but we should be setting by pointer
        if (m_ptr)
            *m_ptr = std::move(o);
        else
            m_ptr = new T(std::move(o));
        return *this;
    }
     
    // it's a smart pointer
    T& operator*()  const { return *m_ptr; }
    T* operator->() const { return m_ptr; }
    bool operator==(const copy_ptr &o) const
    {
        return (m_ptr == o.m_ptr || (m_ptr && o.m_ptr && *m_ptr == *o.m_ptr)); 
    }
    bool operator!=(const copy_ptr &o) const { return !(*this == o); }

    explicit operator bool()           const { return m_ptr != NULL; }

    T*  get() const { return m_ptr; }
    T** getPtr()    { return &m_ptr; }

    T* release()
    {
        T* ptr = m_ptr;
        m_ptr = NULL;
        return ptr;
    }
};

template <typename T>
copy_ptr<T> make_copy(T* &&v)
{
    copy_ptr<T> ptr(std::forward<T*>(v));
    return ptr;
}

// watch_ptr is a smart pointer that automatically becomes NULL when it's pointee is deleted

struct Watchable;

struct watch_ptr_base {

    const Watchable* ptr  = NULL;
    watch_ptr_base*  next = NULL;
    watch_ptr_base*  prev = NULL;

protected:
    void unlink();
    void link(const Watchable* p);
};

struct Watchable : public IDeletable {

    mutable watch_ptr_base watch_list;

    void nullReferencesTo();

    void onQueueForDelete()
    {
        nullReferencesTo();
    }

    ~Watchable()
    {
        nullReferencesTo();
    }
};

// smart pointer that automatically becomes NULL when it's pointee is deleted
template <typename T>
struct watch_ptr final : public watch_ptr_base {

    ~watch_ptr() 
    {
        unlink();
    }

    // default construct to null
    watch_ptr() NOEXCEPT { }
    watch_ptr(std::nullptr_t) NOEXCEPT { }

    explicit watch_ptr(T *t) NOEXCEPT { link(t); }
    watch_ptr(const watch_ptr &t) NOEXCEPT { link(t.ptr); }
    template <typename U>
    watch_ptr(const watch_ptr<U> &t) NOEXCEPT { link(t.ptr); }

    watch_ptr& operator=(T* t)
    {
        unlink();
        link(t);
        return *this;
    }

    watch_ptr& operator=(const watch_ptr& o) 
    {
        unlink();
        link(o.ptr);
        return *this;
    }
     
    // it's a smart pointer
    T& operator*()                      const { return *get(); }
    T* operator->()                     const { return get(); }
    bool operator==(const watch_ptr &o) const { return ptr == o.ptr; }
    bool operator==(const T* o)         const { return ptr == o; }
    bool operator<(const watch_ptr &o)  const { return ptr < o.ptr; }
    bool operator<(const T* o)          const { return ptr < o; }
    explicit operator bool()            const { return ptr != NULL; }

    T* get() const { return (T*) ptr; }
};

template <typename T> bool operator!=(const watch_ptr<T> &a, T* b) { return !(a == b); }
template <typename T> bool operator!=(T* a, const watch_ptr<T> &b) { return !(b == a); }

template <typename T>
watch_ptr<T> make_watch(T* v)
{
    watch_ptr<T> p(v);
    return p;
}


// vector ///////////////////////////////////////////////////////////////////////////////

template <typename V, typename T>
inline bool vec_contains(const V &v, const T& t)
{
    for (const auto &x : v) {
        if (x == t)
            return true;
    }
    return false;
}

template <typename T>
inline bool vec_contains(const std::initializer_list<T> &v, const T& t)
{
    auto end = std::end(v);
    return std::find(std::begin(v), end, t) != end;
}

template <typename V, typename Fun>
inline void vec_foreach(V& vec, Fun& fun)
{
    foreach (auto &v, vec) {
        fun(v);
    }
}

template <typename V, typename T>
inline int vec_count(const V &v, const T& t)
{
    int count = 0;
    foreach (const auto &x, v) {
        if (x == t)
            count++;
    }
    return count;
}

template <typename V, typename T>
inline size_t vec_find_index(const V &v, const T& t)
{
    auto begin = std::begin(v);
    auto end = std::end(v);
    auto it = std::find(begin, end, t);
    if (it == end)
        return -1;
    else
        return it - begin;
}

struct dummy_ref_t {
    enum { max_bytes = 16 };
    char bytes[max_bytes];
};
extern dummy_ref_t the_dummy_ref;

template <typename T>
struct can_use_zero {
    typedef typename std::remove_reference<T>::type type;
    enum { value = std::is_pod<type>::value || std::is_scalar<T>::value };
};

// default values when we need a reference
template <typename T>
const T &dummy_ref(typename std::enable_if<can_use_zero<T>::value>::type* = 0)
{
    static_assert(sizeof(T) <= sizeof(dummy_ref_t), "increase dummy_ref_t::max_bytes");
    return (const T&) the_dummy_ref;
}

template <typename T>
const T &dummy_ref_()
{
    static T v = T();
    return v;
}

template <typename T>
const T &dummy_ref(typename std::enable_if<!can_use_zero<T>::value>::type* = 0)
{
    return dummy_ref_<typename std::remove_reference<typename std::remove_cv<T>::type>::type>();
}

template <typename V, typename Fun>
const typename V::value_type &vec_find(
    const V &vec, const Fun& fun, const typename V::value_type &def=dummy_ref<typename V::value_type>())
{
    foreach (auto& x, vec) {
        if (fun(x))
            return x;
    }
    return def;
}

// return true if we added (!vec_contains(v, t))
template <typename V, typename T>
inline bool vec_add(V &v, const T& t)
{
    bool adding = !vec_contains(v, t);
    if (adding)
        v.push_back(t);
    return adding;
}

template <typename V, typename V1>
inline bool vec_extend(V &v, const V1& t)
{
    foreach (auto &x, t) {
        v.push_back(x);
    }
    return t.size();
}

template <typename V, typename V1>
inline V vec_intersection(const V &v, const V1& t)
{
    V ret;
    foreach (auto &x, v) {
        if (vec_contains(t, x))
            vec_add(ret, x);
    }
    return ret;
}


template <typename Vec>
const typename Vec::value_type &vec_at(const Vec &v, int i,
                                       const typename Vec::value_type &def=dummy_ref<typename Vec::value_type>())
{
    return ((uint)i < v.size()) ? v[i] :
        ((uint)(-1-i) < v.size()) ? v[v.size() + i] : def;
}

template <typename T, size_t S>
const T &vec_at(const T (&v)[S], int i, const T &def=dummy_ref<T>())
{
    return ((int)i < S) ? v[i] :
        ((uint)(-1) < S) ? v[S + i] : def;
}

template <typename V> size_t vec_size(const V &v) { return v.size(); }
template <typename T, size_t S> size_t vec_size(const T (&v)[S]) { return S; }

template <typename Vec>
typename Vec::value_type &vec_index(Vec &v, size_t i)
{
    if (v.size() <= i) {
        v.resize(i + 1);
    }
    return v[i];
}

template <typename Vec>
void vec_set_index(Vec &v, size_t i, const typename Vec::value_type &val)
{
    if (v.size() <= i) {
        v.resize(i + 1);
    }
    v[i] = val;
}

template <typename T>
inline T *vec_get_ptr(std::vector<T*> &v, uint i)
{
    return (i < v.size()) ? v[i] : NULL;
}

// set v[i] = t, appending if i > v.size() and erasing if t == NULL
template <typename T>
inline void vec_set_index_deep(std::vector<T*> &v, uint i, T* t)
{
    if (i < v.size()) {
        if (v[i] == t)
            ;
        else if (t)
            v[i] = t;
        else
            v.erase(v.begin() + i);
    } else if (t)
        v.push_back(t);
}

// remove v[i], return true if i < v.size()
// swap from end to fill - does not maintain order!
template <typename V>
inline bool vec_pop(V &v, typename V::size_type i)
{
    if (i < v.size()) {
        std::swap(v[i], v.back());
        v.pop_back();
        return true;
    }
    return false;
}

// remove v[i], return true
// swap from end to fill - does not maintain order!
template <typename V>
inline bool vec_pop(V &v, typename V::iterator it)
{
    std::swap(*it, v.back());
    v.pop_back();
    return true;
}

// pop empty elements from back of vector (like str chomp)
template <typename V>
inline void vec_chomp(V &v)
{
    while (v.size() && !v.back())
        v.pop_back();
}

// remove v[i], return true if i < v.size()
// fill down - maintains order (but O(N))
template <typename V>
inline bool vec_erase(V &v, typename V::size_type i)
{
    if (i < v.size()) {
        v.erase(v.begin() + i);
        return true;
    }
    return false;
}

// remove v[i] if test, otherwise increment i. return test
template <typename V, typename I>
inline bool vec_pop_increment(V& v, I &idx, bool test)
{
    if (test) {
        return vec_pop(v, idx);
    } else {
        ++idx;
        return false;
    }
}

// remove v[i] if test, otherwise increment i. return test
template <typename V, typename I>
inline bool vec_pop_increment_deep(V& v, I &idx, bool test)
{
    if (test) {
        return vec_pop_deep(v, idx);
    } else {
        ++idx;
        return false;
    }
}

// remove v[i] if test, otherwise increment i. return test
template <typename V, typename I>
inline bool vec_erase_increment(V& v, I &idx, bool test)
{
    if (test) {
        return vec_erase(v, idx);
    } else {
        ++idx;
        return false;
    }
}

// remove first occurrence of t in v, return true if found
// swap from end to fill - does not maintain order!
template <typename V, typename T>
inline bool vec_remove_one(V &v, const T& t)
{
    for (auto &&x : v) {
        if (x == t) {
            std::swap(x, v.back());
            v.pop_back();
            return true;
        }
    }
    return false;
}

// remove and delete first occurrence of t in v, return true if found
// swap from end to fill - does not maintain order!
template <typename V, typename T>
inline bool vec_remove_one_deep(V &v, const T& t)
{
    auto end=std::end(v);
    for (auto it=std::begin(v); it != end;) {
        if (vec_pop_increment_deep(v, it, *it == t))
            return true;
    }
    return false;
}

// remove and delete v[i], return true if i < v.size()
// swap from end to fill - does not maintain order!
template <typename V>
inline bool vec_pop_deep(V &v, typename V::size_type i)
{
    ASSERT(i < v.size());
    if (i >= v.size())
        return false;
    auto elt = v[i];
    std::swap(v[i], v.back());
    v.pop_back();
    copy_delete(elt);       // destructor might add to this vector, so do this last!
    return true;
}

// remove and delete v[i], return true if i < v.size()
// swap from end to fill - does not maintain order!
template <typename V>
inline bool vec_pop_deep(V &v, typename V::iterator it)
{
    auto elt = *it;
    std::swap(*it, v.back());
    v.pop_back();
    copy_delete(elt);           // destructor might add to this vector, so do this last!
    return true;
}

// clear and deallocate storage for t
template <typename T>
inline void vec_deallocate(T &t)
{
    T().swap(t);
}

template <typename V>
inline void vec_swap(V& vec, size_t i, size_t j)
{
    std::swap(vec[i], vec[j]);
}

// delete all elements and clear v
template <typename V>
inline int vec_clear_deep(V &v)
{
    int count = 0;
    for (size_t i=0; i<v.size(); i++) {
        count += copy_delete(v[i]);
    }
    v.clear();
    return count;
}

template <typename V, typename F>
inline int vec_clear_deep(V &v, const F& fre)
{
    int count = 0;
    for (size_t i=0; i<v.size(); i++) {
        count += fre(v[i]);
    }
    v.clear();
    return count;
}

// resize v to size, deleting removed elements
template <typename T>
inline void vec_resize_deep(std::vector<T*> &v, typename std::vector<T*>::size_type size)
{
    for (size_t i=size; i<v.size(); i++) {
        copy_delete(v[i]);
    }
    v.resize(size, NULL);
}

// copy all elements from o to v, invoking assignment operator for existing elements and copy constructor for new elements
template <typename T, typename U>
inline void vec_copy_deep(std::vector<T*> &v, const std::vector<U*> &o)
{
    const size_t copy_sz = std::min(v.size(), o.size());
    for (size_t i=0; i<copy_sz; i++) {
        *v[i] = *o[i];
    }
    vec_resize_deep(v, o.size());
    for (size_t i=copy_sz; i<o.size(); i++) {
        v[i] = new T(*o[i]);
    }
}

// const version: clear v and fill using copy constructed elements from o
template <typename T, typename U>
inline void vec_copy_deep(std::vector<const T*> &v, const std::vector<U*> &o)
{
    vec_clear_deep(v);
    v.resize(o.size(), NULL);
    for (size_t i=0; i<o.size(); i++) {
        v[i] = new T(*o[i]);
    }
}

// sort entire vector using supplied comparator
template <typename T, typename F>
inline void vec_sort(T& col, const F& comp) { std::sort(std::begin(col), std::end(col), comp); }

template <typename T, typename F>
inline void vec_sort(const T& col, const F& comp) { std::sort(std::begin(col), std::end(col), comp); }

template <typename T>
inline void vec_sort(T& col) { std::sort(std::begin(col), std::end(col)); }

// sort entire collection, using supplied function to get comparison key
template <typename T, typename F>
inline void vec_sort_key(T& col, const F& gkey)
{
    std::sort(std::begin(col), std::end(col), [&](const typename T::value_type &a,
                                                  const typename T::value_type &b) {
                  return gkey(a) < gkey(b); });
}

// sort iterator, using supplied function to get comparison key
template <typename IT, typename F>
inline void vec_sort_key(IT beg, IT end, const F& gkey)
{
    typedef decltype(*beg) value_type;
    std::sort(beg, end, [&](const value_type &a,
                            const value_type &b) {
                  return gkey(a) < gkey(b); });
}

template <typename T, typename F>
inline T vec_sorted(T vec, const F& comp)
{
    std::sort(vec.begin(), vec.end(), comp);
    return vec;
}

template <typename T>
inline T vec_sorted(T vec)
{
    std::sort(vec.begin(), vec.end());
    return vec;
}

template <typename T, typename Fun>
void vec_unique(T& vec, Fun fun)
{
    vec.erase(std::unique(vec.begin(), vec.end(), fun), vec.end());
}

template <typename T, typename Fun>
void vec_unique_deep(T& vec, Fun fun)
{
    auto nend = std::unique(vec.begin(), vec.end(), fun);
    for (auto it=nend; it != vec.end(); ++it)
        copy_delete(*it);
    vec.erase(nend, vec.end());
}

template <typename T>
void vec_sort_unique(T& vec)
{
    vec_sort(vec);
    vec.erase(std::unique(vec.begin(), vec.end()), vec.end());
}

// selection sort the first COUNT elements of VEC, using FUN as a key for comparison
template <typename T, typename F>
inline void vec_selection_sort_key(T& vec, size_t count, const F& fun)
{
    const size_t vecSize = std::distance(std::begin(vec), std::end(vec));
    count = min(count, vecSize);
    for (uint i=0; i<count; i++)
    {
        float minV   = std::numeric_limits<float>::max();
        uint  minIdx = 0;
        for (uint j=i; j<vecSize; j++) {
            float v = fun(vec[j]);
            if (v < minV) {
                minIdx = j;
                minV   = v;
            }
        }
        std::swap(vec[i], vec[minIdx]);
    }
}

// selection sort the first COUNT elements of VEC, using FUN as a comparator
template <typename T, typename F>
inline void vec_selection_sort(T& vec, size_t count, const F& comp)
{
    const size_t vecSize = std::distance(std::begin(vec), std::end(vec));
    count = min(count, vecSize);
    if (count > 50)
        return vec_sort(vec, comp);
    for (uint i=0; i<count; i++) {
        for (uint j=i; j<vecSize; j++) {
            if (comp(vec[j], vec[i]))
                std::swap(vec[i], vec[j]);
        }
    }
}

template <typename T>
inline void vec_reverse(T &vec)
{
    std::reverse(std::begin(vec), std::end(vec));
}

template <typename T>
inline T vec_reversed(T vec)
{
    std::reverse(std::begin(vec), std::end(vec));
    return vec;
}

template <typename A, typename B>
inline auto vec_interleaved(const A& a, const B& b) -> std::vector<typename A::value_type>
{
    std::vector<typename A::value_type> ret;
    auto ait = std::begin(a);
    auto bit = std::begin(b);
    const auto aend = std::end(a);
    const auto bend = std::end(b);

    for (; ait != aend && bit != bend; ++ait, ++bit)
    {
        ret.push_back(*ait);
        ret.push_back(*bit);
    }

    for (; ait != aend; ++ait)
        ret.push_back(*ait);
    for (; bit != bend; ++bit)
        ret.push_back(*bit);
    
    return ret;
}

template <typename T>
inline typename T::value_type vec_max(const T &vec)
{
    typename T::value_type mx{};
    foreach (const auto &x, vec)
        mx = std::max(x, mx);
    return mx;
}

template <typename T>
inline typename T::value_type vec_min(const T &vec)
{
    typename T::value_type mn{};
    foreach (const auto &x, vec)
        mn = std::min(x, mn);
    return mn;
}

template <typename Vec,
          typename F,
          typename T=typename Vec::value_type,
          typename V=typename std::result_of<F(const T&)>::type>
T vec_max(const Vec& vec, const F& fun, const T& init=T())
{
    V maxv = std::numeric_limits<V>::lowest();
    T val  = init;
    foreach (const T &x, vec)
    {
        const V dist = fun(x);
        if (dist > maxv)
        {
            val = x;
            maxv = dist;
        }
    }
    return val;
}

template <typename Vec,
          typename F,
          typename T=typename Vec::value_type,
          typename V=typename std::result_of<F(const T&)>::type>
T vec_min(const Vec& vec, const F& fun, const T& init=T(),
          const V &start=std::numeric_limits<V>::max())
{
    V minv = start;
    T val  = init;
    foreach (const T &x, vec)
    {
        const V dist = fun(x);
        if (dist < minv)
        {
            val = x;
            minv = dist;
        }
    }
    return val;
}

template <typename T, typename F>
inline bool vec_any(const T& v, const F& f)
{
    foreach (auto &x, v) {
        if (f(x))
            return true;
    }
    return false;
}

template <typename T>
inline bool vec_any(const T& v)
{
    for_ (x, v) {
        if (x)
            return true;
    }
    return false;
}

template <typename T>
inline T vec_sum(const std::vector<T>& v)
{
    T r{};
    for_ (x, v) { r += x; }
    return r;
}

template <typename V, typename T=typename V::value_type, typename F>
inline T vec_foldl(const V& v, F fun, T init=T())
{
    for_ (x, v) {
        init = fun(init, x);
    }
    return init;
}


template <typename T, typename F>
inline bool vec_all(const T& v, const F& f)
{
    foreach (auto &x, v) {
        if (!f(x))
            return false;
    }
    return true;
}

template <typename T>
inline bool vec_all(const T& v)
{
    for_ (x, v) {
        if (!x)
            return false;
    }
    return true;
}

template <typename T>
inline int vec_next(const T& vec, int index, int delta)
{
    if (!vec_any(vec))
        return -1;
    index = modulo(index + delta, vec_size(vec));
    delta = (delta >= 0) ? 1 : -1;
    for (; !vec_at(vec, index); index = modulo(index + delta, vec_size(vec)));
    return index;
}

template <typename T, typename Fun>
inline auto vec_map(const T &inv, const Fun &fun) -> std::vector<decltype(fun(*std::begin(inv)))>
{
    std::vector<decltype(fun(*std::begin(inv)))> outs;
    foreach (const auto &x, inv)
    {
        outs.push_back(fun(x));
    }
    return outs;
}

template <typename R, typename T, typename Fun>
inline std::vector<R> vec_map(const T &inv, const Fun &fun)
{
    std::vector<R> outs;
    foreach (const auto &x, inv)
    {
        outs.push_back(fun(x));
    }
    return outs;
}

template <typename R, typename T>
inline std::vector<R> vec_map(const T &inv)
{
    std::vector<R> outs;
    foreach (const auto &x, inv)
    {
        outs.push_back(R(x));
    }
    return outs;
}

template <typename R, typename Vec, typename Fun>
inline R vec_foldl(const Vec &vec, const Fun & fun)
{
    R ret{};
    foreach (auto &x, vec)
    {
        ret = fun(ret, x);
    }
    return ret;
}

template <typename R, typename V, typename Fun>
inline R vec_average(const V& vec, Fun fun)
{
    R r;
    foreach (const auto& x, vec)
        r += fun(x);
    r /= R(vec.size());
    return r;
}

template <typename T, typename F>
inline int vec_pop_if(T &vec, F pred)
{
    int count = 0;
    for (uint i=0; i<vec.size();)
    {
        if (vec_pop_increment(vec, i, pred(vec[i])))
            count++;
    }
    return count;
}


template <typename T>
inline int vec_pop_if_not(T &vec)
{
    int count = 0;
    for (uint i=0; i<vec.size();)
    {
        if (vec_pop_increment(vec, i, !vec[i]))
            count++;
    }
    return count;
}


// swap from end to fill - does not maintain order!
template <typename Vec, typename T>
inline int vec_remove(Vec &v, const T &t)
{
    int count = 0;
    for (int i=0; i<v.size();) {
        if (vec_pop_increment(v, i, v[i] == t))
            count++;
    }
    return count;
}

template <typename T, typename F>
inline int vec_pop_if_deep(T &vec, F pred)
{
    int count = 0;
    for (uint i=0; i<vec.size();)
    {
        if (pred(vec[i])) {
            vec_pop_deep(vec, i);
            count++;
        } else
            i++;
    }
    return count;
}

template <typename T, typename F>
inline int vec_pop_unless(T &vec, F pred)
{
    int count = 0;
    for (uint i=0; i<vec.size();)
    {
        if (vec_pop_increment(vec, i, !pred(vec[i])))
            count++;
    }
    return count;
}

template <typename T, typename F>
inline int vec_pop_unless_deep(T &vec, F pred)
{
    int count = 0;
    for (uint i=0; i<vec.size();)
    {
        if (!pred(vec[i])) {
            vec_pop_deep(vec, i);
            count++;
        } else
            i++;
    }
    return count;
}


// map ///////////////////////////////////////////////////////////////////////////////
// FIXME make these work for undordered_map as well
// const T& map, const T::value_type& val, ...

template <typename M>
inline bool map_contains(const M &m, const typename M::key_type& key)
{
    return m.count(key) != 0;
}

template <typename M>
inline const typename M::mapped_type &map_get(const M &m, const typename M::key_type& key,
                                              const typename M::mapped_type& def=dummy_ref<typename M::mapped_type>())
{
    const typename M::const_iterator it = m.find(key);
    return (it != m.end()) ? it->second : def;
}

template <typename V>
inline const V &map_get(const std::map<std::string, V> &m, const char* key, const V& def=dummy_ref<V>())
{
    const typename std::map<std::string, V>::const_iterator it = m.find(key);
    return (it != m.end()) ? it->second : def;
}

template <typename M>
inline const typename M::mapped_type *map_addr(const M &m, const typename M::key_type& key)
{
    const typename M::const_iterator it = m.find(key);
    return (it != m.end()) ? &it->second : NULL;
}

template <typename K, typename V>
inline V &map_set(std::map<K, V> &m, const K& key, const V& val)
{
    V &v = m[key];
    v = val;
    return v;
}

template <typename M, typename K, typename V>
inline typename M::mapped_type &map_setdefault(M &m, const K& key, const V& def=dummy_ref<V>())
{
    typename M::iterator it = m.find(key);
    if (it != m.end())
        return it->second;
    it = m.insert(make_pair(key, def)).first;
    return it->second;
}
    
template <typename V>
inline V &map_setdefault(std::map<std::string, V> &m, const char *key, const V& def=dummy_ref<V>())
{
    typename std::map<std::string, V>::iterator it = m.find(key);
    if (it != m.end())
        return it->second;
    it = m.insert(make_pair(key, def)).first;
    return it->second;
}

#define MAP_SETDEFAULT(M, K, D) (map_contains((M), (K)) ? (M)[(K)] : map_set((M), (K), (D)))

template <typename M, typename K, typename V>
inline void map_keepcount(M& map, K ident, V change)
{
    if (change > 0) {
        map_setdefault(map, ident, V(0)) += change;
    } else {
        typename M::iterator it = map.find(ident);
        if (it != map.end()) {
            if (it->second + change <= V(0))
                map.erase(it);
            else
                it->second += change;
        }
    }
}

// delete all values and clear v
template <typename T>
inline void map_clear_deep(T &v)
{
    foreach (auto &x, v) {
        copy_delete(x.second);
    }
    v.clear();
}

template <typename K, typename V>
inline bool multimap_contains(const std::multimap<K, V> &map, const typename std::multimap<K, V>::value_type &x)
{
    const typename std::multimap<K, V>::const_iterator end = map.upper_bound(x.first);
    return std::find(map.lower_bound(x.first), end, x) != end;
}

template <typename K, typename V>
inline bool multimap_add(std::multimap<K, V> &map, const typename std::multimap<K, V>::value_type &x)
{
    bool adding = !multimap_contains(map, x);
    if (adding) {
        map.insert(x);
    }
    return adding;
}

template <typename K, typename V>
inline int multimap_remove(std::multimap<K, V> &map, const typename std::multimap<K, V>::value_type &x)
{
    int remove_count = 0;
    typename std::multimap<K, V>::iterator it=map.lower_bound(x.first), end=map.upper_bound(x.first);
    for (;;) {
        it = std::find(it, end, x);
        if (it == end) {
            return remove_count;
        }

        remove_count++;
        it = map.erase(it);
    }
    return 0;
}

template <typename M, typename O>
inline void copy_key(M mb, M me, O o)
{
    for (; mb != me; mb++, o++) {
        *o = mb->first;
    }
}

template <typename M, typename O>
inline void copy_value(M mb, M me, O o)
{
    for (; mb != me; mb++, o++) {
        *o = mb->second;
    }
}

// set ///////////////////////////////////////////////////////////////////////////////


template <typename S>
inline bool set_contains(const S &m, const typename S::value_type& key)
{
    return m.count(key) != 0;
}

template <typename K>
inline const K &set_set(std::set<K> &m, const K& key)
{
    return *m.insert(key).first;
}

template <typename V, typename V1>
inline bool set_extend(V &v, const V1& t)
{
    foreach (auto &x, t) {
        v.insert(x);
    }
    return t.size();
}

template <typename R, typename T, typename Fun>
inline std::set<R> set_map(const T &inv, Fun fun)
{
    std::set<R> outs;
    for (typename T::const_iterator it=inv.begin(), end=inv.end(); it != end; ++it)
    {
        outs.insert(fun(*it));
    }
    return outs;
}

template <typename R, typename T, typename Fun>
inline R cont_map(const T &inv, Fun fun)
{
    R outs;
    for (typename T::const_iterator it=inv.begin(), end=inv.end(); it != end; ++it)
    {
        outs.insert(fun(*it));
    }
    return outs;
}

////// slist ////////////////////////////////////////////////////////////////////////

template <typename T>
inline T* slist_insert(T* list, T* node)
{
    node->next = list;
    return node;
}

template <typename T>
inline T *slist_clear(T *node)
{
    while (node != NULL)
    {
        T *next = node->next;
        copy_delete(node);
        node = next;
    }
    return NULL;
}

// adapter to use intrusive linked lists in foreach loops
template <typename T>
struct slist_iter_t {
    typedef T value_type;
    T *ptr;
    slist_iter_t(T* p=NULL) : ptr(p) {}
    slist_iter_t &operator++() {
        ptr = ptr->next;
        return *this;
    }
    slist_iter_t operator++(int) {
        slist_iter_t self = *this;
        ptr = ptr->next;
        return self;
    }
    T* operator*() { return ptr; }
    const T* operator*() const { return ptr; }
    bool operator=(slist_iter_t o) const { return o.ptr == ptr; }
    bool operator!=(slist_iter_t o) const { return o.ptr != ptr; }
    slist_iter_t begin() const { return *this; }
    slist_iter_t end() const { return slist_iter_t(); }
};

template <typename T>
slist_iter_t<T> slist_iter(T *pt=NULL)
{
    return slist_iter_t<T>(pt);
}



////////////////////////////////////////////////////////////////////////////////////

typedef std::map<uint64, const char*> OL_ThreadNames;
OL_ThreadNames &_thread_name_map();
std::mutex& _thread_name_mutex();

// set thread name, random seed, terminate handler, etc.
void thread_setup(const char* name);
void thread_cleanup();

#if OL_USE_PTHREADS
typedef pthread_t OL_Thread;
#define THREAD_IS_SELF(B) (pthread_self() == (B))
#define THREAD_ALIVE(B) (B)
#else
typedef std::thread* OL_Thread;
#define THREAD_IS_SELF(B) ((B) && std::this_thread::get_id() == (B)->get_id())
#define THREAD_ALIVE(B) ((B) && (B)->joinable())
#endif

OL_Thread thread_create(void *(*start_routine)(void *), void *arg);
void thread_join(OL_Thread thread);
const char* thread_current_name();

// adapted from boost::reverse_lock
template<typename Lock>
class reverse_lock
{
public:
    typedef typename Lock::mutex_type mutex_type;
    reverse_lock& operator=(const reverse_lock &) = delete;
	
    explicit reverse_lock(Lock& m_) : m(m_), mtx(0)
    {
        if (m.owns_lock())
        {
            m.unlock();
        }
        mtx=m.release();
    }
    ~reverse_lock()
    {
        if (mtx) {
            mtx->lock();
            m = std::move(Lock(*mtx, std::adopt_lock));
        }
    }
	
private:
    Lock& m;
    mutex_type* mtx;
};

// http://stackoverflow.com/questions/21892934/how-to-assert-if-a-stdmutex-is-locked
#if DEBUG

struct rmutex : public std::recursive_mutex
{
    void lock()
    {
        std::recursive_mutex::lock();
        if (m_count++ == 0)
            m_holder = std::this_thread::get_id();
    }

    void unlock()
    {
        if (--m_count == 0)
            m_holder = std::thread::id();
        std::recursive_mutex::unlock();
    }

    bool is_locked() const
    {
        return m_holder != std::thread::id();
    }

    bool is_mine() const
    {
        return m_holder == std::this_thread::get_id();
    }

private:
    std::thread::id m_holder;
    int             m_count = 0;
};

#else
typedef std::recursive_mutex rmutex;
#endif

#define ASSERT_LOCKED(M) DASSERT((M).is_mine())


// pooled memory allocator
class MemoryPool final {

    struct Chunk final {
        Chunk *next;
    };

    std::mutex    mutex;
    const size_t  element_size;
    size_t        count = 0;
    size_t        used  = 0;
    char         *pool  = NULL;
    Chunk        *first = NULL;
    MemoryPool   *next  = NULL; // next pool
    int           index = 0;    // index in pool chain

public:

    MemoryPool(size_t sz) : element_size(sz) {}
    ~MemoryPool();

    // allocate a pool containing CNT elements
    size_t create(size_t cnt);

    size_t getCount() const { return count; }
    size_t getUsed() const { return used + (next ? next->getUsed() : 0); }
    size_t getElementSize() const { return element_size; }

    template <typename T>
    T* begin()
    {
        ASSERT(sizeof(T) == element_size);
        return (T*)pool;
    }

    template <typename T>
    T* end()
    {
        ASSERT(sizeof(T) == element_size);
        return (T*)(pool + count * element_size);
    }

    bool isInPool(const void *pt) const;

    void* allocate();           // return a block of element_size bytes
    void deallocate(void *ptr); // free a block returned by allocate()


    friend void* operator new(size_t nbytes, MemoryPool& mp)
    {
        ASSERT(nbytes == mp.getElementSize());
        return mp.allocate();
    }

    friend void operator delete(void* p, MemoryPool& mp)
    {
        mp.deallocate(p);
    }
    
};

template <typename T>
size_t SizeOf(const T& val) { return sizeof(T); }

template <typename T>
size_t SizeOf(const T* ptr) { return ptr? SizeOf(*ptr) : 0; }

template <typename T>
size_t SizeOf(const std::vector<T> &vec) { return SizeOf(vec[0]) * vec.capacity(); }

template <typename U, typename T>
bool instanceof(const U* ptr)
{
    return dynamic_cast<T*>(ptr) != NULL;
}

template <typename T>
bool test_min(const T &dist, T *mindist)
{
    if (dist >= *mindist)
        return false;
    *mindist = dist;
    return true;
}

template <typename T>
bool test_max(const T &dist, T *maxdist)
{
    if (dist <= *maxdist)
        return false;
    *maxdist = dist;
    return true;
}

#endif
