#include "stm32f1xx.h"
#include "stdbool.h"
#include "defines.h"

#include "async.h"
#include "peripheral.h" 
#include "bluetooth.h"

#include "preset_data.h"
#include "color.h"
#include "WS2812B.h"

#include "math.h"

#include "stdio.h"



uint8_t _led_vis_colarr[sizeof(color)*VIS_LED_COUNT];
ws2812b_param _led_handling_param = (ws2812b_param){
  .recv_config = ws2812b_recv_config_single
};

peripheral_info _led_handling = (peripheral_info){
  .peripheral_base = WS2812B_PERIPH,
  .type = WS2812B_PERIPH_TYPE,
  .additional_param = &_led_handling_param,
  .peripheralcb_command = ws2812b_command
};



peripheral_info _serial_debug = (peripheral_info){
  
};



peripheral_info _bluetooth_network = (peripheral_info){
  
};

peripheral_info *_soundget_network = &_bluetooth_network;




bool _update_visualizer(void *dmp){
  
}


#include "memory.h"
int main(){
  #ifdef STM32XX
  SystemClock_Config();
  __rcc_init();
  __gpio_init();
  #endif
  
  printf("test");
  
  async_setup();
  timer_wait(3000);

  _led_handling.command = pc_setup;
  _led_handling.peripheralcb_command(&_led_handling);

  async_timer_add(_update_visualizer, NULL, 30, true);
  YIELDWHILE(true);
}
