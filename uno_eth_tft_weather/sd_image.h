// ==================================================
// sd_image.h - raw RGB565 images (big-endian, no header) from the SD card
// ==================================================
#pragma once
#include <Arduino.h>

bool sdBegin();
bool drawImage(const char *name, int16_t x, int16_t y, uint16_t w, uint16_t h);
