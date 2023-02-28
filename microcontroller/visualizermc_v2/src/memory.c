#include "memory.h"

#define __GET_ADDRESS(MEM, TYPE, ITER) *((TYPE*)((uint32_t)MEM+ITER))

uint32_t _memcpy(void *dst, const void *src, uint32_t size){
  for(uint32_t _iter = 0; _iter < size;){
    if((size-_iter) >= sizeof(uint32_t)){
      __GET_ADDRESS(dst, uint32_t, _iter) = __GET_ADDRESS(src, uint32_t, _iter);
      _iter += sizeof(uint32_t);
    }
    else{
      __GET_ADDRESS(dst, uint8_t, _iter) = __GET_ADDRESS(src, uint8_t, _iter);
      _iter += sizeof(uint8_t);
    }
  }
}