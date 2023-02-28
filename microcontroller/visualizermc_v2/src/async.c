#include "async.h"
#include "defines.h"
#include "peripheral.h"

#if defined(STM32XX)

#if ((F_CPU/10000) > 0xFFFF)
#error CPU clock is too high for the program to use async functionality. The timing is not accurate.
#elif ((F_CPU/1000) > 0xFFFF)
#define ASYNC_TIMER_PSC_OFFSETMULT 10
#else
#define ASYNC_TIMER_PSC_OFFSETMULT 1
#endif

#define ASYNC_TIMER_PSCR_MS F_CPU / (1000*ASYNC_TIMER_PSC_OFFSETMULT) - 1

#define ASYNC_TIMER_CCR(ccr) *((volatile uint32_t*)((uint32_t)&ASYNC_TIMER->CCR1 + ((ccr)*sizeof(uint32_t))))
#define ASYNC_TIMER_CCMR(n) *((volatile uint32_t*)((uint32_t)&ASYNC_TIMER->CCMR1 + ((n)/2*sizeof(uint32_t))))
#define ASYNC_TIMER_CCMR_SETVALUE(ccr, value) ((value) << (((ccr)%2)*8))


uint8_t _polling_cbcount = 0;
async_callback _polling_cb[POLLING_CALLBACK_COUNT];
void *_polling_cb_data[POLLING_CALLBACK_COUNT];

// NOTE: timer id always greater or equal than 1
volatile uint8_t _timer_running = 0;
volatile uint8_t _timer_doneid = 0;
volatile async_callback_timer1 _timer_cb[TIMER_CALLBACK_COUNT];
volatile async_callback_timer2 _timer_cb_bypass[TIMER_CALLBACK_COUNT];
volatile void *_timer_cb_data[TIMER_CALLBACK_COUNT];

volatile bool _timer_isinterval[TIMER_CALLBACK_COUNT];
volatile bool _timer_isrun[TIMER_CALLBACK_COUNT];
volatile uint32_t _timer_time[TIMER_CALLBACK_COUNT];


bool __async_timer_cbdump1(void *_obj){return true;}
void __async_timer_cbdump2(void *_obj){}

uint8_t _async_timer_getid(){
  for(uint8_t i = 0; i < TIMER_CALLBACK_COUNT; i++)
    if(!(_timer_running & (1 << i)))
      return i+1;

  return 0;
}

void ASYNC_TIMER_ISR(){
  _timer_doneid |= ASYNC_TIMER->SR;
  _timer_doneid &= _timer_running;
  ASYNC_TIMER->SR = 0;

  for(uint8_t i = 0; i < TIMER_CALLBACK_COUNT; i++){
    bool _done = (_timer_doneid & (1 << (i+1))) > 0;

    if(_done){
      if(_timer_isinterval[i])
        ASYNC_TIMER_CCR(i) += _timer_time[i];    // the offset is always 0x4 each CCR
      else
        async_timer_stop(i+1);

      _timer_cb_bypass[i](_timer_cb_data[i]);
    }
  }
}


void async_setup(){
  ASYNC_TIMER->PSC = ASYNC_TIMER_PSCR_MS;
  ASYNC_TIMER->ARR = UINT32_MAX;

  ASYNC_TIMER->CR1 = TIM_CR1_CEN;

  for(uint8_t i = 0; i < TIMER_CALLBACK_COUNT; i++){
    ASYNC_TIMER_CCMR(i) &= ~ASYNC_TIMER_CCMR_SETVALUE(i, TIM_CCMR1_OC1M_Msk);

    _timer_cb[i] = __async_timer_cbdump1;
    _timer_cb_bypass[i] = __async_timer_cbdump2;
  }
  
  set_peripheral_interrupt(pt_tim16 | ASYNC_TIMER_NUM, ASYNC_TIMER_ISR, NULL);
}


void async_polling_update(){
  for(uint8_t i = 0; i < _polling_cbcount; i++)
    _polling_cb[i](_polling_cb_data[i]);
}

void async_timer_update(){
  for(uint8_t i = 0; i < TIMER_CALLBACK_COUNT; i++){
    if(_timer_isrun[i])
      continue;

    uint8_t _id = 1 << (i+1);
    if((_timer_doneid & _id)){
      _timer_isrun[i] = true;
      if(_timer_cb[i](_timer_cb_data[i]))
        _timer_doneid &= ~_id;
      
      _timer_isrun[i] = false;
    }
  }
}

void timer_wait(uint16_t time){
  uint16_t _nexttime = ASYNC_TIMER->CNT + time*ASYNC_TIMER_PSC_OFFSETMULT;

  // if the variable _nexttime is overflown
  while(ASYNC_TIMER->CNT > _nexttime)
    ;
  
  while(ASYNC_TIMER->CNT < _nexttime)
    ;
}

uint16_t timer_get_delta(uint16_t *time){
  uint16_t _current = ASYNC_TIMER->CNT;

  uint16_t _deltatime = *time;
  // if overflowed since last time
  if(_current < _deltatime)
    _deltatime = (0xFFFF-_deltatime) + _current;
  else
    _deltatime = _current-_deltatime;
  
  *time = _current;
  return _deltatime/ASYNC_TIMER_PSC_OFFSETMULT;
}


void async_polling_add(async_callback cb, void *data){
  _polling_cb[_polling_cbcount] = cb;
  _polling_cb[_polling_cbcount] = data;
  _polling_cbcount++;
}

uint8_t async_timer_add(async_callback_timer1 cb, void *data, uint32_t time, bool isinterval){
  uint8_t timerid = _async_timer_getid();
  if(timerid){
    uint8_t _timerid = timerid-1;
    uint8_t _id = 1 << timerid;

    time *= ASYNC_TIMER_PSC_OFFSETMULT;

    _timer_running |= _id;
    _timer_doneid &= ~_id;
    _timer_cb[_timerid] = cb;
    _timer_cb_data[_timerid] = data;
    _timer_cb_bypass[_timerid] = __async_timer_cbdump1;

    _timer_isinterval[_timerid] = isinterval;
    _timer_time[_timerid] = time;

    ASYNC_TIMER->DIER |= _id;
    ASYNC_TIMER_CCR(_timerid) = ASYNC_TIMER->ARR + time;
  }

  return timerid;
}


void async_timer_bypass(uint8_t timerid, async_callback_timer2 cb){
  if(timerid)
    _timer_cb_bypass[timerid-1] = cb;
}

void async_timer_stop(uint8_t timerid){
  if(timerid){
    timerid -= 1;
    uint8_t _id = (1 << timerid);

    ASYNC_TIMER->DIER &= ~_id;
    _timer_running &= ~_id;
  }
}

#endif