#include <Adafruit_GFX.h>
#include "config.h"
#include "sd_image.h"
#include "display.h"

Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);

// full-screen scenes on the SD card, index = see displayWeather()
const char SCENES[9][12] PROGMEM = {
  "CLRDAY.BIN", "CLRNITE.BIN", "PTLDAY.BIN", "PTLNITE.BIN",
  "OVCDAY.BIN", "OVCNITE.BIN", "RAIN.BIN",   "SNOW.BIN",   "STORM.BIN"
};

const char DIRS[8][3] PROGMEM = { "N", "NE", "E", "SE", "S", "SW", "W", "NW" };

void displayInit() {
  tft.init(240, 280);
  tft.setRotation(1);                           // landscape 280 x 240
  tft.cp437(true);                              // char 0xF8 = degree sign
}

void displayMessage(const __FlashStringHelper *a, const __FlashStringHelper *b) {
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(3);
  tft.setCursor(20, 90);  tft.print(a);
  tft.setCursor(20, 130); tft.print(b);
}

void displayStale() {
  tft.fillRect(268, 4, 8, 8, ST77XX_RED);
}

// text with a black outline (readable on any background)
static void outlined(int16_t x, int16_t y, uint8_t size, const char *s) {
  tft.setTextSize(size);
  tft.setTextColor(ST77XX_BLACK);
  for (byte i = 0; i < 4; i++) {
    tft.setCursor(x + ((i & 1) ? 1 : -1), y + ((i & 2) ? 1 : -1));
    tft.print(s);
  }
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(x, y);
  tft.print(s);
}

// true if the text contains any digit 1..9  ("0", "0.0", "0.00" -> false)
static bool nonZero(const char *s) {
  for (; *s; s++) if (*s >= '1' && *s <= '9') return true;
  return false;
}

void displayWeather(const Weather &w, const Location &l) {

  // temperature rounded to a whole number, from the text
  int t = atoi(w.temp);
  const char *dot = strchr(w.temp, '.');
  if (dot && dot[1] >= '5') t += (w.temp[0] == '-') ? -1 : 1;

  int  cloud    = atoi(w.cloud);
  bool isDay    = (w.isday[0] == '1');
  bool precip   = nonZero(w.rain);
  bool snow     = precip && t <= 0;
  bool rain     = precip && !snow;
  bool overcast = cloud >= 75;
  bool partly   = cloud >= 25 && !overcast;
  bool storm    = rain && atoi(w.wind) >= 40;

  // ---- scene: sky, sun/moon, clouds, land and precipitation are all in one picture ----
  byte i;
  if      (storm) i = 8;
  else if (snow)  i = 7;
  else if (rain)  i = 6;
  else            i = (overcast ? 4 : partly ? 2 : 0) + (isDay ? 0 : 1);

  char name[12];
  strcpy_P(name, SCENES[i]);
  if (!drawImage(name, 0, 0, 280, 240)) tft.fillScreen(ST77XX_BLACK);

  // ---- right column: temperature, place name, coordinates ----
  char line[32];
  char num[8];

  itoa(t, num, 10);
  uint8_t ts = (strlen(num) > 2) ? 4 : 5;       // "-12" does not fit at size 5
  outlined(158, 20, ts, num);

  outlined(158, 74, (strlen(l.name) > 10) ? 1 : 2, l.name);   // long names: small font

  outlined(158, 96, 2, l.lat);                  // coordinates, e.g. 52.5243 / 13.4105
  outlined(158, 116, 2, l.lon);

  // ---- detail rows (bottom), built without printf ----
  strcpy_P(line, PSTR("HUM "));
  strcat(line, w.hum);
  strcat_P(line, PSTR("%  CLOUD "));
  strcat(line, w.cloud);
  strcat_P(line, PSTR("%"));
  outlined(8, 172, 2, line);

  strcpy_P(line, PSTR("WIND "));
  strcat(line, w.wind);
  strcat_P(line, PSTR(" km/h "));
  strcat_P(line, DIRS[((atoi(w.wdir) + 22) / 45) & 7]);
  outlined(8, 196, 2, line);

  strcpy(line, w.press);                        // whole hPa
  char *p = strchr(line, '.');
  if (p) *p = 0;
  strcat_P(line, PSTR(" hPa  RAIN "));
  strcat(line, w.rain);
  strcat_P(line, PSTR(" mm"));
  outlined(8, 220, 2, line);
}
