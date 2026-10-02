#include "http_stream.h"
#include "weather.h"

static const char HOST[]      PROGMEM = "api.open-meteo.com";

// keys, in the order they appear in the "current" block
static const char K_CURRENT[] PROGMEM = "\"current\":";
static const char K_TEMP[]    PROGMEM = "\"temperature_2m\":";
static const char K_HUM[]     PROGMEM = "\"relative_humidity_2m\":";
static const char K_DAY[]     PROGMEM = "\"is_day\":";
static const char K_RAIN[]    PROGMEM = "\"precipitation\":";
static const char K_CLOUD[]   PROGMEM = "\"cloud_cover\":";
static const char K_PRESS[]   PROGMEM = "\"surface_pressure\":";
static const char K_WIND[]    PROGMEM = "\"wind_speed_10m\":";
static const char K_WDIR[]    PROGMEM = "\"wind_direction_10m\":";

bool weatherFetch(const Location &l, Weather &w) {

  Serial.print(F("[WEATHER] Requesting ")); Serial.print(l.name);
  Serial.print(F(" at ")); Serial.print(l.lat);
  Serial.print(','); Serial.println(l.lon);
  Serial.print(F("[WEATHER] Coordinates: ")); Serial.print(l.lat);
  Serial.print(F(" / ")); Serial.println(l.lon);

  if (!httpOpen(HOST)) return false;

  httpSendP(PSTR("GET /v1/forecast?latitude="));
  httpSend(l.lat);
  httpSendP(PSTR("&longitude="));
  httpSend(l.lon);
  httpSendP(PSTR("&current=temperature_2m,relative_humidity_2m,is_day,precipitation,"
                 "cloud_cover,surface_pressure,wind_speed_10m,wind_direction_10m"));
  httpEnd(HOST);
  httpBufferResponse();

  // skip "current_units", then read the values in the order they arrive
  const char *failedField = 0;
  if (!streamSeek(K_CURRENT)) failedField = "current";
  else if (!streamNumber(K_TEMP,  w.temp,  sizeof(w.temp))) failedField = "temperature_2m";
  else if (!streamNumber(K_HUM,   w.hum,   sizeof(w.hum))) failedField = "relative_humidity_2m";
  else if (!streamNumber(K_DAY,   w.isday, sizeof(w.isday))) failedField = "is_day";
  else if (!streamNumber(K_RAIN,  w.rain,  sizeof(w.rain))) failedField = "precipitation";
  else if (!streamNumber(K_CLOUD, w.cloud, sizeof(w.cloud))) failedField = "cloud_cover";
  else if (!streamNumber(K_PRESS, w.press, sizeof(w.press))) failedField = "surface_pressure";
  else if (!streamNumber(K_WIND,  w.wind,  sizeof(w.wind))) failedField = "wind_speed_10m";
  else if (!streamNumber(K_WDIR,  w.wdir,  sizeof(w.wdir))) failedField = "wind_direction_10m";

  httpClose();
  if (failedField) {
    Serial.print(F("[WEATHER] Response ended or missing field: "));
    Serial.println(failedField);
    return false;
  }
  return true;
}
