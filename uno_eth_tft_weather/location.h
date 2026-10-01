// ==================================================
// location.h - current location, EEPROM storage, Serial Monitor commands
// ==================================================
#pragma once
#include <Arduino.h>

#define LOC_MAGIC 0xA5

struct Location {
  byte magic;
  char lat[9];
  char lon[9];
  char name[21];
};

extern Location loc;

void locationLoad();       // from EEPROM (or the defaults from config.h)
bool locationPoll();       // read Serial; true when a new location was just applied
void locationHelp();       // print usage
