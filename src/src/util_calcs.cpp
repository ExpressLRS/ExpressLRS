#include "util_calcs.h"

double dbm2mw(float dbm)
{
    return pow(10.0, dbm / 10.0);
}

double mw2dbm(float mw)
{
    return 10 * log10(mw);
}

double add_dbm(float a, float b)
{
    return mw2dbm(dbm2mw(a) + dbm2mw(b));
}

double thermal_noise_dbm(float bw)
{
    return -173.8 + 10 * log10(bw);
}