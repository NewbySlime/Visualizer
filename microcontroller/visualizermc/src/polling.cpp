#include "polling.h"
#include "map"

#include "Arduino.h"

std::map<unsigned long long, void(*)(void*)> _functions;

bool polling_addfunc(void(*fn)(void*), void *obj){
  _functions[(unsigned long long)obj] = fn;
  return true;
}

bool polling_removefunc(void *obj){
  if(_functions.find((unsigned long long)obj) == _functions.end())
    return false;
  
  _functions.erase((unsigned long long)obj);
  return true;
}


void polling_update(){
  for(auto p: _functions)
    p.second((void*)p.first);
}