#include <arduino.h>

#define MODE_STARTUP 0
#define MODE_MUSIC 1
#define MODE_SUNRISE 2
#define MODE_MENU 3

// struct ModeData {
//  char name[8];
//  void (*tickFn)();
//};
//
// ModeData modeDatas[2] = {
//  { .name = "Startup", .tickFn = NULL },
//  { .name = "Music", .tickFn = music_loop }
//};

extern char *modeNames[];

extern float modeData_sunrise_brightnessWhite;
extern float modeData_sunrise_brightnessWarmWhite;
extern byte modeData_sunrise_menuSelection;

extern byte currentMode;

void stateMachine_setMode(int newMode);
