#ifndef BITMAP_BATTERY_HEADER
#define BITMAP_BATTERY_HEADER

#include "Adafruit_GFX.h"
#include "Adafruit_SSD1306.h"

void bitmap_battery_ondraw(int16_t offsetx, int16_t offsety, uint16_t sizex, uint16_t sizey, Adafruit_GFX *disp);
void bitmap_battery_setbatterylevel(float lvl);

#endif