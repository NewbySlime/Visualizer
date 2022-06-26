#include "time.hpp"


const char PROGMEM *daystr[] = {
  "Monday",
  "Tuesday",
  "Wednesday",
  "Thursday",
  "Friday",
  "Saturday",
  "Sunday"
};

DateTime currentTime;

bool _usemini = true;
bool _drawmiddle = true;

void draw_time_ondraw(int16_t offsetx, int16_t offsety, uint16_t sizex, uint16_t sizey, Adafruit_GFX *disp){
  
}

void draw_time_settime(const DateTime& time){
  currentTime = time;
}

void draw_time_usemini(bool _use){
  _usemini = true;
}

void draw_time_drawmiddlechar(bool _b){
  _drawmiddle = _b;
}