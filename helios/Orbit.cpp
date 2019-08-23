
#include "StdAfx.h"
#include "Orbit.h"

#include "Graphics.h"
#include "Test.h"

static const double kTiny = 1e-7;
static const double kTinyf = 1e-3;

inline double tanSqr(double x)
{
    const double z = tan(x);
    return z * z;
}

inline double acos_clamp(double x)
{
    return ((x <= -1.0) ? M_PI :
            (x >= 1.0) ? 0.0 :
            acos(x));
}

inline double acosh_clamp(double x)
{
    return (x <= 1.0) ? 0.0 : acosh(x);
}

double3 OrbitalElements::axis() const
{
    double3 vec(0.0, 0.0, 1.0);
    if (argper > kTiny) vec = rotateZ(vec, -argper);
    if (inclination > kTiny) vec = rotateX(vec, -inclination);
    if (longascend > kTiny) vec = rotateZ(vec, longascend);
    return vec;
}    

double OrbitalElements::semiminor() const
{
    if (semimajor < 0.0)    // hyperbolic
        return -semimajor * sqrt(eccentricity * eccentricity - 1.0);
    else
        return semimajor * sqrt(1.0 - eccentricity * eccentricity);
}

double OrbitalElements::semilatus() const
{
    if (eccentricity == 1.0)
        return semimajor * 2.0;
    else
        return semimajor * (1.0 - eccentricity * eccentricity);
    // const double l = (eccentricity < 1.0) ? (1.0 - eccentricity * eccentricity) :
                     // (eccentricity == 1.0) ? 2.0 :
                     // /* (eccentricity > 1.0) ? */ (eccentricity * eccentricity - 1.0);

    // return l * semimajor; 
}

bool OrbitalElements::operator==(const OrbitalElements &o) const
{
    return eccentricity == o.eccentricity && semimajor == o.semimajor &&
        inclination == o.inclination && longascend == o.longascend &&
        argper == o.argper && trueanom == o.trueanom && gravparam == o.gravparam;
}


bool fpu_error(const OrbitalElements &s)
{
    return fpu_error(s.eccentricity) || fpu_error(s.semimajor) ||
        fpu_error(s.inclination) || fpu_error(s.longascend) ||
        fpu_error(s.argper) || fpu_error(s.trueanom) ||
        fpu_error(s.gravparam);
}

double3 pos_from_elements(const OrbitalElements &el)
{
    const double p = el.semilatus();
    const double r = p / (1.0 + el.eccentricity * cos(el.trueanom)); // distance from CoM

    ASSERT(r > 0.0);

    const d2 Omega = a2v(el.longascend);
    const d2 u = a2v(el.argper + el.trueanom);
    const d2 i = a2v(el.inclination);
        
    return r * d3(Omega.x * u.x - Omega.y * u.y * i.x,
                  Omega.y * u.x + Omega.x * u.y * i.x,
                  i.y * u.y);
}

OrbitalState state_from_elements(const OrbitalElements &el)
{
    const double p = el.semilatus();
    const double r = p / (1.0 + el.eccentricity * cos(el.trueanom)); // distance from CoM

    ASSERT(r > 0.0);

    const d2 Omega = a2v(el.longascend);
    const d2 u = a2v(el.argper + el.trueanom);
    const d2 i = a2v(el.inclination);

    OrbitalState st;
    st.pos = r * d3(Omega.x * u.x - Omega.y * u.y * i.x,
                    Omega.y * u.x + Omega.x * u.y * i.x,
                    i.y * u.y);

    const double h_r = sqrt(el.gravparam * p) / r;
    const double q   = h_r * el.eccentricity * sin(el.trueanom) / p;
    st.vel = st.pos * q - h_r * d3(Omega.x * u.y + Omega.y * u.x * i.x,
                                   Omega.y * u.y - Omega.x * u.x * i.x,
                                   -i.y * u.x);
    return st;
}

OrbitalElements elements_from_state(const OrbitalState &st, double gravparam)
{
    OrbitalElements el;
    el.gravparam = gravparam;
    
    // angular momentum
    const double3 h_ = cross(st.pos, st.vel);
    const double h = length(h_);
    const double r = length(st.pos);
    const double v = length(st.vel);
    const double E = (v*v / 2.0) - (gravparam / r); // specific energy
    
    el.semimajor = -gravparam / (2.0 * E);
    el.eccentricity = sqrt(max(0.0, 1.0 - h*h / (el.semimajor * gravparam)));
    el.inclination = acos_clamp(h_.z / h);

    // el.longascend = atan2(h_.x, -h_.y);
    el.longascend = nearZero(abs(h_.z) - h) ? 0.0 : atan2(h_.x, -h_.y);
    if (el.longascend < 0.0)
        el.longascend += M_TAU;

    // argument of latitude
    const double i_y = sin(el.inclination);
    const double h_z_sign = (h_.z < 0.0) ? -1.0 : 1.0;
    const double u = nearZero(i_y) ? v2a(st.pos.x, st.pos.y * h_z_sign) : // true longitude
                     atan2(st.pos.z / i_y, dot(st.pos.xy(), a2v(el.longascend)));
    
    if (el.eccentricity < kTiny)
    {
        el.argper = 0.0;
        el.trueanom = u;
    }
    else
    {
        double p = el.semilatus();
        el.trueanom = acos_clamp((p - r) / (el.eccentricity * r));
        if (dot(st.pos, st.vel) < 0.0)
            el.trueanom = -el.trueanom;
        el.argper = modulo(u - el.trueanom, M_TAU);
    }

    DASSERT(!fpu_error(el));

    return el;
}


// M is the mean anomaly
// E is the eccentric anomaly
double true_anom(double M, double ecc)
{
    double theta = 0.0;
    if (ecc < 1.0)
    {
        M = modulo(M, M_TAU);
        auto kepler = [=](double E) { return E - ecc * sin(E) - M; };
        auto kepler_prim = [ecc](double E) { return 1.0 - ecc * cos(E); };

        double guess = (M > M_PI) ? M - ecc : M + ecc;
        double E = findRootNewton(kepler, kepler_prim, guess, kTiny);
        theta = 2.0 * atan(sqrt((1.0 + ecc) / (1.0 - ecc)) * tan(E / 2.0));
        DASSERT(!fpu_error(theta));
    }
    else if (ecc == 1.0)
    {
        // FIXME
        theta = M;
    }
    else if (ecc > 1.0)
    {
        // if (M > M_PI)
            // M -= M_TAU;
        auto kepler = [=](double H) { return ecc * sinh(H) - H - M; };
        auto kepler_prim = [ecc](double H) { return ecc * cosh(H) - 1.0; };

        // guess logic is from tudat convertMeanToEccentricAnomalies.h line 365
        double guess = 0;
        if (abs(M) < 6.0 * ecc)
        {
            double x = sqrt(8.0 * (ecc - 1.0) / ecc);
            guess = x * sinh((1.0/3.0) * asinh(3.0 * M / (x * (ecc - 1.0))));
        }
        else
        {
            guess = (M > 0.0) ? log(2.0 * M / ecc) : -log(-2.0 * M / ecc);
        }
        double H = findRootNewton(kepler, kepler_prim, guess, kTiny);
        theta = 2.0 * atan(sqrt((ecc + 1.0) / (ecc - 1.0)) * tanh(H / 2.0));

        //ASSERT(abs(theta) < M_PI_2 + asin(1.0 / ecc));
        DASSERT(!fpu_error(theta));
    }

    return theta;
}

// theta is the true anomaly
double mean_anom(double theta, double ecc)
{
    double M = 0.0;
    if (ecc < 1.0)
    {
        // eccentric anomaly
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
        // hyperbolic anomaly
        // the acos equation gets the sign wrong for negative theta because it goes through cos
        // double H = acosh_clamp((ecc + cos(theta)) / (1.0 + ecc * cos(theta)));
        double H = 2.0 * atanh(sqrt((ecc - 1.0) / (ecc + 1.0)) * tan(theta / 2.0));
        M = ecc * sinh(H) - H;
        DASSERT(!fpu_error(M));
    }
    return M;
}

double OrbitalElements::step_trueanom(double dt) const
{
    if (abs(dt) < kTiny)
        return trueanom;
    // also correct for hyperbolas with negative semimajor
    double deltaM = dt * sqrt(gravparam / abs(cubed(semimajor)));
	return true_anom(mean_anom() + deltaM, eccentricity);
}

void OrbitalElements::step(double dt)
{
    trueanom = step_trueanom(dt);
}

void OrbitalElements::step_circular(double dt)
{
    double deltaM = dt * sqrt(gravparam / abs(cubed(semimajor)));
    trueanom += deltaM;
}

OrbitalElements OrbitalState::to_elements(double gravparam) const
{
    return elements_from_state(*this, gravparam);
}


OrbitalElements2 elements_from_state(const state4 &st, float gravparam)
{
    const double h = cross(st.pos, st.vel);
    const double r = length(st.pos);
    const double v = length(st.vel);
    const double E = (v*v / 2.0) - (gravparam / r); // specific energy

    OrbitalElements2 el;
    el.gravparam = gravparam;
    el.semimajor = -gravparam / (2.0 * E);
    el.eccentricity = sqrt(max(0.0, 1.0 - h*h / (el.semimajor * gravparam)));
    el.inclination = (h < 0.0) ? -1.f : 1.f;

    const double u = v2a(st.pos.x, st.pos.y * el.inclination);
    
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

state4 state_from_elements(const OrbitalElements2 &el)
{
    const double p   = el.semilatus();
    const double r   = p / (1.0 + el.eccentricity * cos(el.trueanom)); // distance from CoM

    const d2     u   = a2v(el.argper + el.trueanom);
    const double i_x = el.inclination;

    const d2 pos = r * d2(u.x, u.y * i_x);
    
    const double h_r = sqrt(el.gravparam * p) / r;
    const double q   = h_r * el.eccentricity * sin(el.trueanom) / p;
 
    const d2 vel = pos * q - h_r * d2(u.y, -u.x * i_x);

    return state4(pos, vel);
}


OrbitalElements2::OrbitalElements2(const OrbitalElements &el3)
    : eccentricity(el3.eccentricity),
      semimajor(el3.semimajor),
      inclination((el3.inclination > M_PI_2f) ? -1.f : 1.f),
      argper(el3.argper),
      trueanom(el3.trueanom),
      gravparam(el3.gravparam){}

f2 OrbitalElements2::to_pos() const
{
	const OrbitalElements2 &el = *this;
    const double p   = el.semilatus();
    const d2     u   = a2v(el.argper + el.trueanom);
    const double r   = p / (1.0 + el.eccentricity * cos(el.trueanom)); // distance from CoM

    return r * d2(u.x, u.y * el.inclination);
}


void OrbitalElements2::step(double dt)
{
	trueanom = true_anom(mean_anom() + M_TAU * dt / period(), eccentricity);
}


d2 radec2lambdabeta(d2 radec)
{
    d2 a = a2v(radec.x);
    d2 d = a2v(radec.y);
    return d2(atan((a.y * kEarthTiltVec.x + tan(radec.y) * kEarthTiltVec.y) / a.x),
              d.y * kEarthTiltVec.x - d.x * kEarthTiltVec.y * a.y);
}

d3 radec2ecliptic(d2 radec)
{
    d2 lb = radec2lambdabeta(radec);
    return d3(a2v(lb.x) * cos(lb.y), sin(lb.y));
}


#include "Save.h"

static double kThresh = 1e-5;
static double kThreshf = 1e-2;

static bool about_eql(double a, double b)
{
    return abs(a - b) < max(kTiny, max(abs(a), abs(b)) * kThresh);
}

static bool about_eql(float a, float b)
{
    return abs(a - b) < max(kTinyf, max(abs(a), abs(b)) * kThreshf);
}

static bool about_eql(const OrbitalState &a, const OrbitalState &b)
{
    return (about_eql(a.pos.x, b.pos.x) &&
            about_eql(a.pos.y, b.pos.y) &&
            about_eql(a.pos.z, b.pos.z) &&
            about_eql(a.vel.x, b.vel.x) &&
            about_eql(a.vel.y, b.vel.y) &&
            about_eql(a.vel.z, b.vel.z));
}

static bool about_eql(const state4 &a, const state4 &b)
{
    return (about_eql(a.pos.x, b.pos.x) &&
            about_eql(a.pos.y, b.pos.y) &&
            about_eql(a.vel.x, b.vel.x) &&
            about_eql(a.vel.y, b.vel.y));
}


static void check_specific_energy(const state4 &st, const OrbitalElements2 &el)
{
    double E0 = -el.gravparam / (2.0 * el.semimajor);
    
    double r = length(st.pos);
    double v = length(st.vel);
    double E1 = (v*v / 2.0) - (el.gravparam / r); // specific energy

    ASSERT(about_eql(E0, E1));
}


DEF_TEST(orbit)
{
    vector<OrbitalElements> vec;
    loadFileAndParse("data/element_test.lua", &vec);

    for_ (el, vec)
    {
        OrbitalState st = el.to_state();
        OrbitalElements el2 = st.to_elements(el.gravparam);
        OrbitalState st2 = el2.to_state();
        double3 pos = el2.to_pos();

        ASSERT(about_eql(st, st2));
        ASSERT(about_eql(pos, st2.pos));

        if (el.eccentricity < 1.0)
        {
            OrbitalElements el3 = el;
            el3.step(el3.period());
            OrbitalState st3 = el3.to_state();
            ASSERT(about_eql(st, st3));
        }

        OrbitalElements el4 = el;
        el4.step(kTiny);
        OrbitalState st4 = el4.to_state();
        ASSERT(about_eql(st, st4));
        OrbitalElements el5 = el;
        el5.step(kTiny);
    }

    for_ (el3d, vec)
    {
        OrbitalElements2 el = el3d;
        state4 st = el.to_state();
        OrbitalElements2 el2 = elements_from_state(st, el.gravparam);        

        check_specific_energy(st, el);
        check_specific_energy(st, el2);
        
        state4 st2 = el2.to_state();
        f2 pos = el2.to_pos();

        ASSERT(about_eql(st, st2));
        ASSERT(about_eql(pos, st2.pos));

        if (el.eccentricity < 1.0)
        {
            OrbitalElements2 el3 = el;
            el3.step(el3.period());
            state4 st3 = el3.to_state();
            ASSERT(about_eql(st, st3));
        }

        OrbitalElements2 el4 = el;
        el4.step(0.0000001);
        state4 st4 = el4.to_state();
        ASSERT(about_eql(st, st4));
    }
}
