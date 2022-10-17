#include "drawables.hpp"

#include "display/animation.hpp"
#include "display/display.hpp"

#define _AcceptBar_Height (float)6/128


const float PROGMEM _AcceptBar_Time = 1.2f;
const float PROGMEM _AcceptBar_TimeAfter = 0.3f;


void _dmpfunc(void*){}


draw_acceptbar::draw_acceptbar(){
  _currentcb = _dmpfunc;
  _animdata = AnimationData::get_animdata(AnimationData::EaseIn2);
}

void draw_acceptbar::onDraw(drawable_cbparam &param){
  bool _callcb = false;
  if(_msreset){
    _msreset = false;
    _elapsedtime = 0;
  }
  else
    _elapsedtime += param.deltams;

  // update
  if(_ispressed && !_doafter){
    _barlvl = bezier_func1D(_animdata.first, _animdata.second, (float)_elapsedtime/(_AcceptBar_Time*1000));

    if(_barlvl >= 1.f){
      _barlvlconstraint = _barlvl;
      _callcb = true;
      _doafter = true;
      _msreset = true;
    }
  }
  else if(_doafter){
    _barlvl = bezier_func1D(_animdata.first, _animdata.second, (float)_elapsedtime/(_AcceptBar_TimeAfter*1000));
    _barlvl = _barlvlconstraint - (_barlvl*_barlvlconstraint);

    if(_barlvl <= 0.f){
      _ispressed = false;
      _doafter = false;
    }
  }

  // draw the bar
  if(_barlvl > 0.f){
    Adafruit_GFX *disp = ((displayHandler*)param.disphandler)->getdisplay();

    uint16_t acceptBarH = round(_AcceptBar_Height * param.sizey);
    uint16_t acceptBarLen = round(_barlvl*param.sizex/2);
    disp->fillRect(0, param.sizey-acceptBarH, acceptBarLen, acceptBarH, SSD1306_WHITE);
    disp->fillRect(param.sizex-acceptBarLen, param.sizey-acceptBarH, acceptBarLen, acceptBarH, SSD1306_WHITE);
  }

  // callback
  if(_callcb && _objclass)
    _currentcb(_objclass);
}

void draw_acceptbar::onpressed(){
  _msreset = true;
  _doafter = false;
  _ispressed = true;
}

void draw_acceptbar::onreleased(){
  if(_ispressed){
    _msreset = true;
    _doafter = true;
    _barlvlconstraint = _barlvl;
  }
}


void draw_acceptbar::setcallback(OnAcceptCallback cb, void *obj){
  _currentcb = cb;
  _objclass = obj; 
}