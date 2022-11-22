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

class Idraw{
  public:
    virtual void update(drawable_cbparam &param);

};

class Idraw_interval: Idraw{
  private:
    int16_t time_start;
    int16_t time_ms;
    bool _interval;

  protected:
    bool _is_interval();
    int16_t _currenttime();
  
  public:
    void update(drawable_cbparam &param);

    void setInterval(int16_t ms);
};

#endif