#include <SPI.h>
#include <SD.h>
#include "config.h"
#include "display.h"
#include "sd_image.h"

#define CHUNK 70                        // pixels per SD read (Uno has only 2 KB RAM)

// low-level SD classes: smaller than SD.open()/File because no write code is linked in
static Sd2Card  card;
static SdVolume volume;
static SdFile   root, file;
static uint16_t buf[CHUNK];

bool sdBegin() {
  if (!card.init(SPI_HALF_SPEED, SD_CS)) {
    Serial.println(F("[SD] Card initialization failed"));
    return false;
  }
  if (!volume.init(card)) {
    Serial.println(F("[SD] Volume initialization failed"));
    return false;
  }
  if (!root.openRoot(volume)) {
    Serial.println(F("[SD] Could not open card root"));
    return false;
  }
  return true;
}

bool drawImage(const char *name, int16_t x, int16_t y, uint16_t w, uint16_t h) {

  if (!file.open(root, name, O_READ)) return false;

  for (uint16_t row = 0; row < h; row++) {
    for (uint16_t col = 0; col < w; col += CHUNK) {

      uint16_t n = w - col;
      if (n > CHUNK) n = CHUNK;

      // 1) read from SD while the TFT is deselected
      if (file.read(buf, n * 2) != (int)(n * 2)) { file.close(); return false; }

      // 2) file is big-endian, AVR is little-endian
      for (uint16_t i = 0; i < n; i++)
        buf[i] = (buf[i] << 8) | (buf[i] >> 8);

      // 3) push to the TFT
      tft.drawRGBBitmap(x + col, y + row, buf, n, 1);
    }
  }

  file.close();
  return true;
}
