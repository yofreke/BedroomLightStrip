/****************************************
  Example Sound Level Sketch for the
  Adafruit Microphone Amplifier
****************************************/

#define PIN_LED_RED 3
#define PIN_LED_GREEN 5
#define PIN_LED_BLUE 6
#define PIN_LED_WHITE 9
#define PIN_LED_WARMWHITE 10

#define OCTAVE 1 // use the octave output function
#define FHT_N 256 // set to 256 point fht

#include <FHT.h> // include the library

void setup()
{
  Serial.begin(9600);

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

  ADCSRA = 0xe5; // set the adc to free running mode
  ADMUX = 0x40; // use adc0
  DIDR0 = 0x01; // turn off the digital input for adc0
}

//#define AVG_SAMPLE_COUNT 100
#define AVG_SAMPLE_COUNT 10

#define BRIGHTNESS_SMOOTH_FACTOR_UP 0.15f
#define BRIGHTNESS_SMOOTH_FACTOR_DOWN 0.08f

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

#define FLAG_LOW_PASS true
#define FLAG_NORMALIZE_BRIGHTNESS false

void music_loop() {
  while (1) { // reduces jitter
    cli();  // UDRE interrupt slows this way down on arduino1.0
    for (int i = 0 ; i < FHT_N ; i++) { // save 256 samples
      while (!(ADCSRA & 0x10)); // wait for adc to be ready
      ADCSRA = 0xf5; // restart adc
      byte m = ADCL; // fetch adc data
      byte j = ADCH;
      int k = (j << 8) | m; // form into an int
      k -= 245; // used to be 0x0200, form into a signed int
      k <<= 6; // form into a 16b signed int
      fht_input[i] = k; // put real data into bins
    }
    fht_window(); // window the data for better frequency response
    fht_reorder(); // reorder the data before doing the fht
    fht_run(); // process the data in the fht
    fht_mag_octave(); // take the output of the fht
    sei();

    float maxBrightness = 0.0f;

    for (int i = 0; i < lightsCount; i++) {
      // Subtract 128 because this microphone oscillates around 50% power
      //      int currentValue = abs(fht_oct_out[lights[i].octave] - 128);
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
      analogWrite(lights[i].ledPin, brightness * 0.95f);
    }

    //    for (int i = 0; i < 8; i++) {
    //      Serial.print(fht_oct_out[i]);
    //      Serial.print(" ");
    //    }
    Serial.println("");

    delay(1);
  }
}

long startTimeMs = 0;

void sunrise_loop() {
  analogWrite(PIN_LED_WHITE, 50);
  analogWrite(PIN_LED_WARMWHITE, 200);

  while (true) {
    delay(1000);
  }
}

void loop() {
  music_loop();

//    sunrise_loop();
}
