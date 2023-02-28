#include "misc.h"
#include "math.h"

uint16_t USART_get_baudratediv(uint16_t uart_n, uint32_t rate){
  uint32_t fpclk = 0;

  #ifdef STM32F1XX
  switch(uart_n){
    break; case 1:
      fpclk = F_APB2;
    
    break; default:
      fpclk = F_APB1;
  }

  #else
  #error USART_get_baudratediv current code isn't for another STM32 board.
  #endif

  float _f = (float)fpclk/(rate*16);
  float _frac, _mantissa;
  _frac = modff(_f, &_mantissa);

  uint16_t _ifrac = (uint16_t)roundf(_frac*16);
  uint16_t _imantissa = (uint16_t)_mantissa + _ifrac/16;
  _ifrac = _ifrac % 16;

  return (_imantissa << 4) | (_ifrac & 0b1111);
}