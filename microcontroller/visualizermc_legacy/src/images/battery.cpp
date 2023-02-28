#include "drawables.hpp"
#include "Arduino.h"

#include "display/display.hpp"

#define _batteryBarWidth 1
#define _batteryBarHeight 4
#define _batteryBarMany 10

#define _width 16
#define _height 8
const uint8_t PROGMEM _bits[] = {
 0xfc,0xff,0xfc,0xff,0x0f,0xc0,0x0f,0xc0,0x0f,0xc0,0x0f,0xc0,0xfc,0xff,0xfc,
 0xff };

#define _batteryOffsetx _width-4
#define _batteryOffsety 0

#define _batteryBarOffsetx _batteryOffsetx+4
#define _batteryBarOffsety _batteryOffsety+2

// this will always put on the top right
void bitmap_battery::onDraw(drawable_cbparam &param){
  Adafruit_GFX *disp = ((displayHandler*)param.disphandler)->getdisplay();
  disp->drawXBitmap(param.sizex-_batteryOffsetx, _batteryOffsety, _bits, _width, _height, SSD1306_WHITE);

  if(_battbar <= _batteryBarMany && _battbar != 0)
    disp->fillRect(param.sizex-_batteryBarOffsetx+(_batteryBarWidth*(_batteryBarMany-_battbar)), _batteryBarOffsety, _batteryBarWidth*_battbar, _batteryBarHeight, SSD1306_WHITE);
}

void bitmap_battery::setbatterylevel(float lvl){
  _battbar = (uint8_t)roundf(lvl*_batteryBarMany);
}