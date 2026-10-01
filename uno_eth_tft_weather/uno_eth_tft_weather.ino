
#include <SPI.h>
#include <SD.h>
#include <Ethernet.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>

#include "config.h"
#include "location.h"
#include "weather.h"
#include "http_stream.h"
#include "sd_image.h"
#include "display.h"

Location locations[2];
Weather weather[2];
bool haveData[2] = { false, false };
bool fetchFailed[2] = { false, false };
unsigned long lastFetch = 0;
byte displayedLocation = 0;

// Keep the EEPROM/Serial-configured location in slot 2.
static void waitFor(unsigned long ms) {
  unsigned long t = millis();
  while (millis() - t < ms) {
    if (locationPoll()) {
      locations[1] = loc;
      haveData[1] = false;
      fetchFailed[1] = false;
      lastFetch = 0;
      return;
    }
  }
}

static void fetchWeather() {
  for (byte i = 0; i < 2; i++) {
    if (weatherFetch(locations[i], weather[i])) {
      haveData[i] = true;
      fetchFailed[i] = false;
      Serial.print(F("[WEATHER] Updated: temp=")); Serial.print(weather[i].temp);
      Serial.print(F(" C humidity=")); Serial.print(weather[i].hum);
      Serial.print(F("% wind=")); Serial.print(weather[i].wind);
      Serial.println(F(" km/h"));
    } else {
      fetchFailed[i] = true;
    }
  }
  lastFetch = millis();
}

void setup() {

  Serial.begin(9600);
  Serial.println(F("[BOOT] UNO Ethernet TFT weather starting"));
  locationLoad();
  strcpy_P(locations[0].lat, PSTR("51.7592"));
  strcpy_P(locations[0].lon, PSTR("19.4560"));
  strcpy_P(locations[0].name, PSTR("Lodz"));
  locations[0].magic = LOC_MAGIC;
  locations[1] = loc;

  // keep every SPI device deselected before starting anything
  pinMode(TFT_CS, OUTPUT); digitalWrite(TFT_CS, HIGH);
  pinMode(ETH_CS, OUTPUT); digitalWrite(ETH_CS, HIGH);
  pinMode(SD_CS,  OUTPUT); digitalWrite(SD_CS,  HIGH);

  Serial.println(F("[BOOT] Initializing display"));
  displayInit();
  Serial.println(F("[BOOT] Display ready"));


  netBegin();
  delay(1000);

  Serial.println(F("[SD] Initializing card"));
  if (!sdBegin()) {
    Serial.println(F("[SD] Initialization failed; check card, wiring, and format"));
    while (true) {}
  }
  Serial.println(F("[SD] Ready"));

  locationHelp();
}

void loop() {

  unsigned long refreshInterval = fetchFailed[0] || fetchFailed[1] ? RETRY_MS : REFRESH_MS;
  if (!lastFetch || millis() - lastFetch >= refreshInterval) fetchWeather();

  if (haveData[displayedLocation]) {
    displayWeather(weather[displayedLocation], locations[displayedLocation]);
    if (fetchFailed[displayedLocation]) displayStale();
  } else {
    displayMessage(F("NO DATA"), F("RETRYING.."));
  }

  waitFor(LOCATION_SWITCH_MS);
  displayedLocation = (displayedLocation + 1) % 2;
}
