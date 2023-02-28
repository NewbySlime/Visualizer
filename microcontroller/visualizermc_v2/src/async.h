#ifndef ASYNC_HEADER
#define ASYNC_HEADER

#include "stdint.h"
#include "stdbool.h"

#include "defines.h"


#if defined(STM32XX)
#define POLLING_CALLBACK_COUNT 8
#define TIMER_CALLBACK_COUNT 4
#endif

#define YIELDWHILE(b)\
  while(b){\
    async_timer_update();\
    async_polling_update();\
  }


typedef void (*async_callback)(void*);
typedef bool (*async_callback_timer1)(volatile void*);
typedef void (*async_callback_timer2)(volatile void*);

void async_setup();

void async_polling_update();
void async_timer_update();
void timer_wait(uint16_t ms);
uint16_t timer_get_delta(uint16_t *lastms);

void async_polling_add(async_callback cb, void *data);
uint8_t async_timer_add(async_callback_timer1 cb, void *data, uint32_t ms, bool isinterval);

void async_timer_bypass(uint8_t timerid, async_callback_timer2 cb);
void async_timer_stop(uint8_t timerid);

#endif