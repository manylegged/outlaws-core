
//
// Rand.h - random number generation and related utility functions
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

#ifndef RAND_H
#define RAND_H

inline std::default_random_engine &random_device()
{
    static std::random_device rd;
    static std::default_random_engine e1(rd());
    return e1;
}

template <typename T>
inline uint randrange()
{
    std::uniform_int_distribution<T> uniform_dist(std::numeric_limits<T>::min(), 
                                                  std::numeric_limits<T>::max());
    return uniform_dist(random_device());
}

// return a random number from [start-end)
inline int randrange(int start, int end)
{
    ASSERT(start < end);
    std::uniform_int_distribution<int> uniform_dist(start, end-1);
    return uniform_dist(random_device());
    //return start + (rand() % (end - start));
}

inline bool randbool() { return randrange(0, 2); }

template <typename T>
inline const T &randselect(std::initializer_list<T> l)
{
    return *(l.begin() + randrange(0, l.size()));
}

template <typename Vec>
inline typename Vec::value_type& randselect(Vec& vec)
{
    ASSERT(vec.size());
    if (vec.size() == 1)
        return vec[0];
    return vec[randrange(0, vec.size())];
}

template <typename Vec>
inline const typename Vec::value_type& randselect(const Vec& vec)
{
    ASSERT(vec.size());
    if (vec.size() == 1)
        return vec[0];
    return vec[randrange(0, vec.size())];
}


inline float randrange(float start, float end)
{
    ASSERT(start <= end);
    std::uniform_real_distribution<float> uniform_dist(start, end);
    return uniform_dist(random_device());
    //return float(start + ((end - start) * float(rand())) / float(RAND_MAX));
}

inline float2 randrange(float2 start, float2 end)
{
    return float2(randrange(start.x, end.x), randrange(start.y, end.y));
}

inline float3 randrange(float3 start, float3 end)
{
    return float3(randrange(start.x, end.x), randrange(start.y, end.y), randrange(start.z, end.z));
}

inline float randangle() { return randrange(0.f, M_TAOf); }
inline float2 randpolar(float minradius, float maxradius) 
{
    return randrange(minradius, maxradius) * angleToVector(randangle());
}
inline float2 randpolar(float radius) 
{
    return radius * angleToVector(randangle());
}

// randomized vector with length (minradius, maxradius], uniformly distributed throughout area
inline float2 randpolar_uniform(float minradius, float maxradius) 
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


inline bool chance(float v) { return randrange(0.f, 1.f) < v; }

// Box-Muller transform
// adapted from http://en.literateprograms.org/Box-Muller_transform_(C)#chunk def:Apply Box-Muller transform on x, y
inline float rand_normal(float mean, float stddev)
{
    static float n2 = 0.0;
    static int n2_cached = 0;
    if (!n2_cached)
    {
        float x, y, r;
        do {
            x = randrange(-1.f, 1.f);
            y = randrange(-1.f, 1.f);
            r = x*x + y*y;
        } while (r == 0.0 || r > 1.0);

        float d = sqrtf(-2.f*logf(r)/r);
        float n1 = x*d;
        n2 = y*d;
        float result = n1*stddev + mean;
        n2_cached = 1;
        return result;
    }
    else
    {
        n2_cached = 0;
        return n2*stddev + mean;
    }
}

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

#endif // RAND_H
