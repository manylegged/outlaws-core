
//
// stl_ext.h - general purpose standard library type stuff.
// - smart pointers
// - stl container convenience wrappers
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

#ifndef STL_EXT_H
#define STL_EXT_H

#include <vector>
#include <map>
#include <set>
#include <algorithm>
#include <type_traits>

#if _MSC_VER == 1600
// vs2010 specific syntax
#define foreach(A, B) for each (A in (B))
#else
// c++11 syntax
#define foreach(A, B) for (A : (B))
#endif

#define unless(X) if (!(X))

// lambda((int* a), return *a + 3)
#define lambda(X, Y) [&] X { return (Y); }

// return bit index of leading 1 (0x80000000 == 0x80030100 == 31, 0x1 == 0, 0x0 == -1)
inline int findLeadingOne(uint v)
{
    int i=0;
    if (v&0xffff0000) { i += 16; v >>= 16; }
    if (v&0xff00)     { i +=  8; v >>=  8; }
    if (v&0xf0)       { i +=  4; v >>=  4; }
    if (v&0xc)        { i +=  2; v >>=  2; }
    if (v&0x2)        { return i + 1;      }
    if (v&0x1)        { return i;          }
    return -1;
}

inline bool isPow2(int x)
{ 
    return (x&(x-1)) == 0; 
}


template <typename T>
static void deleteNull(T*& v)
{
    delete v;
    v = NULL;
}


template <typename T>
std::unique_ptr<T> make_unique(T* val)
{
    std::unique_ptr<T> ptr(val);
    return ptr;
}

namespace std {

    template <typename K, typename V>
    struct hash< std::pair<K, V> > {
        std::size_t operator()(const std::pair<K, V>& pt) const
        {
            return std::hash<K>()(pt.first) ^ (std::hash<V>()(pt.second) << 1);
        }
    };

    template <typename T>
    struct hash< glm::detail::tvec2<T> > {
        std::size_t operator()(const glm::detail::tvec2<T>& pt) const
        {
            return std::hash<T>()(pt.x) ^ (std::hash<T>()(pt.y) << 1);
        }
    };

    template <typename T>
    struct hash< glm::detail::tvec3<T> > {
        std::size_t operator()(const glm::detail::tvec2<T>& pt) const
        {
            return std::hash<T>()(pt.x) ^ (std::hash<T>()(pt.y) << 1) ^ (std::hash<T>()(pt.z) << 2);
        }
    };
}

// smart pointer supporting distributed and centralized ownership
// if explicitOwner field is set, share the pointer, never delete
// otherwise copy the object around and delete on destruction
template <typename T>
class copy_ptr {
    
    T* m_ptr = NULL;

    template <typename U>
    void assign1(U* v, typename std::enable_if<!std::is_const<U>::value>::type* = 0)
    {
        if (m_ptr && !m_ptr->explicitOwner && v && !v->explicitOwner)
            *m_ptr = *v;
        else
            reset((v && v->explicitOwner) ? v : v ? new T(*v) : NULL);
    }

    template <typename U>
    void assign1(U* v, typename std::enable_if<std::is_const<U>::value>::type* = 0)
    {
        reset((v && v->explicitOwner) ? v : v ? new T(*v) : NULL);
    }
    
public:

    ~copy_ptr() { reset(); }

    // default construct to null
    copy_ptr() { }
    copy_ptr(std::nullptr_t) { }
    //copy_ptr(int) { }

    // take ownership
    explicit copy_ptr(T* &&t) noexcept : m_ptr(t) { t = NULL; }
    copy_ptr& operator=(T* &&t) noexcept { reset(t); t = NULL; return *this; }
    copy_ptr& reset(T* v=NULL)
    {
        if (m_ptr == v)
            return *this;
        if (m_ptr && !m_ptr->explicitOwner) {
            delete m_ptr;
        }
        m_ptr = v;
        return *this;
    }

    // copy data when assigning unless data has explicit owner
    copy_ptr(const copy_ptr& other) { assign(other.m_ptr); }
    copy_ptr(copy_ptr&& other) noexcept : m_ptr(other.m_ptr) { other.m_ptr = NULL; }
    copy_ptr& operator=(copy_ptr&& other) noexcept { std::swap(m_ptr, other.m_ptr); return *this; }
    copy_ptr& operator=(const copy_ptr& other) { return assign(other.m_ptr); }
    copy_ptr& assign(T* v)
    {
        if (m_ptr == v)
            return *this;
        else
            this->assign1(v);
        return *this;
    }
    
    // forward assignment from pointee type - copy
    copy_ptr& operator=(const T& other)
    {
        ASSERT(!other.explicitOwner); // technically fine, but we should be setting by pointer
        if (m_ptr)
            *m_ptr = other;
        else
            m_ptr = new T(other);
        return *this;
    }

    // forward assignment from pointee type - move
    copy_ptr& operator=(T&& other) noexcept
    {
        ASSERT(!other.explicitOwner); // technically fine, but we should be setting by pointer
        if (m_ptr)
            *m_ptr = std::move(other);
        else
            m_ptr = new T(std::move(other));
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

    watch_ptr_base() : ptr(NULL), next(NULL), prev(NULL ) {}
    
    const Watchable* ptr;
    watch_ptr_base*  next;
    watch_ptr_base*  prev;
};

struct Watchable {

    mutable watch_ptr_base watch_list;

    virtual ~Watchable()
    {
        for (watch_ptr_base* watcher=watch_list.next; watcher != NULL; watcher = watcher->next)
        {
            watcher->ptr = NULL;
            if (watcher->prev)
                watcher->prev->next = NULL;
            watcher->prev = NULL;
        }
    }
};

// smart pointer that automatically becomes NULL when it's pointee is deleted
template <typename T>
class watch_ptr : public watch_ptr_base {

    void unlink()
    {
        if (next)
            next->prev = prev;
        if (prev)
            prev->next = next;
        next = NULL;
        prev = NULL;

        ptr  = NULL;
    }

    void link(const Watchable* p)
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

public:
    ~watch_ptr() 
    {
        unlink();
    }

    // default construct to null
    watch_ptr() { }
    watch_ptr(std::nullptr_t) { }

    explicit watch_ptr(T *t) { link(t); }
    watch_ptr(const watch_ptr &t) { link(t.ptr); }
    template <typename U>
    watch_ptr(const watch_ptr<U> &t) { link(t.ptr); }

    watch_ptr& operator=(T* t)
    {
        unlink();
        link(t);
        return *this;
    }

    watch_ptr& operator=(const watch_ptr& other) 
    {
        unlink();
        link(other.ptr);
        return *this;
    }
     
    // it's a smart pointer
    T& operator*()  const { return *get(); }
    T* operator->() const { return get(); }
    bool operator==(const watch_ptr &o) const { return ptr == o.ptr; }
    bool operator<(const watch_ptr &o)  const { return ptr < o.ptr; }
    explicit operator bool()            const { return ptr != NULL; }

    T*  get() const { return (T*) ptr; }
};

template <typename T>
watch_ptr<T> make_watch_compare(const T* v)
{
    watch_ptr<T> p;
    p.ptr = v;
    return p;
}

template <typename T>
watch_ptr<T> make_watch(T* v)
{
    watch_ptr<T> p(v);
    return p;
}


// vector ///////////////////////////////////////////////////////////////////////////////

template <typename V, typename T>
inline bool vector_contains(const V &v, const T& t)
{
    auto end = std::end(v);
    return std::find(std::begin(v), end, t) != end;
}

template <typename V, typename T>
inline int vector_count(const V &v, const T& t)
{
    int count = 0;
    foreach (const auto &x, v) {
        if (x == t)
            count++;
    }
    return count;
}

template <typename V, typename T>
inline size_t vector_find_index(const V &v, const T& t)
{
    auto begin = std::begin(v);
    auto end = std::end(v);
    auto it = std::find(begin, end, t);
    if (it == end)
        return -1;
    else
        return it - begin;
}

template <typename V, typename Fun>
const typename V::value_type &vector_find(
    const V &vec, const Fun& fun, const typename V::value_type &def=typename V::value_type())
{
    foreach (auto& x, vec) {
        if (fun(x))
            return x;
    }
    return def;
}

// return true if we added (!vector_contains(v, t))
template <typename V, typename T>
inline bool vector_add(V &v, const T& t)
{
    bool adding = !vector_contains(v, t);
    if (adding)
        v.push_back(t);
    return adding;
}

template <typename T>
inline const T &vector_get_index(std::vector<T> &v, uint i, const T &def=T())
{
    return (i < v.size()) ? v[i] : def;
}

template <typename T>
inline T *vector_get_ptr(std::vector<T*> &v, uint i)
{
    return (i < v.size()) ? v[i] : NULL;
}

// set v[i] = t, appending if i > v.size() and erasing if t == NULL
template <typename T>
inline void vector_set_index_deep(std::vector<T*> &v, uint i, T* t)
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

// remove first occurrence of t in v, return true if found
// swap from end to fill - does not maintain order!
template <typename V, typename T>
inline bool vector_remove_one(V &v, const T& t)
{
    typename V::iterator it = std::find(std::begin(v), std::end(v), t);
    if (it != std::end(v)) {
        std::swap(*it, v.back());
        v.pop_back();
        return true;
    }
    return false;
}

// remove and delete first occurrence of t in v, return true if found
// swap from end to fill - does not maintain order!
template <typename V, typename T>
inline bool vector_remove_one_deep(V &v, const T& t)
{
    typename V::iterator it = std::find(std::begin(v), std::end(v), t);
    if (it != std::end(v)) {
        std::swap(*it, v.back());
        delete v.back();
        v.pop_back();
        return true;
    }
    return false;
}

// remove v[i], return true if i < v.size()
// swap from end to fill - does not maintain order!
template <typename V>
inline bool vector_remove_index(V &v, typename V::size_type i)
{
    if (i < v.size()) {
        std::swap(v[i], v.back());
        v.pop_back();
        return true;
    }
    return false;
}

// remove and delete v[i], return true if i < v.size()
// swap from end to fill - does not maintain order!
template <typename V>
inline bool vector_remove_index_deep(V &v, typename V::size_type i)
{
    ASSERT(i < v.size());
    if (i < v.size()) {
        auto elt = v[i];
        std::swap(v[i], v.back());
        v.pop_back();
        delete elt;             // destructor might add to this vector, so do this last!
        return true;
    }
    return false;
}

// clear and deallocate storage for t
template <typename T>
inline void vector_deallocate(T &t)
{
    T().swap(t);
}

// delete all elements and clear v
template <typename V>
inline void vector_clear_deep(V &v)
{
    for (size_t i=0; i<v.size(); i++) {
        delete v[i];
    }
    v.clear();
}

// resize v to size, deleting removed elements
template <typename T>
inline void vector_resize_deep(std::vector<T*> &v, typename std::vector<T*>::size_type size)
{
    for (size_t i=size; i<v.size(); i++) {
        delete v[i];
    }
    v.resize(size, NULL);
}

// copy all elements from o to v, invoking assignment operator for existing elements and copy constructor for new elements
template <typename T, typename U>
inline void vector_copy_deep(std::vector<T*> &v, const std::vector<U*> &o)
{
    const size_t copy_sz = std::min(v.size(), o.size());
    for (size_t i=0; i<copy_sz; i++) {
        *v[i] = *o[i];
    }
    vector_resize_deep(v, o.size());
    for (size_t i=copy_sz; i<o.size(); i++) {
        v[i] = new T(*o[i]);
    }
}

// const version: clear v and fill using copy constructed elements from o
template <typename T, typename U>
inline void vector_copy_deep(std::vector<const T*> &v, const std::vector<U*> &o)
{
    vector_clear_deep(v);
    v.resize(o.size(), NULL);
    for (size_t i=0; i<o.size(); i++) {
        v[i] = new T(*o[i]);
    }
}

template <typename K>
struct key_comparator {
    K m_key;
    key_comparator(K k): m_key(k) {}
    template <typename T>
    bool operator()(const T& a, const T& b) { return m_key(a) < m_key(b); }
};

template <typename K>
key_comparator<K> make_key_comparator(K k) { return key_comparator<K>(k); }

// sort entire vector using supplied comparator
template <typename T, typename F>
inline void vector_sort(T& col, const F& comp) { std::sort(col.begin(), col.end(), comp); }

// sort entire vector, using supplied function to get comparison key
template <typename T, typename F>
inline void vector_sort_key(T& col, const F& gkey) { std::sort(col.begin(), col.end(), make_key_comparator(gkey)); }

// selection sort the first COUNT elements of VEC, using FUN as a key for comparison
template <typename T, typename F>
inline void vector_selection_sort_key(T& vec, size_t count, const F& fun)
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
inline void vector_selection_sort(T& vec, size_t count, const F& comp)
{
    const size_t vecSize = std::distance(std::begin(vec), std::end(vec));
    count = min(count, vecSize);
    for (uint i=0; i<count; i++) {
        for (uint j=i; j<vecSize; j++) {
            if (comp(vec[j], vec[i]))
                std::swap(vec[i], vec[j]);
        }
    }
}

// return minimum element of vector, with comparison by fkey
template <typename T, typename F>
inline size_t vector_min_idx_by_key(T& col, const F& fkey)
{
    float minV   = std::numeric_limits<float>::max();
    size_t  minIdx = 0;
    for (size_t i=0; i<col.size(); i++) {
        float v = fkey(col[i]);
        if (v < minV) {
            minIdx = i;
            minV = v;
        }
    }
    return minIdx;
}

template <typename T, typename F>
inline typename T::value_type &vector_min_element_by_key(T& col, const F& fkey)
{
    size_t minIdx = vector_min_element_key_idx(col, fkey);
    return col[minIdx];
}

template <typename T, typename F>
inline size_t vector_max_idx_by_key(T& col, const F& fkey)
{
    float maxV   = std::numeric_limits<float>::min();
    size_t  maxIdx = 0;
    for (size_t i=0; i<col.size(); i++) {
        float v = fkey(col[i]);
        if (v > maxV) {
            maxIdx = i;
            maxV = v;
        }
    }
    return maxIdx;
}

template <typename T, typename F>
inline typename T::value_type &vector_max_element_by_key(T& col, const F& fkey)
{
    size_t maxIdx = vector_max_idx_by_key(col, fkey);
    return col[maxIdx];
}

template <typename T, typename F>
inline bool vector_any(const T& v, const F& f)
{
    foreach (auto &x, v) {
        if (f(x))
            return true;
    }
    return false;
}

template <typename T, typename F>
inline bool vector_all(const T& v, const F& f)
{
    foreach (auto &x, v) {
        if (!f(x))
            return false;
    }
    return true;
}


template <typename R, typename T, typename Fun>
inline std::vector<R> vector_map(const T &inv, Fun fun)
{
    std::vector<R> outs;
    for (typename T::const_iterator it=inv.begin(), end=inv.end(); it != end; ++it)
    {
        outs.push_back(fun(*it));
    }
    return outs;
}

template <typename R, typename V, typename Fun>
inline R vector_average(const V& vec, Fun fun)
{
    R r;
    foreach (const auto& x, vec)
        r += fun(x);
    r /= R(vec.size());
    return r;
}

template <typename T, typename F>
inline int vector_remove_if(T &vec, F pred)
{
    int count = 0;
    for (uint i=0; i<vec.size();)
    {
        if (pred(vec[i])) {
            vector_remove_index(vec, i);
            count++;
        } else
            i++;
    }
    return count;
}


template <typename T>
inline int vector_remove_if_not(T &vec)
{
    int count = 0;
    for (uint i=0; i<vec.size();)
    {
        if (!vec[i]) {
            vector_remove_index(vec, i);
            count++;
        } else
            i++;
    }
    return count;
}



// swap from end to fill - does not maintain order!
template <typename V, typename T>
inline int vector_remove(V &v, const T& t)
{
    return vector_remove_if(v, lambda((const T& e), e == t));
}

template <typename T, typename F>
inline int vector_remove_if_deep(T &vec, F pred)
{
    int count = 0;
    for (uint i=0; i<vec.size();)
    {
        if (pred(vec[i])) {
            vector_remove_index_deep(vec, i);
            count++;
        } else
            i++;
    }
    return count;
}

template <typename T, typename F>
inline int vector_remove_unless(T &vec, F pred)
{
    int count = 0;
    for (uint i=0; i<vec.size();)
    {
        if (!pred(vec[i])) {
            vector_remove_index(vec, i);
            count++;
        } else
            i++;
    }
    return count;
}

template <typename T, typename F>
inline int vector_remove_unless_deep(T &vec, F pred)
{
    int count = 0;
    for (uint i=0; i<vec.size();)
    {
        if (!pred(vec[i])) {
            vector_remove_index_deep(vec, i);
            count++;
        } else
            i++;
    }
    return count;
}


// map ///////////////////////////////////////////////////////////////////////////////
// FIXME make these work for undordered_map as well
// const T& map, const T::value_type& val, ...

template <typename K, typename V>
inline bool map_contains(const std::map<K, V> &m, const K& key)
{
    return m.count(key) != 0;
}

template <typename M>
const typename M::mapped_type &map_get(const M &m, const typename M::key_type& key,
                                       const typename M::mapped_type& def=typename M::mapped_type())
{
    const typename M::const_iterator it = m.find(key);
    return (it != m.end()) ? it->second : def;
}

template <typename V>
inline const V &map_get(const std::map<std::string, V> &m, const char* key, const V& def=V())
{
    const typename std::map<std::string, V>::const_iterator it = m.find(key);
    return (it != m.end()) ? it->second : def;
}

template <typename K, typename V>
inline const V *map_get_addr(const std::map<K, V> &m, const K& key)
{
    const typename std::map<K, V>::const_iterator it = m.find(key);
    return (it != m.end()) ? &it->second : NULL;
}


template <typename T, typename K>
inline auto map_get_ptr(const T &m, const K& key) -> const typename T::mapped_type
{
    const typename T::const_iterator it = m.find(key);
    return (it != m.end()) ? it->second : NULL;
}

template <typename V>
inline const V *map_get_ptr(const std::map<std::string, V*> &m, const char*key)
{
    const typename std::map<std::string, V*>::const_iterator it = m.find(key);
    return (it != m.end()) ? it->second : NULL;
}

template <typename K, typename V>
inline V *map_get_ptr(std::map<K, V*> &m, const K& key)
{
    typename std::map<K, V*>::const_iterator it = m.find(key);
    return (it != m.end()) ? it->second : NULL;
}

template <typename V>
inline V *map_get_ptr(std::map<std::string, V*> &m, const char*key)
{
    typename std::map<std::string, V*>::const_iterator it = m.find(key);
    return (it != m.end()) ? it->second : NULL;
}

template <typename V>
inline const V *map_get_addr(const std::map<std::string, V> &m, const char* key)
{
    const typename std::map<std::string, V>::const_iterator it = m.find(key);
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
inline typename M::mapped_type &map_setdefault(M &m, const K& key, const V& def=V())
{
    typename M::iterator it = m.find(key);
    if (it != m.end())
        return it->second;
    it = m.insert(make_pair(key, def)).first;
    return it->second;
}
    
template <typename V>
inline V &map_setdefault(std::map<std::string, V> &m, const char *key, const V& def=V())
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


template <typename K>
inline bool set_contains(const std::set<K> &m, const K& key)
{
    return m.count(key) != 0;
}

template <typename K>
inline const K &set_set(std::set<K> &m, const K& key)
{
    return *m.insert(key).first;
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
inline void slist_clear(T *node)
{
    while (node != NULL)
    {
        T *next = node->next;
        delete node;
        node = next;
    }
}

////////////////////////////////////////////////////////////////////////////////////


#endif