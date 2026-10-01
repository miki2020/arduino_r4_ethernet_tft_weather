// ==================================================
// display.h - everything that draws on the ST7789
// ==================================================
#pragma once
#include <Adafruit_ST7789.h>
#include "location.h"
#include "weather.h"

extern Adafruit_ST7789 tft;

void displayInit();
void displayMessage(const __FlashStringHelper *a, const __FlashStringHelper *b);
void displayWeather(const Weather &w, const Location &l);   // full scene picture + values
void displayStale();                                        // small red mark: data is old
