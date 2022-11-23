#include "timer.hpp"
#include "Arduino.h"

#define _MAX_TIMERCOUNT 32

unsigned long _timePrevious = 0;
int _timerDelta[_MAX_TIMERCOUNT];

// if the high timer is below 0, then that means timer isn't used
int _timerDelta_Hi[_MAX_TIMERCOUNT];

TimerCallbackArg _timerCallback[_MAX_TIMERCOUNT];
void *_timerObj[_MAX_TIMERCOUNT];

char _timerTimeout[_MAX_TIMERCOUNT/8 + ((_MAX_TIMERCOUNT%8) > 0? 1: 0)];


bool _timerTimeout_getFlag(int id){
  return (_timerTimeout[id/8] & (1 << (id%8))) > 0;
}

void _timerTimeout_setFlag(int id, bool _flag){
  char _id = (1 << (id%8));

  if(_flag)
    _timerTimeout[id/8] |= _id;
  else
    _timerTimeout[id/8] &= ~_id;
}

int _getSmallestId(){
  for(int i = 0; i < _MAX_TIMERCOUNT; i++)
    if(_timerDelta_Hi[i] < 0)
      return i;
  
  return -1;
}

int timer_setTimer(unsigned long delay, TimerCallbackArg cbarg, void *arg, bool oneshot){
  int _newid = _getSmallestId();
  if(_newid >= 0){
    _timerDelta[_newid] = delay;
    _timerDelta_Hi[_newid] = delay;

    _timerCallback[_newid] = cbarg;
    _timerObj[_newid] = arg;
    _timerTimeout_setFlag(_newid, oneshot);
  }

  return _newid;
}

int timer_setInterval(unsigned long delay, TimerCallbackArg cbarg, void *arg){
  return timer_setTimer(delay, cbarg, arg, false);
}

int timer_setTimeout(unsigned long delay, TimerCallbackArg cbarg, void *arg){
  return timer_setTimer(delay, cbarg, arg, true);
}

bool timer_changeInterval(unsigned int timer_id, unsigned long delay){
  if(timer_id >= _MAX_TIMERCOUNT || _timerDelta_Hi[timer_id] < 0)
    return false;
  
  _timerDelta_Hi[timer_id] = delay;
  return true;
}

void timer_deleteTimer(unsigned int timer_id){
  timer_changeInterval(timer_id, -1);
}

void timer_update(){
  unsigned long _currtime = millis();
  unsigned long _deltatime = 0;
  if(_currtime < _timePrevious)
    _deltatime = _currtime + (UINT32_MAX - _timePrevious);
  else
    _deltatime = _currtime - _timePrevious;

  _timePrevious = _currtime;

  for(int i = 0; i < _MAX_TIMERCOUNT; i++){
    if(_timerDelta_Hi[i] >= 0){
      int _currtimer = _timerDelta[i] - _deltatime;
      if(_currtimer < 0){
        if(_timerTimeout_getFlag(i))
          _timerDelta_Hi[i] = -1;
        else
          _currtimer = _timerDelta_Hi[i] + _deltatime;

        _timerCallback[i](_timerObj[i]);
      }

      _timerDelta[i] = _currtimer;
    }
  }
}

int _timerSetup(){
  for(int i = 0; i < _MAX_TIMERCOUNT; i++)
    _timerDelta_Hi[i] = -1;
  
  return 0;
}

int __timerSetup = _timerSetup();