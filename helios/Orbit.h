
#pragma once

#include "PhysicsConstants.h"


#define DEF_OP(K, X) friend K operator X(const K &a, const K &b) { return K(a.pos X b.pos, a.vel X b.vel); } \
                     friend K &operator X ## =(K &a, const K &b) { return a = a X b; return a; }
                             
#define DEF_DOP(K, X) friend K operator X(const K &a, value_type b) { return K(a.pos X b, a.vel X b); } \
                      friend K operator X(value_type a, const K &b) { return K(a X b.pos, a X b.vel); } \
                      friend K &operator X ## =(K &a, value_type b) { return a = a X b; return a; }

// first derivative state
template <typename V>
struct State1 {

    typedef V                      vector_type;
    typedef typename V::value_type value_type;

    enum {
        dims = vector_type::components,
        components = 2 * vector_type::components
    };
    
    V pos;
    V vel;

    DEF_OP(State1, -);
    DEF_OP(State1, +);
    DEF_DOP(State1, *);
    DEF_DOP(State1, /);

    friend State1 operator*(const glm::mat3 &m, const State1 &s) { return State1(m * s.pos, m * s.vel); }
    friend State1 operator*(const glm::mat4 &m, const State1 &s) { return State1(transform(m, s.pos), transform_vec(m, s.vel)); }

    friend value_type distance(const State1 &a, const State1 &b) { return ::distance(a.pos, b.pos) + ::distance(a.vel, b.vel); }
    friend bool fpu_error(const State1 &s) { return fpu_error(s.pos) || fpu_error(s.vel); }

    value_type &operator[](size_t i) { return (i > dims) ? vel[i-dims] : pos[i]; }
    const value_type &operator[](size_t i) const { return (i > dims) ? vel[i-dims] : pos[i]; }

    State1() {}
    State1(const vector_type &p) : pos(p) {}
    State1(const vector_type &p, const vector_type &v) : pos(p), vel(v) {}
    template <typename U>
    State1(const State1<U> &s) : pos(s.pos), vel(s.vel) {}

    State1(const glm::tvec4<value_type> &v) : pos(V(v.xy())), vel(V(v.zw())) { static_assert(components == 4, ""); }
    operator glm::tvec4<value_type>() { static_assert(components == 4, ""); return glm::tvec4<value_type>(pos.xy(), vel.xy()); }

    bool operator==(const State1 &o) const { return pos == o.pos && vel == o.vel; }
    
    void step(value_type dt)
    {
        pos += vel * dt;
    }
};

#undef DEF_OP
                      
struct OrbitalElements;
struct OrbitalState;

double mean_anom(double theta, double ecc);
double true_anom(double M, double ecc);

OrbitalState state_from_elements(const OrbitalElements &el);
double3 pos_from_elements(const OrbitalElements &el);
OrbitalElements elements_from_state(const OrbitalState &st, double gravparam);

struct OrbitalElements;

struct OrbitalState : public State1<d3> {

    OrbitalState() {}
    OrbitalState(const OrbitalState &s) : State1<d3>(s) {}
    OrbitalState(const State1<d3> &s) : State1<d3>(s) {}
    OrbitalState(const double3 &p) : State1<d3>(p) {}
    OrbitalState(const double3 &p, const double3 &v) : State1<d3>(p, v) {}

    OrbitalElements to_elements(double gravparam) const;

    friend double distance(const OrbitalState &a, const OrbitalState &b) { return distance(State1<d3>(a), State1<d3>(b)); }
    friend bool fpu_error(const OrbitalState &s) { return fpu_error(State1<d3>(s)); }

    d3 axis() const { return normalize(cross(pos, vel)); }

    SERIAL_INDEXED_ACCEPT(v) { return v.VISIT(pos.x) && v.VISIT(pos.y) && v.VISIT(pos.z) &&
            v.VISIT(vel.x) && v.VISIT(vel.y) && v.VISIT(vel.z); }
};

struct OrbitalElements {
    double eccentricity = 0.0;   // e (>0)
    double semimajor    = 0.0;   // alpha (m or km)
    double inclination  = 0.0;   // i (0-M_PI)
    double longascend   = 0.0;   // longitude of the ascending node (Omega, 0-M_TAU)
    double argper       = 0.0;   // argument of periapsis (w, omega, 0-M_TAU)
    double trueanom     = 0.0;   // true anomaly (v, 0-M_TAU)
 
    double gravparam    = 0.0;   // (m3/s2 or km3/s2)

    // e < 1 is elliptical
    // e == 1 is parabolic
    // e > 1 is hyperbolic

    void step(double dt);
    void step_circular(double dt); // faster, for rendering orbits
    double step_trueanom(double dt) const;

    // T orbital period. semimajor axis is negative for hyperbola so it returns nan
    double period() const 
    {
        ASSERT(semimajor > 0.0);
        return M_TAU * sqrt(cubed(semimajor) / gravparam); 
    }

    double semiminor() const;

    // (r) distance from CoM
    double distance() const { return semilatus() / (1.0 + eccentricity * cos(trueanom)); }

    // for hyperbolic orbits - e=1 -> -M_PI  e=infinity -> 0
    double asymptotic_angle() const { return 2.0 * asin(clamp(1.0 / eccentricity)); }
    
    void semimajor_from_period(double T)
    {
        semimajor = pow(gravparam * squared(T / M_TAU), 1.0/3.0);
    }

    double mean_anom() const { return ::mean_anom(trueanom, eccentricity); }
    double3 axis() const;        // axis of revolution
    OrbitalState to_state() const { return state_from_elements(*this); }
    double3 to_pos() const { return pos_from_elements(*this); }

    // semi-latus rectum (p)
    double semilatus() const;
    double periapsis() const { return (1.0 - eccentricity) * semimajor; } // nearest, fastest (negative for hyperbolic)
    double apoapsis() const { return (1.0 + eccentricity) * semimajor; }  // farthest, slowest

    double perispeed() const { return sqrt(((1.0 + eccentricity) * gravparam) /
                                        (1.0 - eccentricity) * semimajor); } // maximum speed
    double apospeed() const { return sqrt(((1.0 - eccentricity) * gravparam) /
                                       (1.0 + eccentricity) * semimajor); } // minimum speed
    double circlespeed() const { return sqrt(gravparam / semimajor); }

    void set_apsis(double periapsis, double apoapsis)
    {
        semimajor = (periapsis + apoapsis) / 2.0;
        eccentricity = 1.0 - (periapsis / semimajor);
    }

    friend bool fpu_error(const OrbitalElements &s);

    bool operator==(const OrbitalElements &o) const;

    SERIAL_INDEXED_ACCEPT(v) {
        return v.VISIT(eccentricity) && v.VISIT(semimajor) && v.VISIT(inclination) &&
            v.VISIT(longascend) && v.VISIT(argper) && v.VISIT(trueanom) && v.VISIT(gravparam);
    }
};

inline double eccentricity_from_apsis(double periapsis, double apoapsis)
{
    double semimajor = (periapsis + apoapsis) / 2.0;
    double eccentricity = 1.0 - (periapsis / semimajor);
    return eccentricity;
}

// equatorial coordinates are centered on PLANET with XY at the equator and Z the north pole
// ecliptic coordinates are centered on the sun with XY the orbit of the planet
// +x is in the same direction for both coordinate systems
inline double3 equatorial_from_ecliptic(d3 pos, double axial_tilt)
{
    const double cosa = cos(axial_tilt);
    const double sina = sin(axial_tilt);
    return d3(pos.x, cosa * pos.y - sina * pos.z, sina * pos.y + cosa * pos.z);
}

static const double kEarthTilt = 23.43702 * kToRadians;
static const double2 kEarthTiltVec = a2v(kEarthTilt);

// convert right ascension, declination into ecliptic polar coordinates (lambda, beta)
d2 radec2lambdabeta(d2 radec);
// convert right ascension, declination into ecliptic cartesian coordinates
d3 radec2ecliptic(d2 radec);


template <typename T>
T stepped(T it, double dt)
{
    it.step(dt);
    return it;
}

template <typename T>
T stepped_circular(T it, double dt)
{
    it.step_circular(dt);
    return it;
}

inline double mu_from_T_alpha(double T, double alpha) { return cubed(alpha) / squared(T/M_TAU); }
inline double T_from_mu_alpha(double mu, double alpha) { return sqrt(cubed(alpha) / mu); }
inline double v_from_T_a(double T, double a) { return M_TAU * a / T; }

typedef State1<f2> state4;
struct OrbitalElements2;

OrbitalElements2 elements_from_state(const state4 &st, float gravparam);
state4 state_from_elements(const OrbitalElements2 &st);

struct OrbitalElements2 {
    float eccentricity = 0.0;   // e
    float semimajor    = 0.0;   // a
    float inclination  = 1.0;   // i, -1 for retrograde or 1
    float argper       = 0.0;   // argument of periapsis (w, omega, 0-M_TAU)
    float trueanom     = 0.0;   // v
    
    float gravparam = 0.0;

    OrbitalElements2() {}
    OrbitalElements2(const OrbitalElements &el3);
    OrbitalElements2(const state4 &s, float mu) { *this = elements_from_state(s, mu); }

    double semilatus() const
    {
        return semimajor * ((eccentricity == 1.f) ? 2.f : (1.0 - eccentricity * eccentricity));
    }

    // T orbital period. semimajor axis is negative for hyperbola
    double period() const { return M_TAU * sqrt(cubed(abs((double)semimajor)) / (double)gravparam); }
    
    double periapsis() const { return (1.0 - eccentricity) * semimajor; } // nearest
    double apoapsis() const { return (1.0 + eccentricity) * semimajor; } // farthest

    double perispeed() const { return sqrt(((1.0 + eccentricity) * gravparam) /
                                        (1.0 - eccentricity) * semimajor); } // maximum speed
    double apospeed() const { return sqrt(((1.0 - eccentricity) * gravparam) /
                                       (1.0 + eccentricity) * semimajor); } // minimum speed
    double circlespeed() const { return sqrt(gravparam / semimajor); }

    void set_apsis(double periapsis, double apoapsis)
    {
        semimajor = (periapsis + apoapsis) / 2.0;
        eccentricity = 1.0 - (periapsis / semimajor);
    }

    state4 to_state() const { return state_from_elements(*this); }
    f2 to_pos() const;

    void step(double dt);
    double mean_anom() const { return ::mean_anom(trueanom, eccentricity); }
};

