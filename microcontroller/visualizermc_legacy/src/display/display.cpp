#include "timer.hpp"
#include "animation.hpp"

#include "Adafruit_SSD1306.h"
#include "display.hpp"

#define DISP_DRAWFLAG_LEFTBORDER 0b1
#define DISP_DRAWFLAG_RIGHTBORDER 0b10


#define DISP_LIST_TXTSIZE 2
#define IMTEXT_TXTSIZE 1

#define _boxsign_width 1/64
#define _boxsign_height 1


// this can be used for back forth
const float PROGMEM bezier_slideAnimationData[] = {0, -0.18, 0.1, 1};
const size_t PROGMEM bezier_slideAnimationDatalen = sizeof(bezier_slideAnimationData)/sizeof(float);
const float PROGMEM bezier_slideAnimationTime = 0.5f;

const float PROGMEM bezier_nudgeAnimationData[] = {0, 0.2, 0};
const size_t PROGMEM bezier_nudgeAnimationDatalen = sizeof(bezier_nudgeAnimationData)/sizeof(float);
const float PROGMEM bezier_nudgeAnimationTime = 0.25f;

const char *_dmpstr = "";
void _dmpfunc1(listDisplay_Interact**, void*){}
const listDisplay_Interact::_interactData __dmpint{
  .onClicked = _dmpfunc1,
  .useFixedName = true,
  .name = _dmpstr,
  .objdraw = NULL
};


const float PROGMEM _im_txtmargin = (float)1/8;

const float PROGMEM _im_animtime = 0.25f;
const size_t PROGMEM _im_blinkintrms = 600;
const size_t PROGMEM _im_maintxttimems = 6000;
const size_t PROGMEM _im_fulldisptimems = 5000;



//      draw_importanttxt class
draw_importanttxt::draw_importanttxt(){
  _animdata = AnimationData::get_animdata(AnimationData::EaseOut);
}

void draw_importanttxt::onDraw(drawable_cbparam &param){
  if(_dodraw){
    Adafruit_GFX *disp = ((displayHandler*)param.disphandler)->getdisplay();

    if(_doreset){
      _doreset = false;

      if(_refreshtxt){
        _refreshtxt = false;
        int16_t _dmpi = 0;

        disp->setTextSize(IMTEXT_TXTSIZE);
        disp->getTextBounds(_currenttxt, 0, 0, &_dmpi, &_dmpi, &_txtwidth_main, &_txtheight);

        _barheight = (_im_txtmargin*2*_txtheight)+_txtheight;
      }

      _currenttime = 0;
    }
    else
      _currenttime += param.deltams;

    int16_t barposy = _barheight;
    if(_animtxt){
      barposy = bezier_func1D(_animdata.first, _animdata.second, _currenttime/(_im_animtime*1000)) * barposy;

      if(_animtxtreverse)
        barposy = _barheight - barposy;

      if(!_animtxtreverse && barposy >= _barheight){
        barposy = _barheight;
        _animtxt = false;
      }
      else if(_animtxtreverse && barposy <= 0){
        _dodraw = false;
        return;
      }
    }

    disp->fillRect(0, 0, param.sizex, barposy, SSD1306_WHITE);

    disp->setTextSize(IMTEXT_TXTSIZE);
    disp->setTextWrap(false);
    disp->setTextColor(SSD1306_BLACK);
    int16_t txtposy = barposy - _barheight + _barheight*_im_txtmargin;
    if(_drawtxt){
      disp->setCursor((param.sizex/2)-(_txtwidth_main/2), txtposy);
      disp->print(_currenttxt);
    }
      
    if(_currenttime >= (_idxInterval * _im_blinkintrms)){
      _drawtxt = !_drawtxt;
      _idxInterval++;
    }
    
    if(_currenttime >= _im_maintxttimems){
      _doreset = true;
      _animtxt = true;
      _animtxtreverse = true;
    }
  }
}

void draw_importanttxt::print_text(const char *txt){
  _currenttxt = txt;
  _idxInterval = 0;
  _dodraw = true;
  _doreset = true;
  _drawmain = true;
  _refreshtxt = true;
  _animtxt = true;
  _animtxtreverse = false;
}



//      displayHandler class

// return true if target index not outside the array index
// and it cannot add contentLayerIndex
void displayHandler::_check_drawborder(int idx){
  _drawFlag &= ~(DISP_DRAWFLAG_LEFTBORDER | DISP_DRAWFLAG_RIGHTBORDER);

  if(idx <= 0 && !_llListInteract->_current->listLoop)
    _drawFlag |= DISP_DRAWFLAG_LEFTBORDER;
  else if(idx >= (_llListInteract->_current->listinteract.size()-1) && !_llListInteract->_current->listLoop)
    _drawFlag |= DISP_DRAWFLAG_RIGHTBORDER;
}

bool displayHandler::_change_list(size_t contentLayerdec, int32_t additionN){
  bool b = false;

  if(contentLayerdec > 0 && contentLayerdec <= _nlayersContent){
    _txt2 = _txt1;
    for(size_t i = 0; i < contentLayerdec && _llListInteract->_before; i++){
      listInteract_linkedList *_link = _llListInteract;
      _llListInteract = _llListInteract->_before;

      delete _link;
    }

    _nlayersContent -= contentLayerdec;

    _txt1 = &_llListInteract->_current->listinteract[_llListInteract->_current->listIndex];

    b = true;
  }

  int newIndex = _getListIndex(additionN);
  if(newIndex != -1){
    _check_drawborder(newIndex);

    if(additionN != 0){
      _txt2 = &_llListInteract->_current->listinteract[_llListInteract->_current->listIndex];

      _llListInteract->_current->listIndex = newIndex;
      _txt1 = &_llListInteract->_current->listinteract[_llListInteract->_current->listIndex];

      b = true;
    }
  }

  return b;
}

void displayHandler::_onDisplayRefresh(){
  if(_currbindDisp){
    _clear();
    float _bezierAnimVal = 0.f;

    unsigned long _deltatimems = 0, _currenttimems = millis();
    if(_currenttimems < _timelastUpdatems)
      _deltatimems = UINT64_MAX-_timelastUpdatems+_currenttimems;
    else
      _deltatimems = _currenttimems-_timelastUpdatems;
    _timelastUpdatems = millis();
    

    _currbindDisp->setTextWrap(false);
    _currbindDisp->setTextColor(SSD1306_WHITE);
    if(_currentlyTransitioning){
      if(_refreshText){
        if(!_txt1->useFixedName)
          _txt1->objdraw->onVisible(true);
        _refreshText = false;
        _startTransitionms = millis();

        int8_t _m = 1;
        switch(_transitionType){
          break; case nudge_left:
            _m = -1;
          
          case nudge_right:
            _drawTxt2 = false;
            _timeAnimation = bezier_nudgeAnimationTime;
            _dataAnimationBezier = bezier_nudgeAnimationData;
            _dataAnimationlen = bezier_nudgeAnimationDatalen;
            _txt1_startPosx = 0;
              _txt1_startPosy = 0;
            _txt1_finalPosx = _displaysizex*_m;
              _txt1_finalPosy = 0;

          break; case slide_left:
            _m = -1;

          case slide_right:
            _drawTxt2 = true;
            _timeAnimation = bezier_slideAnimationTime;
            _dataAnimationBezier = bezier_slideAnimationData;
            _dataAnimationlen = bezier_slideAnimationDatalen;
            _txt1_startPosx = -_displaysizex*_m;
              _txt1_startPosy = 0;
            _txt1_finalPosx = 0;
              _txt1_finalPosy = 0;
            _txt2_startPosx = 0;
              _txt2_startPosy = 0;
            _txt2_finalPosx = _displaysizex*_m;
              _txt2_finalPosy = 0;

          break; case back_forth:
            _drawTxt2 = true;
            _timeAnimation = bezier_slideAnimationTime;
            _dataAnimationBezier = bezier_slideAnimationData;
            _dataAnimationlen = bezier_slideAnimationDatalen;
            _txt1_startPosx = 0;
              _txt1_startPosy = _displaysizey;
            _txt1_finalPosx = 0;
              _txt1_finalPosy = 0;
            _txt2_startPosx = 0;
              _txt2_startPosy = 0;
            _txt2_finalPosx = 0;
              _txt2_finalPosy = _displaysizey;
          
          break; case nudge_back:
            _drawTxt2 = false;
            _timeAnimation = bezier_nudgeAnimationTime;
            _dataAnimationBezier = bezier_nudgeAnimationData;
            _dataAnimationlen = bezier_nudgeAnimationDatalen;
            _txt1_startPosx = 0;
              _txt1_startPosy = 0;
            _txt1_finalPosx = 0;
              _txt1_finalPosy = _displaysizey;
        }

        _getTextSize();
      }

      float _curranimtime = (float)(millis()-_startTransitionms)/(_timeAnimation*1000);
      _bezierAnimVal = bezier_func1D(
        _dataAnimationBezier,
        _dataAnimationlen,
        _curranimtime
      );

      if(_curranimtime >= 1.0f){
        _currentlyTransitioning = false;
        if(!_txt2->useFixedName)
          _txt2->objdraw->onVisible(false);
      }
      else if(_drawTxt2){
        int16_t _finposx = (int16_t)roundf(_bezierAnimVal*(_txt2_finalPosx-_txt2_startPosx)+_txt2_startPosx);
        int16_t _finposy = (int16_t)roundf(_bezierAnimVal*(_txt2_finalPosy-_txt2_startPosy)+_txt2_startPosy);
        if(_txt2->useFixedName){
          _currbindDisp->setTextSize(DISP_LIST_TXTSIZE);
          _currbindDisp->setCursor(
            (_displaysizex-_txtsizex2)/2 + _finposx,
            (_displaysizey-_txtsizey2)/2 + _finposy
          );
          _currbindDisp->printf(_txt2->name);
        }
        else{
          drawable_cbparam _param{
            .offsetx = _finposx,
            .offsety = _finposy,
            .sizex = _displaysizex,
            .sizey = _displaysizey,
            .disphandler = this,
            .deltams = _deltatimems,
            .islist = true,
            .isTransitioning = _currentlyTransitioning,
            .isCurrentDrawable = false
          };

          _txt2->objdraw->onDraw(_param);
        }
      }
    }

    int16_t _finposx1 = 0;
    int16_t _finposy1 = 0;
    if(_currentlyTransitioning){
      _finposx1 = (int16_t)roundf(_bezierAnimVal*(_txt1_finalPosx-_txt1_startPosx)+_txt1_startPosx);
      _finposy1 = (int16_t)roundf(_bezierAnimVal*(_txt1_finalPosy-_txt1_startPosy)+_txt1_startPosy);
    }


    drawable_cbparam _param{
      .offsetx = _finposx1,
      .offsety = _finposy1,
      .sizex = _displaysizex,
      .sizey = _displaysizey,
      .disphandler = this,
      .deltams = _deltatimems,
      .islist = true,
      .isTransitioning = _currentlyTransitioning,
      .isCurrentDrawable = true
    };

    if(_txt1->useFixedName){
      _currbindDisp->setTextSize(DISP_LIST_TXTSIZE);
      _currbindDisp->setCursor(
        (_displaysizex-_txtsizex1)/2 + _finposx1,
        (_displaysizey-_txtsizey1)/2 + _finposy1
      );
      _currbindDisp->printf(_txt1->name);
    }
    else
      _txt1->objdraw->onDraw(_param);
    
    _param.islist = false;
    for(auto _obj: _callbacksDrawList)
      _obj->onDraw(_param);


    // drawing box signs for border of the list
    if(_drawFlag & DISP_DRAWFLAG_LEFTBORDER){
      int16_t x1 = _displaysizex*_boxsign_width, y1 = _displaysizey*_boxsign_height;
      int16_t middle_y = (_displaysizey-y1)/2;

      _currbindDisp->fillRect(0, middle_y, x1, y1, SSD1306_WHITE);
    }
    
    if(_drawFlag & DISP_DRAWFLAG_RIGHTBORDER){
      int16_t x1 = _displaysizex*_boxsign_width, y1 = _displaysizey*_boxsign_height;
      int16_t middle_y = (_displaysizey-y1)/2;

      _currbindDisp->fillRect(_displaysizex-x1, middle_y, x1, y1, SSD1306_WHITE);
    }
    
    // checking important text
    _importanttxt_ui.onDraw(_param);
    _display();
  }
}

void displayHandler::_getTextSize(){
  if(_currbindDisp){
    int16_t _dmp = 0;
    _txtsizex1 = _txtsizex2 = _txtsizey1 = _txtsizey2 = 0;
    _currbindDisp->setTextSize(DISP_LIST_TXTSIZE);
    if(_txt1->useFixedName)
      _currbindDisp->getTextBounds(_txt1->name, 0, 0, &_dmp, &_dmp, &_txtsizex1, &_txtsizey1);

    Serial.printf("Txtsizex1 %d, Txtsizey1 %d\n", _txtsizex1, _txtsizey1);
    
    if(_txt2->useFixedName)
      _currbindDisp->getTextBounds(_txt2->name, 0, 0, &_dmp, &_dmp, &_txtsizex2, &_txtsizey2);
  }
}

void displayHandler::_clear(){
  switch(_dispType){
    break; case SSD1306:
      ((Adafruit_SSD1306*)_currbindDisp)->clearDisplay();
  }
}

void displayHandler::_display(){
  switch(_dispType){
    break; case SSD1306:
      ((Adafruit_SSD1306*)_currbindDisp)->display();
  }
}

int displayHandler::_getListIndex(int n){
  int _idx = _llListInteract->_current->listIndex;
  if(n != 0){
    do{
      _idx += (n > 0? 1: -1);
      if(_idx < 0 || _idx >= _llListInteract->_current->listinteract.size()){
        if(_llListInteract->_current->listLoop){
          if(_idx < 0)
            _idx += _llListInteract->_current->listinteract.size();
          else
            _idx = _idx % _llListInteract->_current->listinteract.size();
        }
        else
          return -1;
      }

      if(!_llListInteract->_current->listinteract[_idx].isSkipped)
        n += n > 0? -1: 1;
    }
    while(n != 0);
  }

  return _idx;
}


displayHandler::displayHandler(){
  _txt1 = _txt2 = &__dmpint;
  _callbacksDrawList.reserve(16);
  _timerid_display = timer_setInterval(1000/DISPLAY_REFRESHRATE, __onDisplayRefresh, this);
}

void displayHandler::bind_display(uint16_t sizex, uint16_t sizey, displayType disptype, Adafruit_GFX *disp){
  _displaysizex = sizex;
  _displaysizey = sizey;

  _dispType = disptype;
  _currbindDisp = disp;
}

void displayHandler::add_objectDraw(drawable *dobj){
  _callbacksDrawList.push_back(dobj);
}

void displayHandler::use_mainlist(listDisplay_Interact *list){
  while(_llListInteract){
    listInteract_linkedList *_l = _llListInteract;
    _llListInteract = _llListInteract->_before;
    delete _l;
  }

  _llListInteract = new listInteract_linkedList{
    ._before = NULL,
    ._current = list
  };

  _mainpos = list->listIndex;
  _txt1 = &list->listinteract[list->listIndex];
  _getTextSize();
}

void displayHandler::back_tomainlist(){
  if(_nlayersContent != 0 || _llListInteract->_current->listIndex != _mainpos){
    _currentlyTransitioning = true;
    _refreshText = true;
    _transitionType = nudge_back;
    if(!_txt2->useFixedName)
      _txt2->objdraw->onVisible(false);

    _txt2 = &__dmpint;
    _change_list(_nlayersContent, _mainpos-_llListInteract->_current->listIndex);
  }
}

void displayHandler::importanttext(const char *txt){
  _importanttxt_ui.print_text(txt);
}

Adafruit_GFX *displayHandler::getdisplay(){
  return _currbindDisp;
}

void displayHandler::pass_input(sensor_actType act){
  if(_txt1->useFixedName)
    _txt1->inputPass(act);
  else
    _txt1->objdraw->passInput(act);
}


void displayHandler::act_left(){
  if(!_currentlyTransitioning){
    Serial.printf("act_left\n");
    _currentlyTransitioning = true;
    _refreshText = true;

    if(_change_list(0, 1))
      _transitionType = slide_left;
    else
      _transitionType = nudge_left;

    pass_input(sensor_actType::ui_left);
  }
}

void displayHandler::act_right(){
  if(!_currentlyTransitioning){
    Serial.printf("act_right\n");
    _currentlyTransitioning = true;
    _refreshText = true;

    if(_change_list(0, -1))
      _transitionType = slide_right;
    else
      _transitionType = nudge_right;

    pass_input(sensor_actType::ui_right);
  }
}

void displayHandler::act_back(){
  if(!_currentlyTransitioning){
    pass_input(sensor_actType::ui_backing);
    
    Serial.printf("backing\n");
    _currentlyTransitioning = true;
    _refreshText = true;
    if(_change_list(1, 0))
      _transitionType = back_forth;
    else
      _transitionType = nudge_back;
  }
}

void displayHandler::act_accept(){
  if(!_currentlyTransitioning){
    pass_input(sensor_actType::ui_accept);

    listDisplay_Interact *_nextlist = NULL;

    auto &_currentinteract = _llListInteract->_current->listinteract[_llListInteract->_current->listIndex];
    auto *funcp = _currentinteract.onClicked;
    if(funcp != NULL)
      funcp(&_nextlist, _currentinteract.additionalObj);

    _currentlyTransitioning = true;
    _refreshText = true;

    if(_nextlist){
      _transitionType = back_forth;
      _txt2 = _txt1;
      _nlayersContent++;

      listInteract_linkedList *_list = new listInteract_linkedList{
        ._before = _llListInteract,
        ._current = _nextlist
      };
      _llListInteract = _list;

      _txt1 = &_llListInteract->_current->listinteract[_llListInteract->_current->listIndex];

      _check_drawborder(_llListInteract->_current->listIndex);
    }
    else{
      _transitionType = nudge_back;
    }
  }
}