#include "polling.h"

#include "Arduino.h"

struct _pollingInfo{
  PollingCallbackArg cb;
  void *obj;
};


std::vector<_pollingInfo> _pollingFunctions;

bool operator<(const _pollingInfo &_pi1, const _pollingInfo &_pi2){
  return _pi1.cb < _pi2.cb || (_pi1.cb == _pi2.cb && _pi1.obj < _pi2.obj);
}

bool operator==(const _pollingInfo &_pi1, const _pollingInfo &_pi2){
  return _pi1.cb == _pi2.cb && _pi1.obj == _pi2.obj;
}


bool polling_addfunc(PollingCallbackArg cb, void *obj){
  _pollingInfo _pi{
    .cb = cb,
    .obj = obj
  };

  auto _iter = std::lower_bound(_pollingFunctions.begin(), _pollingFunctions.end(), _pi);

  _pollingFunctions.insert(_iter, _pi);
  return true;
}

bool polling_removefunc(PollingCallbackArg cb, void *obj){
  _pollingInfo _pi{
    .cb = cb,
    .obj = obj
  };

  auto _iter = std::find(_pollingFunctions.begin(), _pollingFunctions.end(), _pi);

  if(_iter == _pollingFunctions.end())
    return false;
  
  _pollingFunctions.erase(_iter);
  return true;
}


void polling_update(){
  for(auto pi: _pollingFunctions)
    pi.cb(pi.obj);
}