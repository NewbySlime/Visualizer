#include "USART_general.h"
#include "misc.h"
#include "memory.h"



static const uint32_t __buffer_size = PERIPHERAL_BUFFERSIZE/2;
static const uint32_t __buffer_write_offset = PERIPHERAL_BUFFERSIZE/2;
static const uint32_t __buffer_read_offset = 0;


typedef struct _USART_param{
  const uint32_t usart_baudrate;

  const uint8_t *memory;
  const uint32_t memory_size;


  volatile uint8_t _buffer[PERIPHERAL_BUFFERSIZE];
  volatile uint8_t *_current_memwrite;
  volatile uint8_t *_current_memread;

  volatile uint32_t _writeiter;
  volatile uint32_t _writeiter_boundary;

  volatile uint32_t _readiter;
  volatile uint32_t _readiter_boundary;

  bool _is_used_synchronously;
} _USART_param;
#define USART_param _USART_param


// returns iteration
uint32_t _usart_memcpy(void *dst, const void *src, uint32_t dst_iter, uint32_t size){
  for(uint32_t _i = 0; _i < size;){
    uint32_t _cpysize = __buffer_size-dst_iter;
    if(_cpysize > (size-_i))
      _cpysize = size-_i;

    memcpy(
      (uint8_t*)((uint32_t)dst+dst_iter),
      (uint8_t*)((uint32_t)src+_i),
      _cpysize
    );

    dst_iter = (dst_iter+_cpysize) % __buffer_size;
    _i += _cpysize;
  }
}

void _usart_interrupt(void *data){
  USART_TypeDef *_usart = ((peripheral_info*)data)->peripheral_base;
  USART_param *_param = ((peripheral_info*)data)->additional_param;

  if(_usart->SR & USART_SR_TXE){
    _usart->DR = _param->_current_memwrite[_param->_writeiter];
    _param->_writeiter = (_param->_writeiter + 1) % __buffer_size;

    if(_param->_writeiter == _param->_writeiter_boundary)
      _usart->CR1 &= ~USART_CR1_TXEIE;
  }

  if(_usart->SR & USART_SR_RXNE){
    _param->_current_memread[_param->_readiter] = _usart->DR;
    _param->_readiter_boundary++;

    if(_param->_readiter == _param->_readiter_boundary)
      _param->_readiter++;
  }
}


peripheral_result USART_command(peripheral_info *pinfo){
  if(pinfo->type != pt_uart)
    return pr_wrongtype;
  
  USART_TypeDef *_usart = (USART_TypeDef*)pinfo->peripheral_base;
  USART_param *_param = (USART_param*)pinfo->additional_param;
  switch(pinfo->command){
    break; case pc_setup:{
      if(_usart->CR1 & USART_CR1_UE)
        return pr_notready;

      _usart->BRR = USART_get_baudratediv(pinfo->type & __pt_peripheral_num, _param->usart_baudrate);
      _usart->CR2 = 0;
      _usart->CR3 = 0;
      _usart->CR1 = 0;

      _param->_current_memwrite = (uint8_t*)((uint32_t)_param->_buffer+__buffer_write_offset);
      _param->_writeiter = 0;
      _param->_writeiter_boundary = 0;

      _param->_current_memread = (uint8_t*)((uint32_t)_param->_buffer+__buffer_write_offset);
      _param->_readiter = 0;
      _param->_readiter_boundary = 0;

      _param->_is_used_synchronously = false;
    }


    break; case pc_start:{
      if(_usart->CR1 & USART_CR1_UE)
        return pr_already_started;

      _usart->CR1 = USART_CR1_UE | USART_CR1_TE | USART_CR1_RE;
    }


    break; case pc_stop:{
      _usart->CR1 = 0;
    }


    break; case pc_read:{
      
    }

    break; case pc_write:{
      if(!_param->_is_used_synchronously && pinfo->command & pc_async && _param->memory_size <= __buffer_size){
        uint32_t _buf_readysize;
        while(true){
          if(_param->_writeiter_boundary < _param->_writeiter)
            _buf_readysize = (__buffer_size - _param->_writeiter) + _param->_writeiter_boundary;
          else
            _buf_readysize = _param->_writeiter_boundary-_param->_writeiter_boundary;

          if(_buf_readysize < _param->_writeiter_boundary)
            __asm("WFI");
          else
            break;
        }

        _param->_writeiter_boundary = _usart_memcpy(
          (uint8_t*)((uint32_t)_param->_buffer+__buffer_write_offset), 
          _param->memory,
          _param->_writeiter_boundary,
          _param->memory_size
        );

        _usart->CR1 |= USART_CR1_TXEIE;
      }
      else{
        while(_param->_writeiter != _param->_writeiter_boundary)
          ;
        
        _param->_writeiter = 0;
        _param->_writeiter_boundary = _param->memory_size;
        _param->_current_memwrite = _param->memory;
        _param->_is_used_synchronously = true;

        _usart->CR1 |= USART_CR1_TXEIE;
        while(_param->_writeiter != _param->_writeiter_boundary)
          __asm("WFI");

        _param->_current_memwrite = (uint8_t*)((uint32_t)_param->_buffer+__buffer_write_offset);
        _param->_writeiter = 0;
        _param->_writeiter_boundary = 0;
      }
    }
  }
}