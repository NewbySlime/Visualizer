#include "timer.hpp"
#include "vector"

#include "ESP8266TimerInterrupt.h"
#include "ESP8266_ISR_Timer.h"

ESP8266TimerInterrupt Timer;
ESP8266_ISR_Timer ISR_Timer;


std::vector<TimerCallbackArg> cbcont;
std::vector<int*> idcont;
std::vector<void*> cobjcont;
volatile unsigned short _flags = 0;

// this should only raise the flag
void IRAM_ATTR _onTimeout(void *param){
  int idtimer = *(int*)param;
  _flags |= (1 << idtimer);
}

int timer_setInterval(unsigned long delay, TimerCallbackArg cb, void *cobj){
  int *idTimer = new int();
  *idTimer = ISR_Timer.setInterval(delay, _onTimeout, idTimer);
  if(*idTimer >= 0){
    cbcont[*idTimer] = cb;
    cobjcont[*idTimer] = cobj;
    idcont[*idTimer] = idTimer;
  }

  return *idTimer;
}

bool timer_changeInterval(unsigned int timer_id, unsigned long delay){
  return ISR_Timer.changeInterval(timer_id, delay);
}

void timer_deleteTimer(unsigned int timer_id){
  if(timer_id < MAX_NUMBER_TIMERS){
    ISR_Timer.deleteTimer(timer_id);
    cobjcont[timer_id] = NULL;

    if(idcont[timer_id] != NULL){
      delete idcont[timer_id];
      idcont[timer_id] = NULL;
    }
  }
}

// if a flag is raised, then run it
// only run it on loop() or any function that's not in interrupt context
void timer_update(){
  if(_flags > 0){
    //Serial.printf("flags: 0x%X\n", _flags);
    for(char i = 0; i < MAX_NUMBER_TIMERS; i++){
      if((_flags & (1 << i)) > 0){
        cbcont[i](cobjcont[i]);
        _flags &= ~(1 << i);   
      }
    }
  }
}


void IRAM_ATTR _onInterval(){
  ISR_Timer.run();
}

int timer_init(){
  idcont.resize(MAX_NUMBER_TIMERS, NULL);
  cbcont.resize(MAX_NUMBER_TIMERS);
  cobjcont.resize(MAX_NUMBER_TIMERS, NULL);
  Timer.attachInterruptInterval(1000, _onInterval);
  return 0;
}

int __tmp = timer_init();