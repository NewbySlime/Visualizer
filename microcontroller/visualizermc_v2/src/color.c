#include "color.h"


color color_mix_linear(color col1, color col2, float range){
  return (color){
    .r = (col1.r-col2.r)*range,
    .g = (col1.g-col2.g)*range,
    .b = (col1.b-col2.b)*range
  };
}


color color_sum(color col1, color col2){
  return (color){
    .r = col1.r+col2.r,
    .g = col1.g+col2.g,
    .b = col1.b+col2.b
  };
}

color color_sub(color col1, color col2){
  return (color){
    .r = col1.r-col2.r,
    .g = col1.g-col2.g,
    .b = col1.b-col2.b
  };
}

color color_mult(color col, float val){
  return (color){
    .r = col.r*val,
    .g = col.g*val,
    .b = col.b*val
  };
}


color color_ranges_get(color_ranges colrange, float range){
  if(colrange.colors[0].range > range)
    return colrange.colors[0].col;

  uint32_t i = 1;
  for(; i < colrange.count; i++){
    if(colrange.colors[i].range > range)
      return color_mix_linear(
        colrange.colors[i-1].col,
        colrange.colors[i].col,
        colrange.colors[i].range - colrange.colors[i-1].range
      );
  }

  return colrange.colors[colrange.count-1].col;
}