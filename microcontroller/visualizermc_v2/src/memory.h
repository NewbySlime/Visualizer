#ifndef MEMORY_HEADER
#define MEMORY_HEADER

#include "stdint.h"


#define memcpy _memcpy

uint32_t _memcpy(void *dst, const void *src, uint32_t size);

#endif