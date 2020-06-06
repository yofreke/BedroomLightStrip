#include <stdint.h>

/**
 * Wait this long between writes.
 * This is safety against errors or continued user input over-working the
 * eeprom.
 */
#define MIN_EEPROM_UPDATE_FREQUENCY_SECONDS 4
/**
 * Wait this long after eeprom needs write to execute write.
 * This avoids writing too early when given duration of user input.
 */
#define EEPROM_UPDATE_DELAY 2

void storage_clearAll();
void storage_writeValues();
void storage_readValues();
void storage_markNeedsUpdate();
void storage_tick();
