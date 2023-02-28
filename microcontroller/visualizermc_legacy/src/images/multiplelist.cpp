#include "drawables.hpp"

draw_multiplelist::draw_multiplelist(){
  _liststr = (std::pair<const char*, const char*>*)malloc(0);
  _liststrlen = 0;
  _curridx = 0;
}

draw_multiplelist::~draw_multiplelist(){
  free(_liststr);
}

void draw_multiplelist::_setCurrentIndex(int idx){
  useText(_liststr[idx].first, _liststr[idx].second);
}

void draw_multiplelist::onDraw(drawable_cbparam &param){
  if(param.isTransitioning){
    _stillTransitioning = true;
    if(param.isCurrentDrawable)
      _setCurrentIndex(_curridx);
    else
      _setCurrentIndex(_beforeidx);
  }

  // set the text to curridx just in case
  else if(_stillTransitioning){
    _stillTransitioning = false;
    _setCurrentIndex(_curridx);
  }

  draw_text::onDraw(param);
}

void draw_multiplelist::passInput(sensor_actType act){
  switch(act){
    break; case sensor_actType::ui_accept:{
      if(_cb1)
        _cb1(_curridx);
    }

    break; case sensor_actType::ui_left:{
      _beforeidx = _curridx;
      _curridx -= 1;
      if(_curridx < 0)
        _curridx += _liststrlen; 
    }

    break; case sensor_actType::ui_right:{
      _beforeidx = _curridx;
      _curridx = (_curridx + 1) % _liststrlen;
    }
  }

  Serial.printf("_curridx %d _beforeidx %d\n", _curridx, _beforeidx);
}

void draw_multiplelist::setCurrentIndex(int idx){
  if(idx < 0 || idx >= _liststrlen)
    return;

  _setCurrentIndex(idx);
}

void draw_multiplelist::setStr(int idx, const char *str1, const char *str2){
  if(idx < 0 || idx >= _liststrlen)
    return;

  _liststr[idx] = {str1, str2};
  if(idx == _curridx)
    setRefreshFlag();
}

void draw_multiplelist::setStr1(int idx, const char *str){
  if(idx < 0 || idx >= _liststrlen)
    return;
  
  _liststr[idx].first = str;
  if(idx == _curridx)
    setRefreshFlag();
}

void draw_multiplelist::setStr2(int idx, const char *str){
  if(idx < 0 || idx >= _liststrlen)
    return;
  
  _liststr[idx].second = str;
  if(idx == _curridx)
    setRefreshFlag();
}

void draw_multiplelist::setCallbackOnAccept(OnAcceptCallback cb){
  _cb1 = cb;
}

void draw_multiplelist::resizeList(int newsize){
  if(newsize <= 0)
    return;

  _liststrlen = newsize;
  _liststr = (std::pair<const char*, const char*>*)malloc(_liststrlen);

  if(_curridx >= _liststrlen){
    _curridx = _liststrlen-1;
    _setCurrentIndex(_curridx);
  }
}