#ifndef RGBLED_STRUCT_HEADER
#define RGBLED_STRUCT_HEADER


#include "color.h"


typedef struct RGBLED_datastruct{
  color_array colarr;
  // in color
  const uint16_t max_recv;
} RGBLED_datastruct;

#endif