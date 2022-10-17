#ifndef DRAWABLE_HEADER
#define DRAWABLE_HEADER

#include "Arduino.h"
#include "Adafruit_GFX.h"

struct drawable_cbparam{
  int16_t offsetx;
  int16_t offsety;
  uint16_t sizex;
  uint16_t sizey;
  void *disphandler;
  size_t deltams;
  bool islist;
};


class drawable{
  public:
    virtual void onDraw(drawable_cbparam &param){}
    virtual void onVisible(bool visible){}
};

#endif