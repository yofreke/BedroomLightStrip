#include <stdint.h>

#include <EEPROM.h>
#include <Wire.h>

#include "globals.h"
#include "stateMachine.h"
#include "storage.h"

static const int EEPROM_MAGIC_NUMBER = 72;

unsigned long lastEepromUpdateSeconds = 0;

unsigned long eepromNeedsUpdateAtSeconds = 0;

void storage_clearAll()
{
  Serial.println("Clearing eeprom...");
  for (int i = 0; i < EEPROM.length(); i++)
  {
    EEPROM.update(i, 0);
  }
}

void storage_readValues()
{
  int eeAddress = 0;

  // // int: magic number
  // int magicNumber;
  // EEPROM.get(eeAddress, magicNumber);
  // eeAddress += sizeof(int);

  // if (magicNumber != EEPROM_MAGIC_NUMBER)
  // {
  //   Serial.println("ERROR: Eeprom magic number does not match expected");
  //   Serial.println(magicNumber);
  //   return;
  // }

  // // float: sunrise white
  // EEPROM.get(eeAddress, modeData_sunrise_brightnessWhite);
  // eeAddress += sizeof(float);

  // // float: sunrise warm white
  // EEPROM.get(eeAddress, modeData_sunrise_brightnessWarmWhite);
}

void storage_writeValues()
{
  Serial.println("writeEepromValues()");

  int eeAddress = 0;

  // // int: magic number
  // EEPROM.put(eeAddress, EEPROM_MAGIC_NUMBER);
  // eeAddress += sizeof(int);

  // // float: sunrise white
  // EEPROM.put(eeAddress, modeData_sunrise_brightnessWhite);
  // eeAddress += sizeof(float);

  // // float: sunrise warm white
  // EEPROM.put(eeAddress, modeData_sunrise_brightnessWarmWhite);
}

void storage_markNeedsUpdate() { eepromNeedsUpdateAtSeconds = g_rtcSeconds; }

void storage_tick()
{
  if (eepromNeedsUpdateAtSeconds > 0 &&
      g_rtcSeconds - eepromNeedsUpdateAtSeconds > EEPROM_UPDATE_DELAY &&
      g_rtcSeconds - lastEepromUpdateSeconds >
          MIN_EEPROM_UPDATE_FREQUENCY_SECONDS)
  {
    storage_writeValues();

    lastEepromUpdateSeconds = g_rtcSeconds;
    eepromNeedsUpdateAtSeconds = 0;
  }
}
