#ifndef DRAW_TIME_HEADER
#define DRAW_TIME_HEADER

#include "Adafruit_GFX.h"
#include "Arduino.h"
#include "RTClib.h"

void draw_time_ondraw(int16_t offsetx, int16_t offsety, uint16_t sizex, uint16_t sizey, Adafruit_GFX *disp);
void draw_time_settime(const DateTime& time);

// this will set the time placement, either in the top bar or the center
void draw_time_usemini(bool _use);

void draw_time_drawmiddlechar(bool _b);

#endif