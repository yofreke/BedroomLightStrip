#include <Wire.h>

#include "lcd_driver.h"

#define PIN_LED_RED 2
#define PIN_LED_GREEN 3
#define PIN_LED_BLUE 4
#define PIN_LED_WHITE 5
#define PIN_LED_WARMWHITE 6

#define PIN_JOYSTICK_X A2
#define PIN_JOYSTICK_Y A3
#define PIN_JOYSTICK_SWITCH 26

#define PIN_CLOCK_DEBUG 50

#define OCTAVE 1 // use the octave output function
//#define LOG_OUT 1             // use the log output function
#define FHT_N 256 // set to 256 point fht

int noise[] = {90, 125, 147, 143, 128, 123, 104, 89}; //just test output without in silence, and use values from ocatve bins

#include <FHT.h> // include the library

#define MODE_STARTUP 0
#define MODE_MUSIC 1
#define MODE_SUNRISE 2
#define MODE_MENU 3

byte currentMode = MODE_STARTUP;

//#define AVG_SAMPLE_COUNT 100
#define AVG_SAMPLE_COUNT 10

//#define BRIGHTNESS_SMOOTH_FACTOR_UP 0.15f
//#define BRIGHTNESS_SMOOTH_FACTOR_DOWN 0.08f
#define BRIGHTNESS_SMOOTH_FACTOR_UP 0.25f
#define BRIGHTNESS_SMOOTH_FACTOR_DOWN 0.15f

typedef struct LightMapping {
  byte ledPin;
  byte octave;

  float factor;
  float power;
};

typedef struct LightMappingData {
  float average;

  float targetBrightness;
  float currentBrightness;
};

double approxRollingAverage (double avg, double new_sample) {
  avg -= avg / AVG_SAMPLE_COUNT;
  avg += new_sample / AVG_SAMPLE_COUNT;

  return avg;
}

// Note: Average will go from resting to near zero when activated
// Octave | Resting Average | Response Range Hz
// 0 | 55 | 50 Hz - 200 Hz
// 1 | 45 | 50 Hz - 200 Hz
// 2 | 550 | 210 Hz - 1500 Hz
// 3 | 550 | 950 Hz - 2800 Hz
// 4 | 575 | 2200 Hz - 5000 Hz
// 5 | 600 | 4800 Hz - 11000 Hz
// 6 | 610 | 11000 Hz - ? Hz
// 7 | 300 (noisey) | ? Hz
#define OCTAVE_REST_AVERAGE_0 50
#define OCTAVE_REST_AVERAGE_1 50
#define OCTAVE_REST_AVERAGE_2 550
#define OCTAVE_REST_AVERAGE_3 550
#define OCTAVE_REST_AVERAGE_4 575
#define OCTAVE_REST_AVERAGE_5 600
#define OCTAVE_REST_AVERAGE_6 610
#define OCTAVE_REST_AVERAGE_7 300

int OCTAVE_REST_AVERAGES[8] = { OCTAVE_REST_AVERAGE_0, OCTAVE_REST_AVERAGE_1, OCTAVE_REST_AVERAGE_2, OCTAVE_REST_AVERAGE_3, OCTAVE_REST_AVERAGE_4, OCTAVE_REST_AVERAGE_5, OCTAVE_REST_AVERAGE_6, OCTAVE_REST_AVERAGE_7 };

LightMapping lights[5] = {
  { .ledPin = PIN_LED_RED, .octave = 2, .factor = 2.6f, .power = 5.0f },
  { .ledPin = PIN_LED_GREEN, .octave = 4, .factor = 1.8f, .power = 4.0f },
  { .ledPin = PIN_LED_BLUE, .octave = 5, .factor = 1.15f, .power = 3.0f },
  { .ledPin = PIN_LED_WARMWHITE, .octave = 6, .factor = 1.5f, .power = 12.0f },
  { .ledPin = PIN_LED_WHITE, .octave = 7, .factor = 1.15f, .power = 10.0f }
};

LightMappingData lightsData[5] = {
  { .average = 0.0f, .targetBrightness = 0.0f, .currentBrightness = 0.0f },
  { .average = 0.0f, .targetBrightness = 0.0f, .currentBrightness = 0.0f },
  { .average = 0.0f, .targetBrightness = 0.0f, .currentBrightness = 0.0f },
  { .average = 0.0f, .targetBrightness = 0.0f, .currentBrightness = 0.0f },
  { .average = 0.0f, .targetBrightness = 0.0f, .currentBrightness = 0.0f }
};

float lowPassFilters[5] = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
float globalLowPassFilter = 0.0f;

byte lightsCount = sizeof(lights) / sizeof(LightMapping);

#define FLAG_LOW_PASS false
#define FLAG_NORMALIZE_BRIGHTNESS false

#define MUSIC_MODE_VALUE_FACTOR 8.0f

bool lastJoystickSwitchValue = false;
float menuAccumulatorX = 0.0f;
float menuAccumulatorY = 0.0f;

#define CONSUME_ACCUMULATOR(sourceAccumulator, destFloat) (destFloat = sourceAccumulator; sourceAccumulator = 0.0f;)

void music_loop() {
  cli();  // UDRE interrupt slows this way down on arduino1.0
  for (int i = 0 ; i < FHT_N ; i++) { // save 256 samples
    int k = analogRead(0);
    k -= 0x0200; // form into a signed int
    k <<= 6; // form into a 16b signed int
    fht_input[i] = k; // put real data into bins
  }
  fht_window(); // window the data for better frequency response
  fht_reorder(); // reorder the data before doing the fht
  fht_run(); // process the data in the fht
  fht_mag_octave();
  sei();

  // End of Fourier Transform code - output is stored in fht_oct_out[i].

  float maxBrightness = 0.0f;

  for (int i = 0; i < lightsCount; i++) {
    float currentValue = (float)fht_oct_out[lights[i].octave];
    //      Serial.print(currentValue);
    //      Serial.print(" ");

    bool usingAverage = true;
    if (usingAverage) {
      float differenceFromAverage = abs((currentValue) - lightsData[i].average);
      //        float differenceFromAverage = abs((currentValue) - OCTAVE_REST_AVERAGES[i]);
      //        Serial.print(differenceFromAverage);

      float differenceAsPercent = differenceFromAverage / lightsData[i].average;
      //        Serial.print(differenceAsPercent);

      // Scale it up some! Party mode!!
      differenceAsPercent = pow(differenceAsPercent * lights[i].factor, lights[i].power);
      // Update target brightness
      lightsData[i].targetBrightness = min(max(differenceAsPercent * 250.0f, 0.0f), 250.0f);

      // Low pass filter
      if (FLAG_LOW_PASS == 1) {
        if (
          lightsData[i].targetBrightness < lowPassFilters[i]
          || (lights[i].ledPin != PIN_LED_WARMWHITE && lights[i].ledPin != PIN_LED_WHITE && lightsData[i].targetBrightness < globalLowPassFilter)
        ) {
          lightsData[i].targetBrightness = 0.0f;
        } else {
          lowPassFilters[i] = lightsData[i].targetBrightness;
        }

        if (lightsData[i].targetBrightness > globalLowPassFilter) {
          globalLowPassFilter = lightsData[i].targetBrightness;
        }

        if (lowPassFilters[i] > 0) {
          lowPassFilters[i] -= 0.15f;
        }

        Serial.print(lowPassFilters[i]);
        Serial.print(" ");
      }

      lightsData[i].average = approxRollingAverage(lightsData[i].average, currentValue);

      //        Serial.print(lights[i].average);
      //        Serial.print(" ");
    } else {
      // Using raw
      float differenceAsPercent = currentValue / 10.0f;
      // Scale it up some! Party mode!!
      differenceAsPercent = pow(differenceAsPercent * lights[i].factor, lights[i].power);
      // Update target brightness
      lightsData[i].targetBrightness = min(max(differenceAsPercent * 250.0f, 0.0f), 250.0f);
    }

    // Update actual brightness
    float brightnessDifference = lightsData[i].targetBrightness - lightsData[i].currentBrightness;
    float factor = brightnessDifference > 0 ? BRIGHTNESS_SMOOTH_FACTOR_UP : BRIGHTNESS_SMOOTH_FACTOR_DOWN;
    lightsData[i].currentBrightness += brightnessDifference * factor;

    if (lightsData[i].currentBrightness > maxBrightness) {
      maxBrightness = lightsData[i].currentBrightness;
    }

    //      if (i == 0) {
    //        Serial.print("currentValue= ");
    //        Serial.print(currentValue);
    //        Serial.print(" currentBrightness= ");
    //        Serial.print(lights[i].currentBrightness);
    //        Serial.print(" targetBrightness= ");
    //        Serial.print(lights[i].targetBrightness);
    //        Serial.println("");
    //      }

  }

  if (globalLowPassFilter > 0) {
    globalLowPassFilter -= 0.2f;
  }

  // Write to pin
  for (int i = 0; i < lightsCount; i++) {
    float brightness = lightsData[i].currentBrightness;

    // Normalize
    if (FLAG_NORMALIZE_BRIGHTNESS == 1) {
      brightness = brightness / maxBrightness * 250;
    }

    // Write to pin
    analogWrite(lights[i].ledPin, min(brightness * MUSIC_MODE_VALUE_FACTOR, 250));
  }

  //    int fht_noise_adjusted[8];
  //    int result_octaves[8];
  //    for (int i = 0; i < 8; i++) {  // For each of the 6 useful octave bins
  //
  //      fht_noise_adjusted[i] = abs(fht_oct_out[i] - noise[i]); // take the pink noise average level out, take the asbolute value to avoid negative numbers
  //      fht_noise_adjusted[i] = constrain(fht_noise_adjusted[i], 37, 125); // 37 lowpass for noise; 125 high pass doesn't go much higher than this [found by trial and error]
  ////      result_octaves[i] = map(fht_noise_adjusted[i], 37, 125, 0, 255); // map to values 0 - 160, i.e. blue to red on colour spectrum - larger range gives more colour variability [found by trial and error]
  //      Serial.print(" ");
  //      Serial.print(fht_oct_out[i]);
  //    }
  //    Serial.println("");
}

#define LCD_UPDATE_INTERVAL_MS 40
#define LCD_EQUALIZER_VALUE_FACTOR 12.0f
volatile unsigned long lastLcdUpdateTimeMs = 0;

/**
   Normally the default UI is shown for a given mode.
   Navigation is enabled by pressing the joystick.
*/
bool drawNavigation = false;

//struct ModeData {
//  char name[8];
//  void (*tickFn)();
//};
//
//ModeData modeDatas[2] = {
//  { .name = "Startup", .tickFn = NULL },
//  { .name = "Music", .tickFn = music_loop }
//};

char *modeNames[] = { "Startup", "Music", "Sunrise" };

/** white=0 and warmWhite=20 is a gentle place to start */
float modeData_sunrise_brightnessWhite = 0.0f;
float modeData_sunrise_brightnessWarmWhite = 20.0f;

/** 0=W; 1=WW; */
byte modeData_sunrise_menuSelection = 0;

void updateLcd(unsigned long now) {
  lastLcdUpdateTimeMs = now;

  if (!drawNavigation) {
    //     Common header
    char line[21];
    sprintf(line, "[Mode: %s]", modeNames[currentMode]);
    lcdSetLine(0, line);

    if (currentMode == MODE_STARTUP) {
      lcdSetLine(1, " BedroomLightStrip ");
      lcdSetLine(2, " ...Starting Up...  ");

    } else if (currentMode == MODE_MUSIC) {
      // Display current music stuff

      // Equalizer
      float valueRed = ((float)lightsData[0].currentBrightness) / 255.0f * LCD_EQUALIZER_VALUE_FACTOR;
      float valueGreen = ((float)lightsData[1].currentBrightness) / 255.0f * LCD_EQUALIZER_VALUE_FACTOR;
      float valueBlue = ((float)lightsData[2].currentBrightness) / 255.0f * LCD_EQUALIZER_VALUE_FACTOR;
      float valueWhite = ((float)lightsData[3].currentBrightness) / 255.0f * LCD_EQUALIZER_VALUE_FACTOR;
      float valueWarmWhite = ((float)lightsData[4].currentBrightness) / 255.0f * LCD_EQUALIZER_VALUE_FACTOR;

      lcdSetChar(0, 1, 'R');
      lcdShowBarGraph(1, 1, 18, '>', ' ', valueRed);

      lcdSetChar(0, 2, 'G');
      lcdShowBarGraph(1, 2, 18, '>', ' ', valueGreen);

      lcdSetChar(0, 3, 'B');
      lcdShowBarGraph(1, 3, 18, '>', ' ', valueBlue);

      lcdSetChar(18, 0, valueWhite > 0.1f ? '!' : ' ');
      lcdSetChar(19, 0, valueWarmWhite > 0.1f ? '!' : ' ');
      
    } else if (currentMode == MODE_SUNRISE) {
      // FIXME: Replace with real time from time module
      lcdSetLine(1, (String("Time: ") + now).c_str());

      //       lcdShowBarGraph(=1, 2, 18, 'X', ' ', (float)lightsData[3].currentBrightness / 255.0f);
      //       lcdShowBarGraph(1, 3, 18, 'X', ' ', (float)lightsData[4].currentBrightness / 255.0f);
      lcdSetChars(0, 2, "W :");
      lcdShowBarGraph(3, 2, 16, '+', ' ', modeData_sunrise_brightnessWhite / 255.0f);

      lcdSetChars(0, 3, "WW:");
      lcdShowBarGraph(3, 3, 16, '+', ' ', modeData_sunrise_brightnessWarmWhite / 255.0f);

      lcdSetChar(19, 2, modeData_sunrise_menuSelection == 0 ? '<' : ' ');
      lcdSetChar(19, 3, modeData_sunrise_menuSelection == 1 ? '<' : ' ');
    }
  } else {
    // Draw navigation menu

  }
}

void setAppMode(int newMode) {
//  Serial.print("setAppMode: newMode= ");
//  Serial.println(newMode);

  currentMode = newMode;

  // Clear LCD between mode changes to ensure clean start
  lcdClear();

  // Fade all lights to 0
  float valueRed = analogRead(PIN_LED_RED);
  int valueGreen = analogRead(PIN_LED_GREEN);
  int valueBlue = analogRead(PIN_LED_BLUE);
  int valueWhite = analogRead(PIN_LED_WHITE);
  int valueWarmWhite = analogRead(PIN_LED_WARMWHITE);

  #define TRANSITION_FADE_FACTOR 0.9f

  for (int i = 0; i < 50; i++) {
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

void sunrise_loop() {
  //  analogWrite(PIN_LED_WHITE,250);
  //  analogWrite(PIN_LED_WARMWHITE, 250);

  //  analogWrite(PIN_LED_WHITE, 25);
  //  analogWrite(PIN_LED_WARMWHITE, 250);

  analogWrite(PIN_LED_WHITE, modeData_sunrise_brightnessWhite);
  analogWrite(PIN_LED_WARMWHITE, modeData_sunrise_brightnessWarmWhite);

  //  delay(100);
}

struct MenuItem {
  char title[16];

  //  struct MenuItem *parentItem;

  //  LinkedList<struct MenuItem*> *childItems;
};
//
//MenuItem selectMode_menuItem = {
//  "Select Mode",
//  NULL,
//  NULL
//};

//MenuItem root_menuItem = {
//  "/",
//  NULL,
//  NULL
//};

struct MenuItem root_menuItem = {
  "/"
};

// Add children references
//root_menuItem.childItems = (struct *MenuItem){ &selectMode_menuItem };
//root_menuItem.title = "asdf";

// Add parent references
//selectMode_menuItem.parentItem = &root_menuItem;

//struct MenuItem* currentItem;

inline float scaleJoystickValue(float n) {
  float scaledValue = (n - 512.0f) / 512.0f;
  // High pass filter
  if (abs(scaledValue) <= 0.05f) {
    return 0;
  }
  return pow(scaledValue, 2.0f) * (scaledValue < 0 ? -1 : 1);
}

#define MENU_ACCUMULATOR_TICK 90

void tick_menu(unsigned long now) {
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
  menuAccumulatorX += scaleJoystickValue(valueY) * -1;
  menuAccumulatorY += scaleJoystickValue(valueX);

  //    Serial.print(menuAccumulatorX);
  //    Serial.print(" ");
  //  Serial.print(menuAccumulatorY);
  //    Serial.println(" ");

  if (valueSwitch != lastJoystickSwitchValue && !valueSwitch) {
    //    FIXME: In future this should go to navigation menu
    //    drawNavigation = true;

    switch (currentMode) {
      case MODE_SUNRISE:
        setAppMode(MODE_MUSIC);
        break;
      case MODE_MUSIC:
        setAppMode(MODE_SUNRISE);
        break;
    }
  }

  // Per mode draw settings
  if (currentMode == MODE_SUNRISE) {
    // Toggle selected value
    if (menuAccumulatorY < -MENU_ACCUMULATOR_TICK) {
      menuAccumulatorY = 0;

      modeData_sunrise_menuSelection = !modeData_sunrise_menuSelection;

    } else if (menuAccumulatorY > MENU_ACCUMULATOR_TICK) {
      menuAccumulatorY = 0;

      modeData_sunrise_menuSelection = !modeData_sunrise_menuSelection;
    }

#define MENU_SUNRISE_X_FACTOR 0.5f

    // Smooth scaling of brightness
    if (!modeData_sunrise_menuSelection) {
      modeData_sunrise_brightnessWhite += menuAccumulatorX * MENU_SUNRISE_X_FACTOR;

      if (modeData_sunrise_brightnessWhite < 0) {
        modeData_sunrise_brightnessWhite = 0;
      } else if (modeData_sunrise_brightnessWhite > 250) {
        modeData_sunrise_brightnessWhite = 250;
      }

      //      Serial.println(modeData_sunrise_brightnessWhite);

    } else {
      modeData_sunrise_brightnessWarmWhite += menuAccumulatorX * MENU_SUNRISE_X_FACTOR;

      if (modeData_sunrise_brightnessWarmWhite < 0) {
        modeData_sunrise_brightnessWarmWhite = 0;
      } else if (modeData_sunrise_brightnessWarmWhite > 250) {
        modeData_sunrise_brightnessWarmWhite = 250;
      }

      //      Serial.println(modeData_sunrise_brightnessWarmWhite);
    }

    menuAccumulatorX = 0;
  }

  lastJoystickSwitchValue = valueSwitch;
}

void setup()
{
  // Debug console
  Serial.begin(9600);

  // LCD
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
  pinMode (PIN_JOYSTICK_X, INPUT);
  pinMode (PIN_JOYSTICK_Y, INPUT);
  pinMode (PIN_JOYSTICK_SWITCH, INPUT_PULLUP);

  pinMode(PIN_CLOCK_DEBUG, OUTPUT);

  // Start app
  delay(250);
  //  setAppMode(MODE_MUSIC);
  setAppMode(MODE_SUNRISE);
}

void loop() {
  while (1) { // reduces jitter
    //  FIXME: Use ModeData to structure name/loop/etc
    if (currentMode == MODE_MUSIC) {
      music_loop();
    } else if (currentMode == MODE_SUNRISE) {
      sunrise_loop();
    }
    unsigned long now = millis();

    tick_menu(now);

    //    Serial.print("lastLcdUpdateTimeMs= ");
    //    Serial.println(lastLcdUpdateTimeMs);

    if (abs(now - lastLcdUpdateTimeMs) > LCD_UPDATE_INTERVAL_MS) {
      updateLcd(now);
      // FIXME: Lol naming
      lcdUpdate(now);
    }
  }
}


void setFreeRunMode() {
  //  TIMSK0 = 0;                               // turn off timer0 for lower jitter
  //  ADCSRA = 0xE5;                            // "ADC Enable", "ADC Start Conversion", "ADC Auto Trigger Enable", 32 prescaler for 38.5 KHz
  //                                            // 0xE4, 16 prescaler
  //                                            // 0xE6, 64 prescaler
  //  ADMUX = 0x40;                             // b0100 0000; use adc0, right-align, use the full 10 bits resolution
  //  DIDR0 = 0x01;                             // turn off the digital input for adc0



  PORTB = 0; // 1 for hight
  //  DDRB = (1<<R1)|(1<<R2)|(1<<R3); //1 for output

  // Note: This will also cause `millis()` and `delay()` calls to stop working.
  //  TIMSK0 = 0; // turn off timer0 for lower jitter

  //ADCSRA = 0xC5;//0xe5; // set the adc to free running mode
  DIDR0 = 0x39; // turn off the digital input for adc0,3,4,5
}
