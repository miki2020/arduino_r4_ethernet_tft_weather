// ==================================================
// weather.h - current weather from Open-Meteo, kept as text
// ==================================================
#pragma once
#include "location.h"

struct Weather {
  char temp[7];     // deg C      "-12.6"
  char hum[4];      // %          "88"
  char isday[2];    // 0 / 1
  char rain[7];     // mm         "0.30"
  char cloud[4];    // %
  char press[8];    // hPa        "997.3"
  char wind[7];     // km/h
  char wdir[4];     // degrees
};

bool weatherFetch(const Location &l, Weather &w);
