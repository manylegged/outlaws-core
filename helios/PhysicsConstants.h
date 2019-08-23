
#pragma once

static const double kG = 6.67408e-20; // km3 / (kg s2)
static const double kC = 299792.458; // km/s

static const double kSecondsFromUnixToJ2000Epoch =  30 * 365 * 24 * 3600;

static const double kStefanBoltzman = 5.67e-8; // W/m²K4

static const double kAUinKm = 1.496e8;
static const double kLightYearinKm = 9.4607e13;
static const double kParsecinKm = 3.0857e13;
static const double kSunGM = 1.32712440018e11; // km3/s2
static const double kEarthGM = 3.986004418e5;  // km3/s2
static const double kEarthRadius = 6371.0;    // km
static const double kSunRadius = 695700;
static const double kSunLuminosity = 3.827e26; // watts

static const double kZeroCInKelvin = 273.16;

static const double kOneG = 9.80665e-3; // km/s
static const double kDaysPerYear = 365.2422;
static const double kSecondsPerHour = 3600.0;
static const double kSecondsPerDay = kSecondsPerHour * 24.0;
static const double kSecondsPerWeek = kSecondsPerDay * 7;
static const double kSecondsPerMonth = kSecondsPerDay * 30;
static const double kSecondsPerYear = kSecondsPerDay * kDaysPerYear;
static const double kHoursPerYear = kDaysPerYear * 24.0;


inline double watts_per_m2(double semimajor_km)
{
    // 0.967 is a fudge factor to get the earth watts/meter to be correct
    return 0.967 * kSunLuminosity / (4.0 * M_PI * squared(semimajor_km * 1e3));           
}

struct Greybody {
    double albedo                   = 0.5;
    double semimajor_km             = kAUinKm;
    double solar_area_to_total_area = 0.25; // 0.25 is for a sphere
    double emissivity               = 0.9;
    double power_per_area           = 0.0; // heat in watts/sq meter

    double mean_temperature() const
    {
        // https://en.wikipedia.org/wiki/Standard_asteroid_physical_characteristics#Surface_temperature

        double watts_per_area = watts_per_m2(semimajor_km);
        double specific_solar_watts = (1.0 - albedo) * watts_per_area * solar_area_to_total_area;

        ASSERT(specific_solar_watts + power_per_area >= 0);

        double temp = pow((specific_solar_watts + power_per_area) / (emissivity * kStefanBoltzman), 0.25);
        return temp;
    }

    double max_temperature() const { return M_SQRT2 * mean_temperature(); }
    double min_temperature() const { return (1.0/M_SQRT2) * mean_temperature(); }
};


// https://en.wikipedia.org/wiki/Tsiolkovsky_rocket_equation
// deltav is km/s
// Mf is Mass final (mass of rocket)
// return mass of propellant in the same units as Mf
inline double rocket_propellant_mass(double deltav, double mf, double specific_impulse)
{
    double ve = specific_impulse * kOneG;
    double mass_ratio = exp(deltav / ve);
    // mass ratio = m0 / mf
    return mass_ratio * mf - mf;
}

inline double rocket_propellant_mass_initial(double deltav, double m0, double specific_impulse)
{
    double ve = specific_impulse * kOneG;
    double mass_ratio = exp(deltav / ve);
    return m0 - (m0 / mass_ratio);
}
