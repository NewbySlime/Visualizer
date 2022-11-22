#include "drawables.hpp"

#define LOADAPPLY_DOT_COUNT 3
#define LOADAPPLY_DOT_UPDATETIME 700


void __fill_txt_wdots(char *txt, size_t txtlen){
  for(int i = 0; i < LOADAPPLY_DOT_COUNT; i++)
    txt[txtlen+1+i] = '.';
}


// actually, the one that swapped around is the null terminating character
void __change_dotpos(char *txt, size_t txtlen, size_t prevdotidx, size_t nextdotidx){
  Serial.printf("changing from %d %d, to %d %d\n", prevdotidx, txt[txtlen+prevdotidx], nextdotidx, txt[txtlen+nextdotidx]);

  char _tmp = txt[txtlen+prevdotidx];
  txt[txtlen+prevdotidx] = txt[txtlen+nextdotidx];
  txt[txtlen+nextdotidx] = _tmp;
}


draw_loadapply::draw_loadapply(){
  useText(NULL, NULL);

  _txt_applied = NULL;
  _txt_cancelled = NULL;

  useDotLoad(false, false);
  _dotidx = 0;

  _loading_state = false;
  onNormal();

  Idraw_interval::setInterval(LOADAPPLY_DOT_UPDATETIME);
}

void draw_loadapply::onDraw(drawable_cbparam &param){
  Idraw_interval::update(param);

  if(Idraw_interval::_is_interval() && _loading_state){
    size_t _newdotidx = (_dotidx+1) % (LOADAPPLY_DOT_COUNT+1);
    if(__txt1_usedots)
      __change_dotpos(__txt1, __txt1len, _dotidx, _newdotidx);
    
    if(__txt2_usedots)
      __change_dotpos(__txt2, __txt2len, _dotidx, _newdotidx);
    
    _dotidx = _newdotidx;
    setRefreshFlag();
  }

  draw_text::onDraw(param);
}

void draw_loadapply::onVisible(bool visible){
  if(!visible && !_loading_state)
    onNormal();
}

void draw_loadapply::useText(const char *txt1, const char *txt2){
  useText1(txt1);
  useText2(txt2);
}

void draw_loadapply::useText1(const char *txt){
  if(!txt)
    txt = "";

  __txt1len = strlen(txt);
  __txt1 = (char*)realloc(__txt1, __txt1len + LOADAPPLY_DOT_COUNT + 1);
  strcpy(__txt1, txt);

  __txt1[__txt1len] = '\0';
  __fill_txt_wdots(__txt1, __txt1len);
  draw_text::useText1(__txt1);

  if(__txt1_usedots)
    __change_dotpos(__txt1, __txt1len, 0, _dotidx);
}

void draw_loadapply::useText2(const char *txt){
  if(!txt)
    txt = "";

  __txt2len = strlen(txt);
  __txt2 = (char*)realloc(__txt2, __txt2len + LOADAPPLY_DOT_COUNT + 1);
  strcpy(__txt2, txt);

  __txt2[__txt2len] = '\0';
  __fill_txt_wdots(__txt2, __txt2len);

  if(_loading_state)
    draw_text::useText2(__txt2);

  if(__txt2_usedots)
    __change_dotpos(__txt2, __txt2len, 0, _dotidx);
}

void draw_loadapply::useDotLoad(bool txt1, bool txt2){
  if(__txt1_usedots != txt1){
    // put the null terminating character based on the dot idx
    if(txt1)
      __change_dotpos(__txt1, __txt1len, 0, _dotidx);

    // put the null terminating character based on the string length
    else
      __change_dotpos(__txt1, __txt1len, _dotidx, 0);
  }

  if(__txt2_usedots != txt2){
    if(txt2)
      __change_dotpos(__txt2, __txt2len, 0, _dotidx);
    else
      __change_dotpos(__txt2, __txt2len, _dotidx, 0);
  }

  __txt1_usedots = txt1;
  __txt2_usedots = txt2;
}

void draw_loadapply::useText_applied(const char *txt){
  _txt_applied = txt;
}

void draw_loadapply::useText_cancelled(const char *txt){
  _txt_cancelled = txt;
}

void draw_loadapply::onApplied(){
  draw_text::useText2(_txt_applied);
  onDoneLoading();
}

void draw_loadapply::onCancelled(){
  draw_text::useText2(_txt_cancelled);
  onDoneLoading();
}

void draw_loadapply::onNormal(){
  draw_text::useText2(NULL);
}

void draw_loadapply::onLoading(){
  _loading_state = true;
  _dotidx = 0;

  draw_text::useText2(__txt2);
}

void draw_loadapply::onDoneLoading(){
  _loading_state = false;
  
  if(__txt1_usedots)
    __change_dotpos(__txt1, __txt1len, _dotidx, 0);
  
  if(__txt2_usedots)
    __change_dotpos(__txt2, __txt2len, _dotidx, 0);
}