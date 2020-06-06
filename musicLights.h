//#define AVG_SAMPLE_COUNT 100
#define AVG_SAMPLE_COUNT 10

//#define BRIGHTNESS_SMOOTH_FACTOR_UP 0.15f
//#define BRIGHTNESS_SMOOTH_FACTOR_DOWN 0.08f
#define BRIGHTNESS_SMOOTH_FACTOR_UP 0.25f
#define BRIGHTNESS_SMOOTH_FACTOR_DOWN 0.15f

typedef struct LightMapping
{
  byte ledPin;
  byte octave;

  float factor;
  float power;
};

typedef struct LightMappingData
{
  float average;

  float targetBrightness;
  float currentBrightness;
};

extern LightMapping lights[5];
extern LightMappingData lightsData[5];

#define FLAG_LOW_PASS false
#define FLAG_NORMALIZE_BRIGHTNESS false

#define MUSIC_MODE_VALUE_FACTOR 8.0f

void musicLights_tick();
