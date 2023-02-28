#include "drawables.hpp"
#include "display/display.hpp"


#define _dividertxt_size 2

void draw_text::checkRefreshText(Adafruit_GFX *disp){
  if(_refreshText && (_txt1 || _txt2)){
    _refreshText = false;
    int16_t _dmpi = 0;

    if(_txt1){
      disp->setTextSize(_txtsize1);
      disp->getTextBounds(_txt1, 0, 0, &_dmpi, &_dmpi, &_txt1w, &_txt1h);
    }
    else
      _txt1w = _txt1h = 0;

    if(_txt2){
      disp->setTextSize(_txtsize2);
      disp->getTextBounds(_txt2, 0, 0, &_dmpi, &_dmpi, &_txt2w, &_txt2h);
    }
    else
      _txt2w = _txt2h = 0;

    onRefreshText(disp);
  }
}

void draw_text::setRefreshFlag(){
  _refreshText = true;
}

void draw_text::onDraw(drawable_cbparam &param){
  if(_drawtext){
    Adafruit_GFX *disp = ((displayHandler*)param.disphandler)->getdisplay();
    checkRefreshText(disp);

    int16_t txt1posx = (param.sizex/2), txt1posy;
    if(_txt1 && _txt2)
      txt1posy = (param.sizey*_txt1h)/(_txt1h+_txt2h);
    else
      txt1posy = (param.sizey+_txt1h)/2;

    // text2
    if(_txt2 && _currtxttype != Center){
      int16_t txt2posx = txt1posx-(_txt2w/2), txt2posy = txt1posy;

      switch(_currtxttype){
        break; case Top_Bottom:
          txt2posy = txt2posy+_dividertxt_size/2+1;
      }

      txt2posx += param.offsetx;
      txt2posy += param.offsety;

      disp->setTextSize(_txtsize2);
      disp->setTextColor(_txtcolor2);
      disp->setCursor(txt2posx, txt2posy);
      disp->print(_txt2);
    }

    // text1
    if(_txt1){
      txt1posx = txt1posx-(_txt1w/2);
      switch(_currtxttype){
        break; case Center:
          txt1posy = (_txt1h+param.sizey)/2;

        break; case Top_Bottom:
          txt1posy = txt1posy-_txt1h-_dividertxt_size/2;
      }

      txt1posx += param.offsetx;
      txt1posy += param.offsety;

      disp->setTextSize(_txtsize1);
      disp->setTextColor(_txtcolor1);
      disp->setCursor(txt1posx, txt1posy);
      disp->print(_txt1);
    }
  }
}

void draw_text::useText(const char *txt1, const char *txt2){
  _txt1 = txt1; _txt2 = txt2;
  _refreshText = true;
}

void draw_text::useText1(const char *txt){
  _txt1 = txt;
  _refreshText = true;
}

void draw_text::useText2(const char *txt){
  _txt2 = txt;
  _refreshText = true;
}

void draw_text::useSize(uint16_t size1, uint16_t size2){
  _txtsize1 = size1; _txtsize2 = size2;
  _refreshText = true;
}

void draw_text::useColor(uint16_t color1, uint16_t color2){
  _txtcolor1 = color1; _txtcolor2 = color2;
}

void draw_text::changeTextType(TextType type){
  _currtxttype = type;
}