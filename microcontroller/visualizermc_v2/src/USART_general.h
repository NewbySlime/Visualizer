#ifndef USART_GENERAL_HEADER
#define USART_GENERAL_HEADER

#include "peripheral.h"
#include "stdbool.h"


typedef struct USART_param{
  uint32_t usart_baudrate;
  
  const uint8_t *memory;
  uint32_t memory_size;

  const char _reserved[
    PERIPHERAL_BUFFERSIZE+
    sizeof(uint8_t*)+
    sizeof(uint8_t*)+
    sizeof(uint32_t)+
    sizeof(uint32_t)+
    sizeof(uint32_t)+
    sizeof(uint32_t)+
    sizeof(bool)
  ];
} USART_param;

peripheral_result USART_command(peripheral_info *pinfo);

#endif