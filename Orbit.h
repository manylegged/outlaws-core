
#pragma once

struct OrbitalElements2;

struct state4 {
    float2 pos;
    float2 vel;
};

OrbitalElements2 elements_from_state(const state4 &st, float gravparam);
state4 state_from_elements(const OrbitalElements2 &st);

double mean_anom(double theta, double ecc);
double true_anom(double M, double ecc);

static const float epsilonf = 0.00001f;
static const double kTiny = 1e-7;

template <typename T>
inline T cubed(T v) { return v * v * v; }

inline double v2a(double x, double y) { return std::atan2(y, x); }

inline double acos_clamp(double x)
{
    return (x <= -1.0) ? M_PI :
        (x >= 1.0) ? 0.0 :
        acos(x);
}
inline double acosh_clamp(double x)
{
    return (x <= 1.0) ? 0.0 : acosh(x);
}

struct OrbitalElements2 {
    float eccentricity = 0.0;   // e
    float semimajor    = 0.0;   // a
    float argper       = 0.0;   // argument of periapsis (w, omega, 0-M_TAU)
    float trueanom     = 0.0;   // v
    float inclination  = 1.0;   // i_x
    
    float gravparam = 0.0;

    OrbitalElements2() {}
    OrbitalElements2(const state4 &s, float mu) { *this = elements_from_state(s, mu); }

    double semilatus() const
    {
        return semimajor * ((eccentricity == 1.f) ? 2.f : (1.0 - eccentricity * eccentricity));
    }

    // T orbital period. semimajor axis is negative for hyperbola
    double period() const { return M_TAU * sqrt(cubed(abs((double)semimajor)) / (double)gravparam); }

    double periapsis() const { return (1.0 - eccentricity) * semimajor; } // nearest
    double apoapsis() const { return (1.0 + eccentricity) * semimajor; }  // farthest
    
    double perispeed() const { return sqrt(((1.0 + eccentricity) * gravparam) /
                                        (1.0 - eccentricity) * semimajor); } // maximum speed
    double apospeed() const { return sqrt(((1.0 - eccentricity) * gravparam) /
                                       (1.0 + eccentricity) * semimajor); } // minimum speed
    double circlespeed() const { return sqrt(gravparam / semimajor); }
    
    state4 to_state() const { return state_from_elements(*this); }
    f2 to_pos() const;

    void step(double dt);
    void step_circular(double dt) { trueanom += M_TAU * dt / period(); }

    double mean_anom() const { return ::mean_anom(trueanom, eccentricity); }

    void set_apsis(double periapsis, double apoapsis)
    {
        semimajor = (periapsis + apoapsis) / 2.0;
        eccentricity = 1.0 - (periapsis / semimajor);
    }
};

inline OrbitalElements2 elements_from_state(const state4 &st, float gravparam)
{
    const double h = cross(st.pos, st.vel);
    const double r = length(st.pos);
    const double v = length(st.vel);
    const double E = (v*v / 2.0) - (gravparam / r); // specific energy

    OrbitalElements2 el;
    el.gravparam = gravparam;
    el.semimajor = -gravparam / (2.0 * E);
    el.eccentricity = sqrt(max(0.0, 1.0 - h*h / (el.semimajor * gravparam)));
    el.inclination = (h < 0.0) ? -1.f : 1.0;

    const double u = v2a((double)st.pos.x, st.pos.y * el.inclination);
    
    if (el.eccentricity < epsilonf) {
        el.argper = 0.f;
        el.trueanom = u;
    } else {
        const double p = el.semilatus();
        el.trueanom = acos_clamp((p - r) / (el.eccentricity * r));
        if (dot(st.pos, st.vel) < 0.f)
            el.trueanom = -el.trueanom;
        el.argper = modulo(u - el.trueanom, M_TAU);
        DASSERT(!fpu_error(el.trueanom));
        DASSERT(!fpu_error(el.argper));
    }
    return el;
}

inline OrbitalElements2 circular_elements_from_state(const state4 &st, float gravparam)
{
    OrbitalElements2 el;
    el.gravparam = gravparam;
    el.semimajor = length(st.pos);
    el.inclination = (cross(st.pos, st.vel) < 0.0) ? -1.f : 1.0;
    el.trueanom = v2a(st.pos.x, st.pos.y * el.inclination);
    return el;
}

inline state4 state_from_elements(const OrbitalElements2 &el)
{
    const double p   = el.semilatus();
    const double r   = p / (1.0 + el.eccentricity * cos(el.trueanom)); // distance from CoM

    const d2     u   = a2v(el.argper + el.trueanom);
    const double i_x = el.inclination;

    const d2 pos = r * d2(u.x, u.y * i_x);

    const double h_r = sqrt(el.gravparam * p) / r;
    const double q   = h_r * el.eccentricity * sin(el.trueanom) / p;
    
    const d2 vel = pos * q - h_r * d2(u.y, -u.x * i_x);
    
    return state4{pos, vel};
}

inline f2 OrbitalElements2::to_pos() const
{
	const OrbitalElements2 &el = *this;
    const double p   = el.semilatus();
    const d2     u   = a2v(el.argper + el.trueanom);
    const double r   = p / (1.0 + el.eccentricity * cos(el.trueanom)); // distance from CoM

    return r * d2(u.x, u.y * el.inclination);
}


inline void OrbitalElements2::step(double dt)
{
	trueanom = true_anom(mean_anom() + M_TAU * dt / period(), eccentricity);
}

// theta is the true anomaly
inline double mean_anom(double theta, double ecc)
{
    double M = 0.0;
    if (ecc < 1.0)
    {
        double E = atan2(sqrt(1.0 - ecc*ecc) * sin(theta), (ecc + cos(theta)));
        M = E - ecc * sin(E);
        DASSERT(!fpu_error(M));
    }
    else if (ecc == 1.0)
    {
        double D = tan(theta / 2.0);
        M = D + cubed(D) / 3.0;
    }
    else if (ecc > 1.0)
    {
        double costheta = cos(theta);
        double E = acosh_clamp((ecc + costheta) / (1.0 + ecc * costheta));
        M = ecc * sinh(E) - E;
        DASSERT(!fpu_error(M));
    }
    return M;
}

// return x where f(x) = 0
// funp is the derivative of fun
template <typename Fun, typename FunP>
double findRootNewton(const Fun& fun, const FunP &funp, double x0, double error=kTiny)
{
    double x = x0;
    double y = fun(x);
    
    while (abs(y) > error)
    {
        y = fun(x);
        double dy = funp(x);
        // if (abs(dy) < epsilon)
            // dy = fun(x)
        x = x - y / dy;
    }
    return x;
}

// M is the mean anomaly
// E is the eccentric anomaly
inline double true_anom(double M, double ecc)
{
    M = modulo(M, M_TAU);

    double theta = 0.0;
    if (ecc < 1.0)
    {
        auto kepler = [=](double E) { return E - ecc * sin(E) - M; };
        auto kepler_prim = [ecc](double E) { return 1.0 - ecc * cos(E); };

        double E = findRootNewton(kepler, kepler_prim, (ecc > 0.8) ? M_PI : M, kTiny);
        theta = 2.0 * atan(sqrt((1 + ecc) / (1.0 - ecc)) * tan(E / 2.0));
    }
    else if (ecc == 1.0)
    {
        // FIXME
        theta = M;
    }
    else if (ecc > 1.0)
    {
        auto kepler = [=](double E) { return ecc * sinh(E) - E - M; };
        auto kepler_prim = [ecc](double E) { return ecc * cosh(E) - 1.0; };

        double E = findRootNewton(kepler, kepler_prim, M_PI, kTiny);
        theta = 2.0 * atan(sqrt((ecc + 1.0) / (ecc - 1.0)) * tanh(E / 2.0));
    }

    DASSERT(!fpu_error(theta));
    return theta;
}
