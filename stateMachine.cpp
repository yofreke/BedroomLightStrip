#include "stateMachine.h"
#include "lcd_driver.h"
#include "pinMap.h"

char *modeNames[] = {"Startup", "Music", "Sunrise"};

/** white=0 and warmWhite=20 is a gentle place to start */
float modeData_sunrise_brightnessWhite = 0.0f;
float modeData_sunrise_brightnessWarmWhite = 20.0f;

/** 0=W; 1=WW; */
byte modeData_sunrise_menuSelection = 0;

byte currentMode = MODE_STARTUP;

void stateMachine_setMode(int newMode)
{
  //  Serial.print("stateMachine_setMode: newMode= ");
  //  Serial.println(newMode);

  currentMode = newMode;

  // Clear LCD between mode changes to ensure clean start
  lcdClear();

  // Fade all lights to 0
  float valueRed = analogRead(PIN_LED_RED);
  delay(1);
  float valueGreen = analogRead(PIN_LED_GREEN);
  delay(1);
  float valueBlue = analogRead(PIN_LED_BLUE);
  delay(1);
  float valueWhite = analogRead(PIN_LED_WHITE);
  delay(1);
  float valueWarmWhite = analogRead(PIN_LED_WARMWHITE);
  delay(1);

#define TRANSITION_FADE_FACTOR 0.9f

  for (int i = 0; i < 50; i++)
  {
    analogWrite(PIN_LED_RED, valueRed *= TRANSITION_FADE_FACTOR);
    analogWrite(PIN_LED_GREEN, valueGreen *= TRANSITION_FADE_FACTOR);
    analogWrite(PIN_LED_BLUE, valueBlue *= TRANSITION_FADE_FACTOR);
    analogWrite(PIN_LED_WHITE, valueWhite *= TRANSITION_FADE_FACTOR);
    analogWrite(PIN_LED_WARMWHITE, valueWarmWhite *= TRANSITION_FADE_FACTOR);

    delay(3);
  }

  analogWrite(PIN_LED_RED, 0);
  analogWrite(PIN_LED_GREEN, 0);
  analogWrite(PIN_LED_BLUE, 0);
  analogWrite(PIN_LED_WHITE, 0);
  analogWrite(PIN_LED_WARMWHITE, 0);
}
