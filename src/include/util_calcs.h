#pragma once
#include <Arduino.h>

double dbm2mw(float dbm);
double mw2dbm(float mw);
double add_dbm(float a, float b);
double thermal_noise_dbm(float bw);


