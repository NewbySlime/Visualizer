#ifndef _DISPLAY_HEADER
#define _DISPLAY_HEADER

#include "Arduino.h"
#include "Adafruit_GFX.h"

#include "vector"
#include "string"

#include "images/drawable.hpp"

#include "input_sensor.hpp"


#define DISPLAY_REFRESHRATE 30
#define _classcallback(type, classtype, namefunc) static type _##namefunc(void *obj){((classtype*)obj)->namefunc();}\
type namefunc();


struct listDisplay_Interact{
  struct _interactData{
    void (*onClicked)(listDisplay_Interact **datalist, void *obj);
    void *additionalObj;

    bool useFixedName = true;
    const char *name;

    bool isSkipped = true;

    // will be used when the display is passing input
    // but can be as NULL
    void (*inputPass)(sensor_actType act);

    // will be used if useFixedName is false
    drawable *objdraw;
  };

  std::vector<_interactData> listinteract;
  size_t listIndex;

  // if this is on, then the list will loop itself
  bool listLoop = false;

  // since all double click will redirect to previous list, this will not be used
  // the handler will use linked list instead
  //void (*onDoubleClicked)();
};

enum displayType{
  SSD1306
};


class draw_importanttxt: public drawable{
  private:
    std::pair<const float*, uint16_t> _animdata;

    const char *_currenttxt;
    bool _dodraw = false, _doreset = false, _drawmain = false, _refreshtxt = false, _animtxt = false, _drawtxt = true, _animtxtreverse = false;
    size_t _currenttime = 0;
    size_t _idxInterval = 0;

    int16_t _barheight = 0;

    uint16_t _txtheight = 0;
    uint16_t _txtwidth_main = 0;

  public:
    draw_importanttxt();
    void onDraw(drawable_cbparam &param);
    void print_text(const char *txt);
};

class displayHandler{
  private:
    enum _typeTransisition{
      slide_left,
      slide_right,
      back_forth,
      nudge_left,
      nudge_right,
      nudge_back
    };

    struct listInteract_linkedList{
      listInteract_linkedList *_before;
      listDisplay_Interact *_current;
    };


    Adafruit_GFX *_currbindDisp = NULL;
    displayType _dispType;
    listInteract_linkedList *_llListInteract = NULL;
    size_t _nlayersContent = 0;
    uint16_t _displaysizex = 0, _displaysizey = 0;

    size_t _mainpos = 0;
    const listDisplay_Interact::_interactData *_txt1, *_txt2;

    draw_importanttxt _importanttxt_ui;
    std::vector<drawable*> _callbacksDrawList;
    int _timerid_display = 0;
    unsigned long _timelastUpdatems = 0;

    _typeTransisition _transitionType;
    size_t _startTransitionms = 0;
    bool _currentlyTransitioning = false;
    bool _refreshText = true;

    float _timeAnimation = 0;
    const float *_dataAnimationBezier;
    size_t _dataAnimationlen = 0;
    bool _drawTxt2 = true;

    uint16_t _drawFlag;

    uint16_t _txtsizex1 = 0, _txtsizey1 = 0;
    uint16_t _txtsizex2 = 0, _txtsizey2 = 0;

    int16_t _txt1_startPosx = 0, _txt1_finalPosx = 0;
    int16_t _txt1_startPosy = 0, _txt1_finalPosy = 0;

    int16_t _txt2_startPosx = 0, _txt2_finalPosx = 0;
    int16_t _txt2_startPosy = 0, _txt2_finalPosy = 0;

    void _check_drawborder(int idx);
    bool _change_list(size_t contentLayerdec, int32_t addtionN);
    void _clear();
    void _display();
    void _getTextSize();
    _classcallback(void, displayHandler, _onDisplayRefresh);

    // based on current index on list
    int _getListIndex(int n);

  public:
    displayHandler();

    void bind_display(uint16_t sizex, uint16_t sizey, displayType disptype, Adafruit_GFX *disp);
    void add_objectDraw(drawable *dobj);
    void use_mainlist(listDisplay_Interact *list);
    void back_tomainlist();
    void importanttext(const char *txt);
    Adafruit_GFX *getdisplay();

    // this will pass the input to current listDisplay_Interact
    void pass_input(sensor_actType act);

    _classcallback(void, displayHandler, act_left);
    _classcallback(void, displayHandler, act_right);
    _classcallback(void, displayHandler, act_back);
    _classcallback(void, displayHandler, act_accept);
};

#endif