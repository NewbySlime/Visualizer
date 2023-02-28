#include "bluetooth.h"
#include "misc.h"
#include "async.h"

#ifdef STM32XX

#ifdef USING_HC05

#define HC05_DEF_BAUDRATE 38400


/**     NOTES for HC05 enums
 *  Since this code will only set the commands for general purpose transmit/receive,
 *  the enum will cover some of the commands
 * 
 */

// the enum corresponds to __hc05_commands_str
enum __hc05_commands{
  // set/check commands
  __hc05_c_test,
  __hc05_c_restore_def,
  __hc05_c_set_security_mode,
  __hc05_c_set_name,


  ___hc05_c_endof_writecmd,
  ___hc05_c_startof_readcmd = ___hc05_c_endof_writecmd + 0x1000,

  
  // get commands


  ___hc05_c_cmdmask = 0x0FFF
};

// the data corresponds to __hc05_commands
// the commands is a string that have std printf format
static const char *__hc05_commands_str[] = {
  // set command
  "AT",
  "AT+ORGL",
  "AT+SENM=%d,%d",
  "AT+NAME%s",


  // get command
  ""
};

static const char *__hc05_commands_terminating_str = "\r\n";


// NOTE: make sure, if changed, also change the struct in bluetooth.h
typedef struct _HC05_param{
  uint16_t key_pin;
  volatile uint32_t *key_gpio_odr;

  uint16_t en_pin;
  volatile uint32_t *en_gpio_odr;

  
  // the size of param buffer is the maximum param number for every command times uint32_t size in bytes
  uint8_t _cmdparam[sizeof(uint32_t)*4];

  uint32_t _cmd;
  uint32_t _nextcmd;
  uint32_t _iter;
  uint8_t *_data;

  const char *_commandstr;
} _HC05_param;
#define HC05_param _HC05_param


enum __proc_hc05_it_command{
  __proc_hc05_it_sendconfig,


  __proc_hc05_done = 0x1000,
  __proc_hc05_done_ok,
  __proc_hc05_done_fail
};


void __hc05_peripheral_it(void *data){

}


// return false if still in used
bool __hc05_check_hardware_cmd(peripheral_info *pinfo, uint32_t _pc_cmd){
  // check if still in used
  if(((HC05_param*)pinfo->additional_param)->_cmd & __proc_hc05_done){
    HC05_param *_param = (HC05_param*)pinfo->additional_param;

    switch(_pc_cmd & __pc_hardware){

    }

    if(!(_pc_cmd & pc_async))
      while(!(_param->_cmd & __proc_hc05_done))
        ;
  }
  else
    return false;

  return true;
}



peripheral_result HC05_command(peripheral_info *pinfo){
  if((pinfo->type & __pt_peripheral_code) != pt_uart)
    return pr_wrongtype;
  
  USART_TypeDef *_usart = (USART_TypeDef*)pinfo->peripheral_base;
  HC05_param *_param = (HC05_param*)pinfo->additional_param;
  switch(pinfo->command & __pc_command){
    break; case pc_NOP:

    break; case pc_setup:{
      // reset if needed
      if(!(pinfo->command & pc_noreset)){
        *_param->en_gpio_odr &= ~_param->en_pin;
        timer_wait(5);

        *_param->en_gpio_odr |= _param->en_pin;
      }


      _usart->CR1 = USART_CR1_UE | USART_CR1_RE | USART_CR1_TE;
      _usart->CR1 |= USART_CR1_TXEIE | USART_CR1_RXNEIE;
      _usart->BRR = USART_get_baudratediv(pinfo->type & __pt_peripheral_num, HC05_DEF_BAUDRATE);

      set_peripheral_interrupt(pinfo->type, __hc05_peripheral_it, pinfo);

      *_param->key_gpio_odr |= _param->key_pin;

      // setup the configuration first
      


      *_param->key_gpio_odr &= ~_param->key_pin;
    }


    // only run this command when it is explicitly stopped
    break; case pc_start:{

    }


    break; case pc_stop:{
      

      delete_peripheral_interrupt(pinfo->type);
      _usart->CR1 = 0;
    }


    break; case pc_write:{

    }

    break; case pc_read:{

    }


    break; default:
      return pr_nocommand;
  }


  __hc05_check_hardware_cmd(pinfo, pinfo->command);

  return pr_ok;
}

#endif  //  USING_HC05
#endif  //  STM32XX