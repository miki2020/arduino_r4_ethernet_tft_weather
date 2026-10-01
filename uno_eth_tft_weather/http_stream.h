// ==================================================
// http_stream.h - minimal HTTP/1.0 client that reads the reply as a stream
// ==================================================
#pragma once
#include <Arduino.h>

void netBegin();                                   // start Ethernet (static IP)

bool httpOpen(PGM_P host);                         // connect to host:80 (host is in flash)
void httpSendP(PGM_P text);                        // send text stored in flash
void httpSend(const char *text);                   // send text stored in RAM
void httpEnd(PGM_P host);                          // finish the request line + headers
void httpClose();

bool streamSeek(PGM_P key);                        // consume stream up to and including key
bool streamNumber(PGM_P key, char *dst, byte size);   // key, then a number, kept as text
bool streamString(PGM_P key, char *dst, byte size);   // key, then "text", ASCII only
