#ifndef MISC_HEADER
#define MISC_HEADER

#include "stdint.h"
#include "defines.h"

#ifdef STM32XX

// this will return baudrate divider in float16 for usart registers
uint16_t USART_get_baudratediv(uint16_t uart_n, uint32_t rate);

#endif

#endif