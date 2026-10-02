#include <EEPROM.h>
#include "config.h"
#include "location.h"
#include "geocode.h"

Location loc;
Location locations[MAX_LOCATIONS];
byte locationCount = 0;
byte displayedLocation = 0;

static void saveLocations() {
  EEPROM.put(0, locationCount);
  for (byte i = 0; i < locationCount; i++) {
    EEPROM.put((int)(1 + i * sizeof(Location)), locations[i]);
  }

  if (locationCount > 0) {
    loc = locations[displayedLocation];
  }
}

static void loadLocations() {
  locationCount = 0;

  byte storedCount = 0;
  EEPROM.get(0, storedCount);

  if (storedCount == 0 || storedCount > MAX_LOCATIONS) {
    storedCount = 0;
  }

  for (byte i = 0; i < storedCount; i++) {
    Location saved;
    EEPROM.get((int)(1 + i * sizeof(Location)), saved);
    if (saved.magic != LOC_MAGIC) break;
    locations[locationCount++] = saved;
  }

  if (locationCount == 0) {
    strcpy_P(loc.lat,  PSTR(DEFAULT_LAT));
    strcpy_P(loc.lon,  PSTR(DEFAULT_LON));
    strcpy_P(loc.name, PSTR(DEFAULT_NAME));
    loc.magic = LOC_MAGIC;
    if (MAX_LOCATIONS > 0) {
      locations[0] = loc;
      locationCount = 1;
    }
    Serial.println(F("[LOCATION] No saved locations; using defaults"));
  } else {
    loc = locations[0];
    Serial.println(F("[LOCATION] Loaded saved locations from EEPROM"));
  }

  displayedLocation = 0;
  if (locationCount > 0) {
    loc = locations[displayedLocation];
  }
}

static char cmd[32];
static byte cmdLen = 0;

static char *trim(char *s) {
  while (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n') s++;
  char *end = s + strlen(s);
  while (end > s && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r' || end[-1] == '\n')) *--end = 0;
  return s;
}

static bool appendLocation(const Location &t) {
  if (locationCount >= MAX_LOCATIONS) return false;
  locations[locationCount++] = t;
  displayedLocation = locationCount - 1;
  loc = t;
  saveLocations();
  return true;
}

bool locationDeleteByNumber(byte number) {
  if (number == 0 || number > locationCount) return false;

  byte idx = number - 1;
  for (byte i = idx; i + 1 < locationCount; i++) {
    locations[i] = locations[i + 1];
  }
  locationCount--;

  if (locationCount == 0) {
    displayedLocation = 0;
    saveLocations();
    return true;
  }

  if (displayedLocation >= locationCount) displayedLocation = locationCount - 1;
  loc = locations[displayedLocation];
  saveLocations();
  return true;
}

void locationLoad() {
  loadLocations();

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
    if (!appendLocation(loc)) {
      Serial.println(F("[LOCATION] List full; delete one before adding another"));
      return true;
    }
    Serial.print(F("[LOCATION] Added as #"));
    Serial.print(locationCount);
    Serial.print(' ');
    Serial.print(loc.name);
    Serial.print(' ');
    Serial.print(loc.lat);
    Serial.print(',');
    Serial.println(loc.lon);
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

      if (cmd[0] == 'D' || cmd[0] == 'd') {
        char *p = strchr(cmd, ':');
        if (!p) {
          Serial.println(F("[LOCATION] Delete format: D:2"));
          continue;
        }
        byte num = atoi(p + 1);
        if (locationDeleteByNumber(num)) {
          Serial.print(F("[LOCATION] Deleted #"));
          Serial.println(num);
          if (locationCount > 0) {
            Serial.print(F("[LOCATION] Current: #"));
            Serial.print(displayedLocation + 1);
            Serial.print(' ');
            Serial.print(loc.name);
            Serial.print(' ');
            Serial.print(loc.lat);
            Serial.print(',');
            Serial.println(loc.lon);
          }
          return true;
        }
        Serial.println(F("[LOCATION] Invalid delete index"));
        continue;
      }

      if (apply(cmd)) {
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
  Serial.println(F(": send a city name, or lat,lon,NAME; D:2 deletes slot 2"));
}
