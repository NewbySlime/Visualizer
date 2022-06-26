#include "battery.hpp"
#include "Arduino.h"

#define _margintb 4
#define _marginlr 8

#define _batteryBarWidth 1
#define _batteryBarHeight 2
#define _batteryBarMany 5

#define _width 8
#define _height 4
const uint8_t PROGMEM _bits[] = {
 0x01,0x7c,0x7c,0x01 };

#define _batteryOffsetx _width-_marginlr
#define _batteryOffsety _margintb

#define _batteryBarOffsetx _batteryOffsetx+2
#define _batteryBarOffsety _batteryOffsety+1

uint8_t battbar = 0;
// this will always put on the top right
void bitmap_battery_ondraw(int16_t offsetx, int16_t offsety, uint16_t sizex, uint16_t sizey, Adafruit_GFX *disp){
  disp->drawXBitmap(offsetx+sizex-_batteryOffsetx, offsety+_batteryOffsety, _bits, _width, _height);

  if(battbar <= _batteryBarMany && battbar != 0)
    disp->fillRect(offsetx+sizex-_batteryBarOffsetx, offsety+_batteryBarOffsety, _batteryBarWidth*battbar, _batteryBarHeight, SSD1306_WHITE);
}

void bitmap_battery_setbatterylevel(float lvl){
  battbar = (uint8_t)roundf(lvl*_batteryBarMany);
}