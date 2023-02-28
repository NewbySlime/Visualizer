#include "peripheral.h"
#include "stdbool.h"


void __peripheral_interrupt_dmp(void* _obj){}

#ifdef STM32XX
#define __INTERRUPT_NAME(name) _##name##_it_cb
#define __INTERRUPTDATA_NAME(name) _##name##_it_data

#define __CREATE_INTERRUPT2(name, n)\
volatile peripheral_interrupt_cb __INTERRUPT_NAME(name##n) = __peripheral_interrupt_dmp;\
volatile void *__INTERRUPTDATA_NAME(name##n);\
void name##n##_IRQHandler() __attribute__((interrupt));\
void name##n##_IRQHandler(){\
  __INTERRUPT_NAME(name##n)(__INTERRUPTDATA_NAME(name##n));\
}

#define __CREATE_INTERRUPT3(name, n, back)\
volatile peripheral_interrupt_cb __INTERRUPT_NAME(name##n##back) = __peripheral_interrupt_dmp;\
volatile void *__INTERRUPTDATA_NAME(name##n##back);\
void name##n##back##_IRQHandler() __attribute((interrupt));\
void name##n##back##_IRQHandler(){\
  __INTERRUPT_NAME(name##n##back)(__INTERRUPTDATA_NAME(name##n##back));\
}

#define ___CREATE_INTERRUPTMACRO_RUN(_1, _2, _3, _macro, ...) _macro
#define __CREATE_INTERRUPT(name, ...)\
  ___CREATE_INTERRUPTMACRO_RUN(name, __VA_ARGS__, __CREATE_INTERRUPT3, __CREATE_INTERRUPT2)(name, __VA_ARGS__)
  
  
#define __SWITCH_SETINTERRUPT2(name, n, val, data)\
  break; case n:\
    __INTERRUPT_NAME(name##n) = val;\
    __INTERRUPTDATA_NAME(name##n) = data

#define __SWITCH_SETINTERRUPT3(name, n, back, val, data)\
  break; case n:\
    __INTERRUPT_NAME(name##n##back) = val;\
    __INTERRUPTDATA_NAME(name##n##back) = data

#define ___SWITCH_SETINTERRUPTMACRO_RUN(_1, _2, _3, _4, _5, _macro, ...) _macro
#define __SWITCH_SETINTERRUPT(name, ...)\
  ___SWITCH_SETINTERRUPTMACRO_RUN(name, __VA_ARGS__, __SWITCH_SETINTERRUPT3, __SWITCH_SETINTERRUPT2)(name, __VA_ARGS__)






bool _it_setup = false;

/**      Application Context Code
 * 
 *  The code differs between programs. When adding another interrupt,
 * always use __CREATE_INTERRUPT and __SWITCH_SETINTERRUPT
 * 
 *  Also set the priority and enable the interrupt
 */
__CREATE_INTERRUPT(USART, 1);
__CREATE_INTERRUPT(USART, 2);
__CREATE_INTERRUPT(USART, 3);
__CREATE_INTERRUPT(TIM, 2);
void set_peripheral_interrupt(peripheral_type type, peripheral_interrupt_cb cb, void *data){
  if(!_it_setup){
    _it_setup = true;

    NVIC_SetPriority(USART1_IRQn, 2);
    NVIC_EnableIRQ(USART1_IRQn);

    NVIC_SetPriority(USART2_IRQn, 2);
    NVIC_EnableIRQ(USART2_IRQn);

    NVIC_SetPriority(USART3_IRQn, 2);
    NVIC_EnableIRQ(USART3_IRQn);

    NVIC_SetPriority(TIM2_IRQn, 1);
    NVIC_EnableIRQ(TIM2_IRQn);
  }

  switch(type & __pt_peripheral_code){
    break; case pt_uart:
      switch(type & __pt_peripheral_num){
        __SWITCH_SETINTERRUPT(USART, 1, cb, data);
        __SWITCH_SETINTERRUPT(USART, 2, cb, data);
        __SWITCH_SETINTERRUPT(USART, 3, cb, data);
      }

    break; case pt_tim16:
      switch(type & __pt_peripheral_num){
        __SWITCH_SETINTERRUPT(TIM, 2, cb, data);
      }
  }
}

void delete_peripheral_interrupt(peripheral_type type){
  set_peripheral_interrupt(type, __peripheral_interrupt_dmp, NULL);
}

//      End of Application Context Code

#endif
