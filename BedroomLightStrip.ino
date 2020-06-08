#include <EEPROM.h>
#include <Wire.h>
#include <ds3231.h>

#include "lcd_driver.h"

#include "globals.h"
#include "musicLights.h"
#include "pinMap.h"
#include "stateMachine.h"
#include "storage.h"
#include "wifiClient.h"

bool lastJoystickSwitchValue = false;
float menuAccumulatorX = 0.0f;
float menuAccumulatorY = 0.0f;

#define CONSUME_ACCUMULATOR(sourceAccumulator, destFloat)                      \
  (destFloat = sourceAccumulator; sourceAccumulator = 0.0f;)

// Different update rates for different states
#define LCD_UPDATE_INTERVAL_MS 100
#define LCD_EQUALIZER_VALUE_FACTOR 12.0f
volatile unsigned long lastLcdUpdateTimeMs = 0;

/**
   Normally the default UI is shown for a given mode.
   Navigation is enabled by pressing the joystick.
*/
bool drawNavigation = false;

unsigned long lastMenuInteractionTime;
#define LCD_DIM_DELAY_TIME 10000
#define LCD_FADE_TIME 10000

void redrawLcd()
{
  lastLcdUpdateTimeMs = g_milliseconds;

  // Update backlight strength
  // Fade over ten seconds, after first second.
  // Note: Timeing is bad because using system clock.
  unsigned long lastInteractionDiff = g_milliseconds - lastMenuInteractionTime;
  if (lastInteractionDiff > LCD_DIM_DELAY_TIME)
  {
    analogWrite(PIN_LCD_BRIGHTNESS,
                255.0f -
                    min(((float)(lastInteractionDiff - LCD_DIM_DELAY_TIME) /
                         (float)LCD_FADE_TIME) *
                            255.0f,
                        255.0f));
  }
  else
  {
    analogWrite(PIN_LCD_BRIGHTNESS, 255);
  }

  if (!drawNavigation)
  {
    //     Common header
    char line[21];
    sprintf(line, "[Mode: %s]", modeNames[currentMode]);
    lcdSetLine(0, line);

    if (currentMode == MODE_STARTUP)
    {
      lcdSetLine(1, " BedroomLightStrip ");
      lcdSetLine(2, " ...Starting Up...  ");
    }
    else if (currentMode == MODE_MUSIC)
    {
      // Display current music stuff

      // Equalizer
      float valueRed = ((float)lightsData[0].currentBrightness) / 255.0f *
                       LCD_EQUALIZER_VALUE_FACTOR;
      float valueGreen = ((float)lightsData[1].currentBrightness) / 255.0f *
                         LCD_EQUALIZER_VALUE_FACTOR;
      float valueBlue = ((float)lightsData[2].currentBrightness) / 255.0f *
                        LCD_EQUALIZER_VALUE_FACTOR;
      float valueWhite = ((float)lightsData[3].currentBrightness) / 255.0f *
                         LCD_EQUALIZER_VALUE_FACTOR;
      float valueWarmWhite = ((float)lightsData[4].currentBrightness) / 255.0f *
                             LCD_EQUALIZER_VALUE_FACTOR;

      lcdSetChar(0, 1, 'R');
      lcdShowBarGraph(1, 1, 18, '>', ' ', valueRed);

      lcdSetChar(0, 2, 'G');
      lcdShowBarGraph(1, 2, 18, '>', ' ', valueGreen);

      lcdSetChar(0, 3, 'B');
      lcdShowBarGraph(1, 3, 18, '>', ' ', valueBlue);

      lcdSetChar(18, 0, valueWhite > 0.1f ? '!' : ' ');
      lcdSetChar(19, 0, valueWarmWhite > 0.1f ? '!' : ' ');
    }
    else if (currentMode == MODE_SUNRISE)
    {
      // FIXME: Replace with real time from time module
      lcdSetLine(1, (String("Time: ") + g_rtcTimestamp.hour + ':' +
                     g_rtcTimestamp.min + ':' + g_rtcTimestamp.sec)
                        .c_str());

      //       lcdShowBarGraph(=1, 2, 18, 'X', ' ',
      //       (float)lightsData[3].currentBrightness / 255.0f);
      //       lcdShowBarGraph(1, 3, 18, 'X', ' ',
      //       (float)lightsData[4].currentBrightness / 255.0f);
      lcdSetChars(0, 2, "W :");
      lcdShowBarGraph(3, 2, 16, '+', ' ',
                      modeData_sunrise_brightnessWhite / 255.0f);

      lcdSetChars(0, 3, "WW:");
      lcdShowBarGraph(3, 3, 16, '+', ' ',
                      modeData_sunrise_brightnessWarmWhite / 255.0f);

      lcdSetChar(19, 2, modeData_sunrise_menuSelection == 0 ? '<' : ' ');
      lcdSetChar(19, 3, modeData_sunrise_menuSelection == 1 ? '<' : ' ');
    }
  }
  else
  {
    // Draw navigation menu
  }
}

void sunrise_loop()
{
  //  analogWrite(PIN_LED_WHITE,250);
  //  analogWrite(PIN_LED_WARMWHITE, 250);

  //  analogWrite(PIN_LED_WHITE, 25);
  //  analogWrite(PIN_LED_WARMWHITE, 250);

  analogWrite(PIN_LED_WHITE, modeData_sunrise_brightnessWhite);
  analogWrite(PIN_LED_WARMWHITE, modeData_sunrise_brightnessWarmWhite);

  //  delay(100);
}

struct MenuItem
{
  char title[16];

  //  struct MenuItem *parentItem;

  //  LinkedList<struct MenuItem*> *childItems;
};
//
// MenuItem selectMode_menuItem = {
//  "Select Mode",
//  NULL,
//  NULL
//};

// MenuItem root_menuItem = {
//  "/",
//  NULL,
//  NULL
//};

struct MenuItem root_menuItem = {"/"};

// Add children references
// root_menuItem.childItems = (struct *MenuItem){ &selectMode_menuItem };
// root_menuItem.title = "asdf";

// Add parent references
// selectMode_menuItem.parentItem = &root_menuItem;

// struct MenuItem* currentItem;

inline float scaleJoystickValue(float n)
{
  float scaledValue = (n - 512.0f) / 512.0f;
  // High pass filter
  if (abs(scaledValue) <= 0.05f)
  {
    return 0.0f;
  }
  return pow(scaledValue, 2.0f) * (scaledValue < 0.0f ? -1.0f : 1.0f);
}

void tick_menu()
{
  int valueX = analogRead(PIN_JOYSTICK_X);
  // this small pause is needed between reading
  // analog pins, otherwise we get the same value twice
  delay(1);
  int valueY = analogRead(PIN_JOYSTICK_Y);
  delay(1);
  // Default switch value is 1
  bool valueSwitch = digitalRead(PIN_JOYSTICK_SWITCH) != 1;

  //  Serial.print(valueX);
  //  Serial.print(" ");
  //    Serial.print(valueY);
  //  Serial.print(" ");
  //  Serial.print(valueSwitch);
  //    Serial.println(" ");

  // Value from -1 to +1
  // Map joystick values to values that make sense in our project
  float scaledY = scaleJoystickValue(valueX);
  float scaledX = scaleJoystickValue(valueY) * -1;

  if (scaledX == 0 && scaledY == 0 && !valueSwitch)
  {
    return;
  }
  lastMenuInteractionTime = g_milliseconds;

  menuAccumulatorX += scaledX;
  menuAccumulatorY += scaledY;

  //    Serial.print(menuAccumulatorX);
  //    Serial.print(" ");
  //  Serial.print(menuAccumulatorY);
  //    Serial.println(" ");

  if (valueSwitch != lastJoystickSwitchValue && !valueSwitch)
  {
    //    FIXME: In future this should go to navigation menu
    //    drawNavigation = true;

    switch (currentMode)
    {
    case MODE_SUNRISE:
      stateMachine_setMode(MODE_MUSIC);
      break;
    case MODE_MUSIC:
      stateMachine_setMode(MODE_SUNRISE);
      break;
    }
  }

  /** Note: 0.99f is the limit. This is necessary with a 100k logarithmic
   * potentiometer. */
#define MENU_SUNRISE_Y_THRESHOLD 0.99f

  // Per mode draw settings
  if (currentMode == MODE_SUNRISE)
  {
    // Toggle selected value
    if (scaledY < -MENU_SUNRISE_Y_THRESHOLD)
    {
      modeData_sunrise_menuSelection = 0;
    }
    else if (scaledY > MENU_SUNRISE_Y_THRESHOLD)
    {
      modeData_sunrise_menuSelection = 1;
    }

#define MENU_SUNRISE_X_FACTOR 0.75f

    if (menuAccumulatorX != 0)
    {
      // Smooth scaling of brightness
      if (!modeData_sunrise_menuSelection)
      {
        modeData_sunrise_brightnessWhite +=
            menuAccumulatorX * MENU_SUNRISE_X_FACTOR;

        if (modeData_sunrise_brightnessWhite < 0)
        {
          modeData_sunrise_brightnessWhite = 0;
        }
        else if (modeData_sunrise_brightnessWhite > 250)
        {
          modeData_sunrise_brightnessWhite = 250;
        }

        //      Serial.println(modeData_sunrise_brightnessWhite);
      }
      else
      {
        modeData_sunrise_brightnessWarmWhite +=
            menuAccumulatorX * MENU_SUNRISE_X_FACTOR;

        if (modeData_sunrise_brightnessWarmWhite < 0)
        {
          modeData_sunrise_brightnessWarmWhite = 0;
        }
        else if (modeData_sunrise_brightnessWarmWhite > 250)
        {
          modeData_sunrise_brightnessWarmWhite = 250;
        }

        //      Serial.println(modeData_sunrise_brightnessWarmWhite);
      }

      // Current accumulator has been applied to the correct light value,
      // clear for next loop.
      menuAccumulatorX = 0;

      // Save updated light value to eeprom
      storage_markNeedsUpdate();
    }
  }

  lastJoystickSwitchValue = valueSwitch;
}

void setup()
{
  // Debug console
  Serial.begin(9600);

  // LCD
  pinMode(PIN_LCD_BRIGHTNESS, OUTPUT);
  analogWrite(PIN_LCD_BRIGHTNESS, 255);

  lcdSetup();

  // LED brightness output
  pinMode(PIN_LED_RED, OUTPUT);
  pinMode(PIN_LED_GREEN, OUTPUT);
  pinMode(PIN_LED_BLUE, OUTPUT);
  pinMode(PIN_LED_WHITE, OUTPUT);
  pinMode(PIN_LED_WARMWHITE, OUTPUT);

  analogWrite(PIN_LED_RED, 0);
  analogWrite(PIN_LED_GREEN, 0);
  analogWrite(PIN_LED_BLUE, 0);
  analogWrite(PIN_LED_WHITE, 0);
  analogWrite(PIN_LED_WARMWHITE, 0);

  // Mic input
  setFreeRunMode();

  // Joystick input
  pinMode(PIN_JOYSTICK_X, INPUT);
  pinMode(PIN_JOYSTICK_Y, INPUT);
  pinMode(PIN_JOYSTICK_SWITCH, INPUT_PULLUP);

  pinMode(PIN_CLOCK_DEBUG, OUTPUT);

  // Real time clock input
  DS3231_init(0);

  /**
     Set to `1` to update the saved time in the real time clock.
     Ensure time below is reasonable during flash to ensure somewhat accurate
     starting time.
  */
#define SET_REAL_TIME_CLOCK 0

#if SET_REAL_TIME_CLOCK
  g_rtcTimestamp.hour = 15;
  g_rtcTimestamp.min = 5;
  g_rtcTimestamp.sec = 30;
  g_rtcTimestamp.mday = 5;
  g_rtcTimestamp.mon = 6;
  g_rtcTimestamp.year = 2020;

  DS3231_set(g_rtcTimestamp);

  Serial.println("Real time clock updated");
#endif

  // Reset the saved data if the user holds down joystick during boot
  bool resetEepromData = digitalRead(PIN_JOYSTICK_SWITCH) != 1;

  if (resetEepromData)
  {
    // Clear all existing data
    storage_clearAll();

    // Write default values
    storage_writeValues();
  }
  else
  {
    // Restore previous values
    storage_readValues();
  }

  wifiClient_setup();

  // Start app
  for (int i = 0; i < 25; i++)
  {
    analogWrite(PIN_LCD_BRIGHTNESS, i * 10);
    delay(10);
  }

  //  stateMachine_setMode(MODE_MUSIC);
  stateMachine_setMode(MODE_SUNRISE);
}

void loop()
{
  while (1)
  { // reduces jitter

    DS3231_get(&g_rtcTimestamp);
    g_rtcSeconds = get_unixtime(g_rtcTimestamp);

    g_milliseconds = millis();

    //  FIXME: Use ModeData to structure name/loop/etc
    if (currentMode == MODE_MUSIC)
    {
      musicLights_tick();
    }
    else if (currentMode == MODE_SUNRISE)
    {
      sunrise_loop();
    }

    tick_menu();

    //    Serial.print("lastLcdUpdateTimeMs= ");
    //    Serial.println(lastLcdUpdateTimeMs);

    if (abs(g_rtcSeconds - lastLcdUpdateTimeMs) > LCD_UPDATE_INTERVAL_MS)
    {
      redrawLcd();
      // FIXME: Lol naming
      lcdUpdate();

      // Wifi gets lower frequency updates to help with jitter
      wifiClient_tick();
    }

    storage_tick();
  }
}

void setFreeRunMode()
{
  //  TIMSK0 = 0;                               // turn off timer0 for lower
  //  jitter ADCSRA = 0xE5;                            // "ADC Enable", "ADC
  //  Start Conversion", "ADC Auto Trigger Enable", 32 prescaler for 38.5 KHz
  //                                            // 0xE4, 16 prescaler
  //                                            // 0xE6, 64 prescaler
  //  ADMUX = 0x40;                             // b0100 0000; use adc0,
  //  right-align, use the full 10 bits resolution DIDR0 = 0x01; // turn off
  //  the digital input for adc0

  PORTB = 0; // 1 for hight
  //  DDRB = (1<<R1)|(1<<R2)|(1<<R3); //1 for output

  // Note: This will also cause `millis()` and `delay()` calls to stop
  // working.
  //  TIMSK0 = 0; // turn off timer0 for lower jitter

  // ADCSRA = 0xC5;//0xe5; // set the adc to free running mode
  DIDR0 = 0x39; // turn off the digital input for adc0,3,4,5
}

// #region
/**
   Copied from d23231.c
   Ideally we fork that and make this helper available with a difference
   preprocessor flag.
*/

#define xpgm_read_byte(addr) (*(const uint8_t *)(addr))

const uint8_t days_in_month[12] PROGMEM = {31, 28, 31, 30, 31, 30,
                                           31, 31, 30, 31, 30, 31};

// returns the number of seconds since 01.0ti1.1970 00:00:00 UTC, valid for
// 2000..FIXME
uint32_t get_unixtime(struct ts t)
{
  uint8_t i;
  uint16_t d;
  int16_t y;
  uint32_t rv;

  if (t.year >= 2000)
  {
    y = t.year - 2000;
  }
  else
  {
    return 0;
  }

  d = t.mday - 1;
  for (i = 1; i < t.mon; i++)
  {
    d += xpgm_read_byte(days_in_month + i - 1);
  }
  if (t.mon > 2 && y % 4 == 0)
  {
    d++;
  }
  // count leap days
  d += (365 * y + (y + 3) / 4);
  rv = ((d * 24UL + t.hour) * 60 + t.min) * 60 + t.sec +
       SECONDS_FROM_1970_TO_2000;
  return rv;
}

// #endregion
