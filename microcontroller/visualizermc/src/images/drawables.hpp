#ifndef DRAWABLES_HEADER
#define DRAWABLES_HEADER

#include "Adafruit_GFX.h"
#include "Adafruit_SSD1306.h"

#include "drawable.hpp"

#include "RTClib.h"


typedef void (*OnAcceptCallback)(void*);

class draw_acceptbar: public drawable{
  private:
    std::pair<const float*, uint16_t> _animdata;

    OnAcceptCallback _currentcb;
    void *_objclass = NULL;
    unsigned long _elapsedtime = 0;
    bool _ispressed = false, _doafter = false, _msreset = false;
    float _barlvlconstraint = 0.f;
    float _barlvl = 0.f;

  public:
    draw_acceptbar();
    void onDraw(drawable_cbparam &param);

    void onpressed();
    void onreleased();

    void setcallback(OnAcceptCallback cb, void *obj);
};


class bitmap_battery: public drawable{
  private:
    uint8_t _battbar = 0;

  public:
    void onDraw(drawable_cbparam &param);
    void setbatterylevel(float lvl);
};


class draw_text: public drawable{
  public:
    enum TextType{
      Center,
      Top_Bottom
    };
  
  protected:
    bool _drawtext = true, _refreshText = false;

    TextType _currtxttype = Center;
    const char *_txt1 = NULL, *_txt2 = NULL;
    uint16_t _txt1w, _txt1h, _txt2w, _txt2h, _txtsize1, _txtsize2, _txtcolor1, _txtcolor2;

    void checkRefreshText(Adafruit_GFX *disp);

  private:
    // will be called when it needs to refresh
    virtual void onRefreshText(Adafruit_GFX *disp){}

  public:
    virtual void onDraw(drawable_cbparam &param);

    void useText(const char *txt1, const char *txt2);
    void useSize(uint16_t size1, uint16_t size2);
    void useColor(uint16_t color1, uint16_t color2);
    void changeTextType(TextType type);
};


// this will also handle the rtc module
class draw_time: public draw_text{
  private:
    bool _usemini = false;
    bool _drawmiddle = true;

    char _char_timeClock[8];
    uint16_t _txtTimeClock_widthmini = 0, _txtTimeClock_heightmini = 0;
    char _char_timeDate[24];

    uint8_t _minute = 0, _hour = 0;

    int16_t _time1s = 0;
    int32_t _timeup = 0;

    void _onupdate1s();
    void _onupdatetime();
    void onRefreshText(Adafruit_GFX *disp);

  public:
    draw_time();

    // don't forget to call this in derived class
    void onDraw(drawable_cbparam &param);
    void onVisible(bool visible);

    void settime(const DateTime &time);
    DateTime gettime();

    void drawmiddlechar(bool _b);

    void bindRTC(RTC_I2C *rtc);
};


#endif