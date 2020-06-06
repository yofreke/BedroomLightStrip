#include <arduino.h>

#define OCTAVE 1 // use the octave output function
//#define LOG_OUT 1             // use the log output function
#define FHT_N 256 // set to 256 point fht
#include <FHT.h>

#include "musicLights.h"
#include "pinMap.h"

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

// int OCTAVE_REST_AVERAGES[8] = {OCTAVE_REST_AVERAGE_0, OCTAVE_REST_AVERAGE_1,
//                                OCTAVE_REST_AVERAGE_2, OCTAVE_REST_AVERAGE_3,
//                                OCTAVE_REST_AVERAGE_4, OCTAVE_REST_AVERAGE_5,
//                                OCTAVE_REST_AVERAGE_6, OCTAVE_REST_AVERAGE_7};

// FIXME: Probably should do some noise thresholds
// int noise[] = {90,  125, 147, 143, 128,
//                123, 104, 89}; // just test output without in silence, and use
//                               // values from ocatve bins

LightMapping lights[5] = {
    {.ledPin = PIN_LED_RED, .octave = 2, .factor = 2.6f, .power = 5.0f},
    {.ledPin = PIN_LED_GREEN, .octave = 4, .factor = 1.8f, .power = 4.0f},
    {.ledPin = PIN_LED_BLUE, .octave = 5, .factor = 1.15f, .power = 3.0f},
    {.ledPin = PIN_LED_WARMWHITE, .octave = 6, .factor = 1.5f, .power = 12.0f},
    {.ledPin = PIN_LED_WHITE, .octave = 7, .factor = 1.15f, .power = 10.0f}};

LightMappingData lightsData[5] = {
    {.average = 0.0f, .targetBrightness = 0.0f, .currentBrightness = 0.0f},
    {.average = 0.0f, .targetBrightness = 0.0f, .currentBrightness = 0.0f},
    {.average = 0.0f, .targetBrightness = 0.0f, .currentBrightness = 0.0f},
    {.average = 0.0f, .targetBrightness = 0.0f, .currentBrightness = 0.0f},
    {.average = 0.0f, .targetBrightness = 0.0f, .currentBrightness = 0.0f}};

float lowPassFilters[5] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
float globalLowPassFilter = 0.0f;

byte lightsCount = sizeof(lights) / sizeof(LightMapping);

double approxRollingAverage(double avg, double new_sample)
{
  avg -= avg / AVG_SAMPLE_COUNT;
  avg += new_sample / AVG_SAMPLE_COUNT;

  return avg;
}

void musicLights_tick()
{
  cli(); // UDRE interrupt slows this way down on arduino1.0
  for (int i = 0; i < FHT_N; i++)
  { // save 256 samples
    int k = analogRead(0);
    k -= 0x0200;      // form into a signed int
    k <<= 6;          // form into a 16b signed int
    fht_input[i] = k; // put real data into bins
  }
  fht_window();  // window the data for better frequency response
  fht_reorder(); // reorder the data before doing the fht
  fht_run();     // process the data in the fht
  fht_mag_octave();
  sei();

  // End of Fourier Transform code - output is stored in fht_oct_out[i].

  float maxBrightness = 0.0f;

  for (int i = 0; i < lightsCount; i++)
  {
    float currentValue = (float)fht_oct_out[lights[i].octave];
    //      Serial.print(currentValue);
    //      Serial.print(" ");

    bool usingAverage = true;
    if (usingAverage)
    {
      float differenceFromAverage = abs((currentValue)-lightsData[i].average);
      //        float differenceFromAverage = abs((currentValue) -
      //        OCTAVE_REST_AVERAGES[i]);
      //        Serial.print(differenceFromAverage);

      float differenceAsPercent = differenceFromAverage / lightsData[i].average;
      //        Serial.print(differenceAsPercent);

      // Scale it up some! Party mode!!
      differenceAsPercent =
          pow(differenceAsPercent * lights[i].factor, lights[i].power);
      // Update target brightness
      lightsData[i].targetBrightness =
          min(max(differenceAsPercent * 250.0f, 0.0f), 250.0f);

      // Low pass filter
      if (FLAG_LOW_PASS == 1)
      {
        if (lightsData[i].targetBrightness < lowPassFilters[i] ||
            (lights[i].ledPin != PIN_LED_WARMWHITE &&
             lights[i].ledPin != PIN_LED_WHITE &&
             lightsData[i].targetBrightness < globalLowPassFilter))
        {
          lightsData[i].targetBrightness = 0.0f;
        }
        else
        {
          lowPassFilters[i] = lightsData[i].targetBrightness;
        }

        if (lightsData[i].targetBrightness > globalLowPassFilter)
        {
          globalLowPassFilter = lightsData[i].targetBrightness;
        }

        if (lowPassFilters[i] > 0)
        {
          lowPassFilters[i] -= 0.15f;
        }

        Serial.print(lowPassFilters[i]);
        Serial.print(" ");
      }

      lightsData[i].average =
          approxRollingAverage(lightsData[i].average, currentValue);

      //        Serial.print(lights[i].average);
      //        Serial.print(" ");
    }
    else
    {
      // Using raw
      float differenceAsPercent = currentValue / 10.0f;
      // Scale it up some! Party mode!!
      differenceAsPercent =
          pow(differenceAsPercent * lights[i].factor, lights[i].power);
      // Update target brightness
      lightsData[i].targetBrightness =
          min(max(differenceAsPercent * 250.0f, 0.0f), 250.0f);
    }

    // Update actual brightness
    float brightnessDifference =
        lightsData[i].targetBrightness - lightsData[i].currentBrightness;
    float factor = brightnessDifference > 0 ? BRIGHTNESS_SMOOTH_FACTOR_UP
                                            : BRIGHTNESS_SMOOTH_FACTOR_DOWN;
    lightsData[i].currentBrightness += brightnessDifference * factor;

    if (lightsData[i].currentBrightness > maxBrightness)
    {
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

  if (globalLowPassFilter > 0)
  {
    globalLowPassFilter -= 0.2f;
  }

  // Write to pin
  for (int i = 0; i < lightsCount; i++)
  {
    float brightness = lightsData[i].currentBrightness;

    // Normalize
    if (FLAG_NORMALIZE_BRIGHTNESS == 1)
    {
      brightness = brightness / maxBrightness * 250;
    }

    // Write to pin
    analogWrite(lights[i].ledPin,
                min(brightness * MUSIC_MODE_VALUE_FACTOR, 250));
  }
}
