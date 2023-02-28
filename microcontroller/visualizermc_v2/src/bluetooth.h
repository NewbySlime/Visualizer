#ifndef BLUETOOTH_HEADER
#define BLUETOOTH_HEADER

#include "peripheral.h"


// using __pc_hardware mask for peripheral_command
typedef enum bluetooth_general_command{
  bluetooth_gc_set_name = 0x0100,
  bluetooth_gc_set_pincode = 0x0200,

  bluetooth_gc_specific_hardware_command = 0xFF00
} bluetooth_general_command;



// structs used for parameters

typedef struct bluetooth_param_set_name{
  const char *name;
} bluetooth_param_set_name;

typedef struct bluetooth_param_set_pincode{
  const uint32_t pin;
} bluetooth_param_set_pincode;



/**       Using peripheral_command for bluetooth
 *  
 * 
 */

#ifdef USING_HC05

typedef struct HC05_param{
  uint16_t key_pin;
  volatile uint32_t *key_gpio_odr;

  uint16_t en_pin;
  volatile uint32_t *en_gpio_odr;

  // this buffer is used for parameters in getting/setting commands
  uint8_t cmd_param[sizeof(uint32_t)*4];

  const uint8_t _reserved[
    sizeof(uint32_t)+
    sizeof(uint32_t)+
    sizeof(uint32_t)+
    sizeof(uint8_t*)+
    sizeof(const char*)
  ];
} HC05_param;

peripheral_result HC05_command(peripheral_info *pinfo);
#endif


#endif