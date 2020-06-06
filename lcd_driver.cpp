#include <Arduino.h>

#include <LiquidCrystal_PCF8574.h>

#include "lcd_driver.h"

LiquidCrystal_PCF8574 lcd(0x3F);
char lineBuffer[21 * 4];

void lcdSetup() {
  lcd.begin(20, 4);
  lcd.setBacklight(255);
  lcd.clear();
  lcd.noCursor();
  lcd.noBlink();
  lcd.noAutoscroll();
  lcd.setCursor(0, 1);
  lcd.print(" BedroomLightStrip ");
}

void lcdClear() {
  for (int i = 0; i < 21 * 4; i++) {
    lineBuffer[i] = ' ';
  }
}

inline void copyChars(char *source, int sourceStartIndex, char *dest, int destStartIndex, bool padWithSpaces) {
  char c;
  bool fillWithSpaces = false;

//  Serial.println(String("sourceStartIndex=") + sourceStartIndex + " destStartIndex= " + destStartIndex);

  for (int i = 0; i < 21; i++) {
    c = source[sourceStartIndex + i];

    if (c == '\0') {
      // At end of string, done now
      if (!padWithSpaces) {
        return;
      }
      // At end of string, fill remainder of line with spaces
      fillWithSpaces = true;
    }
    if (fillWithSpaces) {
      c = ' ';
    }

    dest[destStartIndex + i] = c;
  }
}

void lcdSetChar(int columnIndex, int rowIndex, char c) {
  lineBuffer[rowIndex * 21 + columnIndex] = c;
}

void lcdSetChars(int columnIndex, int rowIndex, char *c) {
  int lineBufferIndex = rowIndex * 21 + columnIndex;
  copyChars(c, 0, lineBuffer, lineBufferIndex, false);
}

void lcdSetLine(byte rowIndex, char *line) {
  int lineBufferIndex = rowIndex * 21;
  copyChars(line, 0, lineBuffer, lineBufferIndex, true);
}

void lcdShowBarGraph(int columnIndex, int rowIndex, int charWidth, char activeChar, char inactiveChar, float value) {
  int startIndex = rowIndex * 21 + columnIndex;
  float bucketSize = 1.0f / charWidth;
  float bucketLimit;
  for (int i = 0; i < charWidth; i++) {
    bucketLimit = (float)(i + 1)  * bucketSize;
    lineBuffer[startIndex + i] = value > bucketLimit ? activeChar : inactiveChar;
  }
}

byte drawRowIndex = 0;
char drawRowBuffer[21];

void lcdUpdate() {
  lcd.setCursor(0, drawRowIndex);
  copyChars(lineBuffer, drawRowIndex * 21, drawRowBuffer, 0, false);
  drawRowBuffer[20] = '\0';
  lcd.print(drawRowBuffer);

  drawRowIndex++;
  if (drawRowIndex == 4) {
    drawRowIndex = 0;
  }
}
