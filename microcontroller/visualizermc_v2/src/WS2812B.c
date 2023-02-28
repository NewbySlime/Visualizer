#include "WS2812B.h"

#include "stdbool.h"
#include "math.h"
#include "async.h"

#include "memory.h"
#include "misc.h"

#include "color.h"
#include "defines.h"

#include "RGBLED_struct.h"


/*    program codes    */
// [SET] giving color data to the program, with first 2 bytes is a ushort of the length of the data (in colors)
#define WS2812B_DRIVER_DATA 0x8
// [SET] setting the receiving config between continous or single shot mode
#define WS2812B_DRIVER_RECV_CONFIG 0x9
// [GET] tell master how many color (3 bytes) data it can receive in one data batch
#define WS2812B_DRIVER_MAXDATA 0x10
// [] tell master that the received data has been processed
#define WS2812B_DRIVER_DONE 0x11

#define WS2812B_DRIVER_DEF_BAUDRATE 1000000

enum _ws2812b_write_process{
  // setting up procedure
  __proc_setup_recv_sendcode,
  __proc_setup_recv_data,

  __proc_setup_send_sendcode,
  __proc_setup_send_data,

  // sending procedure
  __proc_send_data,
  __proc_send_data_sendlength,
  __proc_send_data_senddata,

  // general procedure
  __proc_wait_done_code,
  __proc_done
};


// NOTE: if the parameter needs to be changed, also changed the ones in WS2812B.h header file
typedef struct _ws2812b_param{
  // data from RGBLED_datastruct
  volatile color_array colarr;
  volatile uint16_t _maxrecv;

  
  volatile uint8_t recv_config;

  volatile uint8_t *_data;
  volatile uint32_t _datalen;
  volatile uint32_t _iter;
  volatile uint8_t _code;
  volatile uint8_t _cmd;
} _ws2812b_param;
#define ws2812b_param _ws2812b_param

#define _set_to_waitdone(usart, param)\
  usart->CR1 &= ~(USART_CR1_TXEIE | USART_CR1_TCIE);\
  usart->CR1 |= USART_CR1_RXNEIE;\
  param->_cmd = __proc_wait_done_code

void _ws2812b_oninterrupt(void *data){
  #ifdef STM32XX
  ws2812b_param *_param = ((peripheral_info*)data)->additional_param;
  USART_TypeDef *_usart = ((peripheral_info*)data)->peripheral_base;

  switch(_param->_cmd){
    break; case __proc_setup_recv_sendcode:{
      if(_usart->SR & USART_SR_TXE){
        _usart->DR = _param->_code;
        _usart->CR1 &= ~USART_CR1_TXEIE;
        _usart->CR1 |= USART_CR1_RXNEIE;
        
        _param->_cmd = __proc_setup_recv_data;
        _param->_iter = 0;
      }
    }


    break; case __proc_setup_recv_data:{
      if(_usart->SR & USART_SR_RXNE){
        _param->_data[_param->_iter] = _usart->DR;
        _param->_iter++;

        if(_param->_iter >= _param->_datalen){
          _usart->CR1 &= ~USART_CR1_RXNEIE;
          _param->_cmd = __proc_done;
        }
      }
    }



    break; case __proc_setup_send_sendcode:{
      if(_usart->SR & USART_SR_TXE){
        _usart->DR = _param->_code;

        _param->_cmd = __proc_setup_send_data;
        _param->_iter = 0;
      }
    }


    break; case __proc_setup_send_data:{
      if(_usart->SR & USART_SR_TXE){
        _usart->DR = _param->_data[_param->_iter];
        _param->_iter++;

        if(_param->_iter >= _param->_datalen){
          _set_to_waitdone(_usart, _param);
        }
      }
    }



    break; case __proc_send_data:{
      if(_usart->SR & USART_SR_TXE){
        _usart->DR = WS2812B_DRIVER_DATA;

        _param->_cmd = __proc_send_data_sendlength;
        _param->_iter = 0;
      }
    }


    break; case __proc_send_data_sendlength:{
      if(_usart->SR & USART_SR_TXE){
        _usart->DR = _param->colarr.count >> (_param->_iter*8);
        _param->_iter++;

        if(_param->_iter >= sizeof(uint16_t)){
          _param->_cmd = __proc_send_data_senddata;
          _param->_iter = 0;
        }
      }
    }


    break; case __proc_send_data_senddata:{
      if(_usart->SR & USART_SR_TXE){
        _usart->DR = *((uint8_t*)((uint32_t)_param->colarr.colors+_param->_iter));
        _param->_iter++;

        if(_param->_iter >= (_param->colarr.count*sizeof(color))){
          _set_to_waitdone(_usart, _param);
        }
      }
    }




    break; case __proc_wait_done_code:{
      if((_usart->SR & USART_SR_RXNE) && _usart->DR == WS2812B_DRIVER_DONE){
        _usart->CR1 &= ~USART_CR1_RXNEIE;
        _param->_cmd = __proc_done;

        peripheral_command _pc = ((peripheral_info*)data)->command;
        if(_pc & pc_async)
          ((peripheral_info*)data)->peripheralcb_onevent(pe_donecommand | (_pc & __pc_command), (peripheral_info*)data);
      }
    }
  }
  #endif
}

peripheral_result ws2812b_command(peripheral_info *pinfo){
  #ifdef STM32XX
  if((pinfo->type & __pt_peripheral_code) != pt_uart)
    return pr_wrongtype;

  USART_TypeDef *usart = (USART_TypeDef*)pinfo->peripheral_base;
  switch(pinfo->command & __pc_command){
    break; case pc_setup:{
      if(usart->CR1 & USART_CR1_UE)
        return pr_notready;

      set_peripheral_interrupt(pinfo->type, _ws2812b_oninterrupt, pinfo);
      usart->BRR = USART_get_baudratediv(pinfo->type & __pt_peripheral_num, WS2812B_DRIVER_DEF_BAUDRATE);
      usart->GTPR = 0;
      usart->CR3 = 0;
      usart->CR2 = 0;
      usart->CR1 = USART_CR1_UE | USART_CR1_RE | USART_CR1_TE;

      ws2812b_param *param = (ws2812b_param*)pinfo->additional_param;

      param->_cmd = __proc_setup_send_sendcode;
      param->_data = (uint8_t*)&param->recv_config;
      param->_datalen = sizeof(uint8_t);
      param->_code = WS2812B_DRIVER_RECV_CONFIG;
      usart->CR1 |= USART_CR1_TXEIE;
      while(param->_cmd != __proc_done)
        ;

      param->_cmd = __proc_setup_recv_sendcode;
      param->_data = (uint8_t*)&param->_maxrecv;
      param->_datalen = sizeof(uint16_t);
      param->_code = WS2812B_DRIVER_MAXDATA;
      usart->CR1 |= USART_CR1_TXEIE;
      while(param->_cmd != __proc_done)
        ;

      delete_peripheral_interrupt(pinfo->type);
      usart->CR1 = 0;
    }

    break; case pc_start:{
      if(usart->CR1 & USART_CR1_UE)
        return pr_notready;

      set_peripheral_interrupt(pinfo->type, _ws2812b_oninterrupt, pinfo);
      usart->BRR = USART_get_baudratediv(pinfo->type & __pt_peripheral_num, WS2812B_DRIVER_DEF_BAUDRATE);
      usart->GTPR = 0;
      usart->CR3 = 0;
      usart->CR2 = 0;
      usart->CR1 = USART_CR1_UE | USART_CR1_RE | USART_CR1_TE;
    }

    break; case pc_stop:{
      ws2812b_param *param = (ws2812b_param*)pinfo->additional_param;
      
      usart->CR1 = 0;
      delete_peripheral_interrupt(pinfo->type);
    }

    break; case pc_write:{
      ws2812b_param *param = (ws2812b_param*)pinfo->additional_param;
         
      if(param->_cmd != __proc_done)
        return pr_notready;

      if(param->colarr.count > param->_maxrecv)
        param->colarr.count = param->_maxrecv;

      param->_cmd = __proc_send_data;
      usart->CR1 |= USART_CR1_TXEIE;

      if(!(pinfo->command & pc_async))
        while(param->_cmd != __proc_done);
    }
  }
  #endif

  return pr_ok;
}