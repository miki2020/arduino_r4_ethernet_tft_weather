
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

Weather weather[MAX_LOCATIONS];
bool haveData[MAX_LOCATIONS] = { false };
bool fetchFailed[MAX_LOCATIONS] = { false };
unsigned long lastFetch = 0;
byte displayScreen = 0;

static void waitFor(unsigned long ms) {
  unsigned long t = millis();
  while (millis() - t < ms) {
    if (locationPoll()) {
      displayScreen = 0;
      for (byte i = 0; i < locationCount; i++) {
        haveData[i] = false;
        fetchFailed[i] = false;
      }
      lastFetch = 0;
      return;
    }
  }
}

static void fetchWeather() {
  for (byte i = 0; i < locationCount; i++) {
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

  // keep every SPI device deselected before starting anything
  pinMode(TFT_CS, OUTPUT); digitalWrite(TFT_CS, HIGH);
  pinMode(ETH_CS, OUTPUT); digitalWrite(ETH_CS, HIGH);
  pinMode(SD_CS,  OUTPUT); digitalWrite(SD_CS,  HIGH);

  Serial.println(F("[BOOT] Initializing display"));
  displayInit();
  displayMessage(F("BOOT"), F("INITIALIZING"));
  Serial.println(F("[BOOT] Display ready"));

  locationLoad();

  netBegin();
  delay(1000);

  Serial.println(F("[SD] Initializing card"));
  if (!sdBegin()) {
    Serial.println(F("[SD] Initialization failed; check card, wiring, and format"));
    displayMessage(F("SD"), F("ERROR"));
    while (true) {}
  }
  Serial.println(F("[SD] Ready"));

  displayMessage(F("READY"), F("LOADING DATA"));
  delay(1200);

  locationHelp();
}

void loop() {

  if (locationCount == 0) {
    displayMessage(F("NO DATA"), F("ADD LOCATION"));
    waitFor(LOCATION_SWITCH_MS);
    return;
  }

  unsigned long refreshInterval = fetchFailed[displayedLocation] ? RETRY_MS : REFRESH_MS;
  if (!lastFetch || millis() - lastFetch >= refreshInterval) fetchWeather();

  if (haveData[displayedLocation]) {
    displayWeather(weather[displayedLocation], locations[displayedLocation], displayScreen == 0);
    if (displayScreen == 0 && fetchFailed[displayedLocation]) displayStale();
  } else {
    if (displayScreen == 0) displayMessage(F("NO DATA"), F("RETRYING.."));
    else {
      char scene[12];
      strcpy_P(scene, PSTR("CLRDAY.BIN"));
      drawImage(scene, 0, 0, 280, 240);
    }
  }

  displayStatusBadge(Ethernet.linkStatus() == LinkON, displayedLocation + 1, locationCount);

  waitFor(LOCATION_SWITCH_MS);
  if (displayScreen == 0) displayScreen = 1;
  else {
    displayScreen = 0;
    if (locationCount > 1) displayedLocation = (displayedLocation + 1) % locationCount;
  }
}
