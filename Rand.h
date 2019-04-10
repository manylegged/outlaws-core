
//
// Rand.h - random number generation and related utility functions
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

#ifndef RAND_H
#define RAND_H

#include <random>

const char* thread_current_name();
// #define DEBUG_RAND(X) Reportf X
#define DEBUG_RAND(X)

int& random_seed();

std::mt19937 *&my_random_device();

template <typename T>
inline T randrange()
{
    std::uniform_int_distribution<T> uniform_dist(std::numeric_limits<T>::min(), 
                                                  std::numeric_limits<T>::max());
    T result = uniform_dist(*my_random_device());
    DEBUG_RAND(("[%s] randrange<%s> %d", thread_current_name(), TYPE_NAME(T), result));
    return result;
}

// return a random number from [start-end)
inline int randrange(int start, int end)
{
    // ASSERT(start <= end);
    if (start >= end-1)
        return start;
    std::uniform_int_distribution<int> uniform_dist(start, end-1);
    int result = uniform_dist(*my_random_device());
    DEBUG_RAND(("[%s] randrange(%d, %d) %d", thread_current_name(), start, end, result));
    return result;
}

// return a random number from [0-end)
inline int randrange(int end)
{
    return randrange(0, end);
}

inline int2 randrange(int2 start, int2 end)
{
    return int2(randrange(start.x, end.x), randrange(start.y, end.y));
}

inline int3 randrange(int3 start, int3 end)
{
    return int3(randrange(start.x, end.x), randrange(start.y, end.y), randrange(start.z, end.z));
}

inline bool randbool() { return randrange(0, 2); }

inline float randsign() { return randbool() ? 1.f : -1.f; }

template <typename T>
const T &randselect(std::initializer_list<T> l)
{
    return *(l.begin() + randrange(0, l.size()));
}

template <typename Vec>
auto randselect(const Vec& vec) -> decltype(*std::begin(vec))
{
    const size_t size = std::distance(std::begin(vec), std::end(vec));
    if (size == 0)
        return dummy_ref<decltype(*std::begin(vec))>();
    return *(std::begin(vec) + randrange(0, size));
}

template <typename Vec>
typename Vec::value_type randselect_pop(Vec& vec)
{
    if (vec.empty())
        return dummy_ref<typename Vec::value_type>();
    const int index = randrange(0, vec.size());
    typename Vec::value_type val = std::move(vec[index]);
    vec_pop(vec, index);
    return val;
}

inline float randrange(float start, float end)
{
    // ASSERT(start <= end);
    if (start == end)
        return start;
    if (start > end)
        swap(start, end);
    std::uniform_real_distribution<float> uniform_dist(start, end);
    float result = uniform_dist(*my_random_device());
    DEBUG_RAND(("[%s] randrange(%g, %g) %g", thread_current_name(), start, end, result));
    return result;
}

inline float2 randrange(float2 start, float2 end)
{
    return float2(randrange(start.x, end.x), randrange(start.y, end.y));
}

inline float3 randrange(float3 start, float3 end)
{
    return float3(randrange(start.x, end.x), randrange(start.y, end.y), randrange(start.z, end.z));
}

inline float randnorm() { return randrange(0.f, 1.f); }
inline float randangle() { return randrange(0.f, M_TAUf); }
inline float2 randpolar(float minradius, float maxradius)
{
    return randrange(minradius, maxradius) * angleToVector(randangle());
}
inline float2 randpolar(float radius) 
{
    return radius * angleToVector(randangle());
}
inline float2 randpolar()
{
    return angleToVector(randangle());
}

// randomized vector with length (minradius, maxradius], uniformly distributed throughout area
float2 randpolar_uniform(float minradius, float maxradius) ;


inline bool chance(float v) { return randrange(0.f, 1.f) < v; }

float rand_normal(float mean, float stddev);

// return a random integer from [0-MAXV)
// (truncated) normal distribution, with a mean of 0 and STDDEV (lower numbers more likely)
inline int rand_positive_unormal(uint maxv, float stddev)
{
    ASSERT(maxv > 0);
    ASSERT(stddev > 0.f);
    return clamp(int(floor(fabsf(rand_normal(0, stddev)))), 0, maxv-1);
}

inline float randrange_normal(float mn, float mx)
{
    return clamp(rand_normal(0.5f * (mn + mx), 0.25 * (mx-mn)), mn, mx);
}


inline int myrandom_(int mx) { return randrange(mx); }

template <typename V>
inline void vec_shuffle(V& vec)
{
    std::random_shuffle(std::begin(vec), std::end(vec), myrandom_);
}

template <typename T>
inline T randlerp(T a, T b)
{
    return lerp(a, b, randrange(0.f, 1.f));
}
    

#endif // RAND_H
