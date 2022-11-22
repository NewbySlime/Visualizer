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

  private:
    bool _drawtext = true, _refreshText = false;

    TextType _currtxttype = Center;
    const char *_txt1 = NULL, *_txt2 = NULL;
    uint16_t _txt1w, _txt1h, _txt2w, _txt2h, _txtsize1, _txtsize2, _txtcolor1, _txtcolor2;

    void checkRefreshText(Adafruit_GFX *disp);
  
  protected:
    // will be called when it needs to refresh
    virtual void onRefreshText(Adafruit_GFX *disp){}
    void setRefreshFlag();

  public:
    virtual void onDraw(drawable_cbparam &param);

    // can pass NULL if no text to display
    virtual void useText(const char *txt1, const char *txt2);
    virtual void useText1(const char *txt);
    virtual void useText2(const char *txt);
    void useSize(uint16_t size1, uint16_t size2);
    void useColor(uint16_t color1, uint16_t color2);
    void changeTextType(TextType type);
};


// this will also handle the rtc module
class draw_time: public draw_text, public Idraw_interval{
  private:
    bool _usemini = false;
    bool _drawmiddle = true;

    char _char_timeClock[8];
    uint16_t _txtTimeClock_widthmini = 0, _txtTimeClock_heightmini = 0;
    char _char_timeDate[24];

    uint8_t _minute = 0, _hour = 0;

    void onRefreshText(Adafruit_GFX *disp);

  public:
    draw_time();

    void onDraw(drawable_cbparam &param);
    void onVisible(bool visible);

    void settime(const DateTime &time);
    DateTime gettime();

    void drawmiddlechar(bool _b);
};


// this class is meant to draw ui like loading... and some stuff like applying and cancelling
class draw_loadapply: public draw_text, public Idraw_interval{
  protected:
    const char *_txt_applied, *_txt_cancelled;
    char *__txt1, *__txt2;
    size_t __txt1len, __txt2len;
    size_t _dotidx;

    bool __txt1_usedots, __txt2_usedots;
    bool _loading_state;

  public:
    draw_loadapply();

    void onDraw(drawable_cbparam &param);
    void onVisible(bool visible);

    // note that this will make another buffer to copy this data
    void useText(const char *txt1, const char *txt2);
    void useText1(const char *txt);
    void useText2(const char *txt);

    void useDotLoad(bool txt1, bool txt2);
    
    void useText_applied(const char *txt);
    void useText_cancelled(const char *txt);

    // this will change the txt2
    // and this will stop the loading state
    void onApplied();

    // this will change the txt2
    // and this will stop the loading state
    void onCancelled();

    // this will change the txt2
    // and this will NOT stop the loading state
    void onNormal();
  
    // change if in state of loading or not
    void onLoading();
    void onDoneLoading();
};


#endif