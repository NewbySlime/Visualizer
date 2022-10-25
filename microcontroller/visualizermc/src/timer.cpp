#include "timer.hpp"

#ifdef SOFTWARE_TIMERS

// if using hardware timers
#else

#include "ESP8266TimerInterrupt.h"
#include "ESP8266_ISR_Timer.h"

ESP8266TimerInterrupt Timer;
ESP8266_ISR_Timer ISR_Timer;


TimerCallbackArg cbcont[MAX_NUMBER_TIMERS];
void* cobjcont[MAX_NUMBER_TIMERS];
int idcont[MAX_NUMBER_TIMERS];
volatile unsigned short _flags = 0;
volatile unsigned short _deleteFlags = 0;

// this should only raise the flag
void IRAM_ATTR _onTimeout(void *param){
  int idtimer = *(int*)param;
  _flags |= (1 << idtimer);
}

void IRAM_ATTR _onTimeoutT(void *param){
  int idtimer = *(int*)param;

  _flags |= (1 << idtimer);
  _deleteFlags |= (1 << idtimer);
}

int getidcont_firstidx(){
  for(int i = 0; i < MAX_NUMBER_TIMERS; i++)
    if(idcont[i] == -1)
      return i;
  
  return -1;
}

int timer_setInterval(unsigned long delay, TimerCallbackArg cb, void *cobj){
  int idcontidx = getidcont_firstidx();
  if(idcontidx == -1)
    return -1;
  
  int timer_idx = ISR_Timer.setInterval(delay, _onTimeout, &idcont[idcontidx]);
  cbcont[timer_idx] = cb;
  cobjcont[timer_idx] = cobj;
  idcont[idcontidx] = timer_idx;

  return timer_idx;
}

int timer_setTimeout(unsigned long delay, TimerCallbackArg cb, void *cobj){
  int idcontidx = getidcont_firstidx();
  if(idcontidx == -1)
    return -1;

  int timer_idx = idcont[idcontidx] = ISR_Timer.setTimeout(delay, _onTimeoutT, &idcont[idcontidx]);
  cbcont[timer_idx] = cb;
  cobjcont[timer_idx] = cobj;
  
  return timer_idx;
}

bool timer_changeInterval(unsigned int timer_id, unsigned long delay){
  return ISR_Timer.changeInterval(timer_id, delay);
}

void timer_deleteTimer(unsigned int timer_id){
  if(timer_id < MAX_NUMBER_TIMERS){
    ISR_Timer.deleteTimer(timer_id);

    if(idcont[timer_id] != -1){
      idcont[timer_id] = -1;
    }

    _flags &= ~(1 << timer_id);
  }
}

// if a flag is raised, then run it
// only run it on loop() or any function that's not in interrupt context
void timer_update(){
  if(_flags > 0){
    //Serial.printf("flags: 0x%X\n", _flags);
    for(char i = 0; i < MAX_NUMBER_TIMERS; i++){
      if((_deleteFlags & (1 << i)) > 0){
        idcont[i] = -1;
        _deleteFlags &= ~(1 << i);
      }

      if((_flags & (1 << i)) > 0){
        _flags &= ~(1 << i);   
        cbcont[i](cobjcont[i]);
      }
    }
  }
}


void IRAM_ATTR _onInterval(){
  ISR_Timer.run();
}

int timer_init(){
  for(int i = 0; i < MAX_NUMBER_TIMERS; i++)
    idcont[i] = -1;

  Timer.attachInterruptInterval(1000, _onInterval);
  return 0;
}

int __tmp = timer_init();

#endif