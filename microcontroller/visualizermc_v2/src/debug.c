#include "debug.h"
#include "sys/unistd.h"

#include "defines.h"
#include "peripheral.h"
#include "USART_general.h"


USART_param _serial_usart_param = (USART_param){
  .usart_baudrate = SERIAL_BAUDRATE
};

peripheral_info _serial_usart = (peripheral_info){
  .peripheral_base = SERIAL_PERIPH,
  .type = SERIAL_PERIPH_TYPE,
  .additional_param = &_serial_usart_param,
  .peripheralcb_command = USART_command
};

int _write(int file, char *data, int len){
  if(file != STDOUT_FILENO && file != STDERR_FILENO)
    return -1;


  // start it up first, just in case
  _serial_usart.command = pc_start;
  _serial_usart.peripheralcb_command(&_serial_usart);


  // then send the data
  _serial_usart_param.memory = data;
  _serial_usart_param.memory_size = len;
  
  _serial_usart.command = pc_write | pc_async;
  _serial_usart.peripheralcb_command(&_serial_usart);

  return 0;
}