// ==================================================
// geocode.h - city name -> name + coordinates (Open-Meteo geocoding API)
// ==================================================
#pragma once
#include "location.h"

bool geocodeCity(const char *city, Location &out);
