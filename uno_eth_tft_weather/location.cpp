#include <EEPROM.h>
#include "config.h"
#include "location.h"
#include "geocode.h"

Location loc;

static char cmd[32];
static byte cmdLen = 0;

static char *trim(char *s) {
  while (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n') s++;
  char *end = s + strlen(s);
  while (end > s && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r' || end[-1] == '\n')) *--end = 0;
  return s;
}

void locationLoad() {
  EEPROM.get(0, loc);
  if (loc.magic != LOC_MAGIC) {
    strcpy_P(loc.lat,  PSTR(DEFAULT_LAT));
    strcpy_P(loc.lon,  PSTR(DEFAULT_LON));
    strcpy_P(loc.name, PSTR(DEFAULT_NAME));
    Serial.println(F("[LOCATION] No saved location; using defaults"));
  } else {
    Serial.println(F("[LOCATION] Loaded saved location from EEPROM"));
  }
  Serial.print(F("[LOCATION] ")); Serial.print(loc.name);
  Serial.print(F(" at ")); Serial.print(loc.lat);
  Serial.print(','); Serial.println(loc.lon);
}

// " -12.345 " -> "-12.345" ; false if it is not a number
static bool number(const char *p, char *dst) {
  while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
  const char *end = p + strlen(p);
  while (end > p && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r' || end[-1] == '\n')) end--;

  byte n = 0, dots = 0;
  bool digit = false;
  if (*p == '-') dst[n++] = *p++;
  for (; p < end; p++) {
    if (*p >= '0' && *p <= '9') {
      if (n < 8) dst[n++] = *p;
      digit = true;
    } else if (*p == '.' && dots == 0) {
      if (n < 8) dst[n++] = *p;
      dots++;
    } else {
      return false;
    }
  }
  dst[n] = 0;
  return digit && p == end;
}

// "lat,lon" or "lat,lon,NAME"
static bool setCoords(char *s) {

  s = trim(s);
  char *c1 = strchr(s, ',');
  if (!c1) return false;
  *c1 = 0;
  char *c2 = strchr(c1 + 1, ',');
  if (c2) *c2 = 0;

  Location t;
  if (!number(s, t.lat) || !number(trim(c1 + 1), t.lon)) return false;
  if (abs(atoi(t.lat)) > 90 || abs(atoi(t.lon)) > 180) return false;

  byte n = 0;
  if (c2) {
    char *name = trim(c2 + 1);
    for (char *p = name; *p && n < sizeof(t.name) - 1; p++)
      if (*p >= 32 && *p < 127 && !(n == 0 && *p == ' ')) t.name[n++] = *p;
  }
  t.name[n] = 0;
  t.magic = LOC_MAGIC;

  loc = t;
  return true;
}

static bool apply(char *s) {

  bool ok;

  if ((*s >= '0' && *s <= '9') || *s == '-') {
    ok = setCoords(s);                              // 52.52,13.405,BERLIN
  } else {
    Serial.println(F("searching..."));
    Location t;
    ok = geocodeCity(s, t);                         // Berlin  -> name + coordinates from the web
    if (ok) loc = t;
  }

  if (ok) {
    EEPROM.put(0, loc);
    Serial.println(F("[LOCATION] Saved to EEPROM"));
  }
  return ok;
}

bool locationPoll() {

  while (Serial.available()) {
    char c = Serial.read();

    if (c == '\r' || c == '\n') {
      if (!cmdLen) continue;
      cmd[cmdLen] = 0;
      trim(cmd);
      cmdLen = 0;

      if (apply(cmd)) {
        Serial.print(loc.name);
        Serial.print(' ');
        Serial.print(loc.lat);
        Serial.print(',');
        Serial.println(loc.lon);
        return true;
      }
      Serial.println(F("[LOCATION] Invalid coordinates or city lookup failed"));

    } else if (cmdLen < sizeof(cmd) - 1) {
      cmd[cmdLen++] = c;
    }
  }
  return false;
}

void locationHelp() {
  Serial.print(loc.name);
  Serial.println(F(": send a city name, or lat,lon,NAME"));
}
