#include "drawable.hpp"



/*      Idraw_interval class      */
bool Idraw_interval::_is_interval(){
  bool b = _interval;
  _interval = false;

  return b;
}

void Idraw_interval::update(drawable_cbparam &param){
  time_ms -= param.deltams;
  if(time_ms <= 0){
    _interval = true;
    time_ms = time_start - (time_ms % time_start);
  }
}

void Idraw_interval::setInterval(int16_t ms){
  time_start = ms;
  time_ms = ms;
}