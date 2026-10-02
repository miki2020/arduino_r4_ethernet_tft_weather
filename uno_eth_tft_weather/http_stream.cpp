#include <SPI.h>
#include <Ethernet.h>
#include "config.h"
#include "http_stream.h"

static EthernetClient client;
static byte mac[] = { NET_MAC };
static IPAddress ip(NET_IP);
static char responseBuf[WHOLE_HTTP_LIMIT];
static size_t responseLen = 0;
static size_t responsePos = 0;
static bool responseBuffered = false;

void netBegin() {
  Ethernet.begin(mac, ip);
  Serial.print(F("[NET] Static IP: "));
  Serial.println(Ethernet.localIP());
}

bool httpOpen(PGM_P host) {
  char h[32];
  strcpy_P(h, host);
  Serial.print(F("[NET] Connecting to ")); Serial.println(h);
  if (client.connect(h, 80)) return true;
  Serial.println(F("[NET] Connection failed"));
  return false;
}

// flash -> socket in 32 byte pieces (print(F()) would send one byte per packet)
void httpSendP(PGM_P text) {
  char b[32];
  byte n = 0;
  char c;
  while ((c = pgm_read_byte(text++))) {
    b[n++] = c;
    if (n == sizeof(b)) { client.write((uint8_t *)b, n); n = 0; }
  }
  if (n) client.write((uint8_t *)b, n);
}

void httpSend(const char *text) {
  client.write((const uint8_t *)text, strlen(text));
}

// HTTP/1.0 => no chunked encoding, the body is a plain stream
void httpEnd(PGM_P host) {
  httpSendP(PSTR(" HTTP/1.0\r\nHost: "));
  httpSendP(host);
  httpSendP(PSTR("\r\nConnection: close\r\n\r\n"));
}

void httpClose() {
  responseBuffered = false;
  responseLen = 0;
  responsePos = 0;
  client.stop();
}

bool httpBufferResponse() {
  responsePos = 0;
  responseLen = 0;
  responseBuffered = false;

  unsigned long t = millis();
  while ((client.connected() || client.available()) && millis() - t < 8000UL) {
    while (client.available()) {
      int ch = client.read();
      if (ch < 0) break;
      if (responseLen < sizeof(responseBuf) - 1) {
        responseBuf[responseLen++] = (char)ch;
      } else {
        responseBuf[sizeof(responseBuf) - 1] = 0;
        responseBuffered = true;
        return true;
      }
    }
    if (!client.connected() && !client.available()) break;
  }

  responseBuf[responseLen] = 0;
  responseBuffered = responseLen > 0;
  return responseBuffered;
}

// ---------------- stream reading ----------------

// one byte from the socket, -1 on timeout / closed
static int readCh() {
  if (responseBuffered) {
    if (responsePos >= responseLen) return -1;
    return (unsigned char)responseBuf[responsePos++];
  }

  unsigned long t = millis();
  while (millis() - t < 8000UL) {
    if (client.available()) return client.read();
    if (!client.connected() && !client.available()) return -1;
  }
  return -1;
}

bool streamSeek(PGM_P key) {
  byte pos = 0;
  while (pgm_read_byte(key + pos)) {
    int ch = readCh();
    if (ch < 0) return false;
    if (ch == (char)pgm_read_byte(key + pos)) pos++;
    else pos = (ch == (char)pgm_read_byte(key)) ? 1 : 0;
  }
  return true;
}

bool streamNumber(PGM_P key, char *dst, byte size) {

  byte n = 0;
  dst[0] = 0;

  if (!streamSeek(key)) return false;

  while (true) {
    int ch = readCh();
    if (ch < 0) break;
    bool digit = (ch >= '0' && ch <= '9');

    if (n == 0) {                                   // skip spaces until the number starts
      if (digit || ch == '-') dst[n++] = ch;
    } else if (digit || ch == '.') {
      if (n < size - 1) dst[n++] = ch;
    } else {
      break;
    }
  }

  dst[n] = 0;
  return n > 0;
}

bool streamString(PGM_P key, char *dst, byte size) {

  byte n = 0;
  int ch;
  dst[0] = 0;

  if (!streamSeek(key)) return false;

  do { ch = readCh(); } while (ch == ' ');
  if (ch != '"') return false;

  while ((ch = readCh()) != '"') {
    if (ch < 0) return false;
    if (ch >= 0xC3 && ch <= 0xC5) {
      int c2 = readCh();
      if (c2 < 0) return false;
      if (c2 < 0x80 || c2 > 0xBF) continue;
      // Skip non-ASCII UTF-8 letters; the stock default font is ASCII-only.
      continue;
    }
    if (ch >= 32 && ch < 127 && n < size - 1) dst[n++] = ch;
  }

  dst[n] = 0;
  return n > 0;
}
