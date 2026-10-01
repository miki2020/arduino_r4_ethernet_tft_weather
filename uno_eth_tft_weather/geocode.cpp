#include "http_stream.h"
#include "geocode.h"

static const char HOST[]   PROGMEM = "geocoding-api.open-meteo.com";
static const char K_NAME[] PROGMEM = "\"name\":";
static const char K_LAT[]  PROGMEM = "\"latitude\":";
static const char K_LON[]  PROGMEM = "\"longitude\":";

// percent-encode for the URL (space -> %20, UTF-8 bytes -> %XX)
static void encode(const char *s, char *out, byte size) {
  byte n = 0;
  for (; *s && n < size - 4; s++) {
    byte c = *s;
    if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')) {
      out[n++] = c;
    } else {
      out[n++] = '%';
      out[n++] = "0123456789ABCDEF"[c >> 4];
      out[n++] = "0123456789ABCDEF"[c & 15];
    }
  }
  out[n] = 0;
}

bool geocodeCity(const char *city, Location &out) {

  char enc[64];
  encode(city, enc, sizeof(enc));

  if (!httpOpen(HOST)) return false;

  httpSendP(PSTR("GET /v1/search?count=1&name="));
  httpSend(enc);
  httpEnd(HOST);

  // first result: "name", "latitude", "longitude" arrive in this order
  bool ok = streamString(K_NAME, out.name, sizeof(out.name))
         && streamNumber(K_LAT,  out.lat,  sizeof(out.lat))
         && streamNumber(K_LON,  out.lon,  sizeof(out.lon));

  httpClose();
  out.magic = LOC_MAGIC;
  return ok;
}
