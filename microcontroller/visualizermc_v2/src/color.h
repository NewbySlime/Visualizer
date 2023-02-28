#ifndef COLOR_HEADER
#define COLOR_HEADER

#include "stdint.h"


typedef struct color{
  uint8_t r, g, b;
} color;

typedef struct color_array{
  uint32_t count;     // in color
  color *colors;
} color_array;


typedef struct color_ranges{
  uint16_t count;

  struct _color_range{
    float range;
    color col;
  } *colors;
} color_ranges;


color color_mix_linear(color col1, color col2, float range);

color color_sum(color col1, color col2);
color color_sub(color col1, color col2);
color color_mult(color col, float val);


// TODO: use range_end and range_start
// range should be between 0f - 1f
color color_ranges_get(color_ranges colrange, float range);

#endif