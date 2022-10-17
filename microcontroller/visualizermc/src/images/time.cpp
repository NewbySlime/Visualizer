#include "drawables.hpp"

#include "display/display.hpp"

#define _time_margintoolbartb 1

#define _time_txtsize 2
#define _time_txtsizemini 1
#define _date_txtsize 1

#define _seperatorlen 1

#define ColorWhite 1

#define UPDATE_1S 1000
#define UPDATE_TIME 60000

const char PROGMEM *daystr[] = {
  "Mon",
  "Tue",
  "Wed",
  "Thu",
  "Fri",
  "Sat",
  "Sun"
};

const char PROGMEM *monthstr[] = {
  "Jan",
  "Feb",
  "Mar",
  "Apr",
  "May",
  "June",
  "July",
  "Aug",
  "Sept",
  "Oct",
  "Nov",
  "Dec"
};


void draw_time::onRefreshText(Adafruit_GFX *disp){
  int16_t _dmp = 0;

  disp->setTextSize(_time_txtsizemini);
  disp->getTextBounds(_char_timeClock, 0, 0, &_dmp, &_dmp, &_txtTimeClock_widthmini, &_txtTimeClock_heightmini);
}

void draw_time::_onupdate1s(){
  _time1s = UPDATE_1S - (_time1s % UPDATE_1S);
  _drawmiddle = !_drawmiddle;
}

void draw_time::_onupdatetime(){
  _timeup = UPDATE_TIME;
  
  // get time data from rtc
  // and _timeup will be set based on the second
}


draw_time::draw_time(){
  useColor(SSD1306_WHITE, SSD1306_WHITE);
  useSize(_time_txtsize, _date_txtsize);
  changeTextType(Top_Bottom);
  
  _time1s = UPDATE_1S;
  _timeup = UPDATE_TIME;
}

void draw_time::onDraw(drawable_cbparam &param){
  Adafruit_GFX *disp = ((displayHandler*)param.disphandler)->getdisplay();

  _time1s -= param.deltams;
  if(_time1s <= 0)
    _onupdate1s();
  
  _timeup -= param.deltams;
  if(_timeup <= 0)
    _onupdatetime();

  sprintf(_char_timeClock, "%.2d%c%.2d", _hour, ' ' + (26 * _drawmiddle), _minute);

  if(!_usemini && param.islist)
    draw_text::onDraw(param);
  else if(_usemini){
    disp->setTextColor(ColorWhite);
    disp->setTextSize(_time_txtsizemini);
    disp->setCursor((param.sizex/2)-(_txtTimeClock_widthmini/2), _time_margintoolbartb);
    disp->print(_char_timeClock);
  }
}

void draw_time::settime(const DateTime& time){
  // for setting up the time clock
  _minute = time.minute();
  _hour = time.hour();

  // for setting up the date
  sprintf(_char_timeDate, "%s, %d %s",
    daystr[time.dayOfTheWeek()],
    time.day(),
    monthstr[time.month()]
  );

  useText(_char_timeClock, _char_timeDate);
}

void draw_time::onVisible(bool visible){
  _usemini = !visible;
}

void draw_time::drawmiddlechar(bool _b){
  _drawmiddle = _b;
}

void draw_time::bindRTC(RTC_I2C *rtc){

}