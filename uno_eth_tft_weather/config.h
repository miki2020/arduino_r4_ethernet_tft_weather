// ==================================================
// config.h - everything you may want to change
// ==================================================
#pragma once
#include <Arduino.h>

// ---- ST7789 240x280 (SPI: SCL->D13, SDA->D11) ----
#define TFT_CS      7          // D10 belongs to the Ethernet chip
#define TFT_DC      9
#define TFT_RST     8

// ---- shield chip selects ----
#define SD_CS       4
#define ETH_CS     10

// ---- network (gateway + DNS are assumed to be x.x.x.1, mask 255.255.255.0) ----
#define NET_MAC     0x02, 0x12, 0x34, 0x56, 0x78, 0x01
#define NET_IP      192, 168, 1, 50

// ---- timing ----
#define REFRESH_MS  600000UL   // weather refresh: 10 min (Open-Meteo updates every 15 min)
#define RETRY_MS     30000UL   // retry after a failed update
#define LOCATION_SWITCH_MS 6000UL

// ---- location used until you send a new one over Serial ----
#define DEFAULT_LAT   "51.4055"
#define DEFAULT_LON   "19.7032"
#define DEFAULT_NAME  "Piotrkow"
