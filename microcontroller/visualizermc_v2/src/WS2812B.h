#ifndef WS2812B_HEADER
#define WS2812B_HEADER

#include "peripheral.h"
#include "stdint.h"
#include "stdbool.h"
#include "color.h"

#include "RGBLED_struct.h"

enum ws2812b_recv_config{
  ws2812b_recv_config_single = 0,
  ws2812b_recv_config_continous = 1
};

typedef struct ws2812b_param{
  volatile RGBLED_datastruct rbgled_data;

  // using ws2812b_recv_config
  // this won't have any effect if not using external driver
  // to apply it, run the peripheral command with setup command
  volatile uint8_t recv_config;
  
  const char __reserved[
    sizeof(uint8_t*)+
    sizeof(uint32_t)+
    sizeof(uint32_t)+
    sizeof(uint8_t)+
    sizeof(uint8_t)
  ];
} ws2812b_param;

/**       pinfo data
 *  peripheral_base is object of UART_TypeDef
 *  additional_param is object of RGBLED_datastruct
 * 
 *  Any other than this, can be set freely
 * 
 * 
 *  The function can be run asynchronously, with event function
 */
peripheral_result ws2812b_command(peripheral_info *pinfo);

#endif