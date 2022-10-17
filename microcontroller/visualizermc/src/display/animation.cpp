#include "animation.hpp"
#include "Arduino.h"

#define return_pairarr(namearr) return {namearr, sizeof(namearr)/sizeof(namearr[0])}

float bezier_func1D(const float *fdata, size_t fdatalen, float val){
  size_t newfdatalen = fdatalen-1;
  float newfdata[newfdatalen];

  for(size_t i = 0; i < newfdatalen; i++)
    newfdata[i] = (fdata[i+1]-fdata[i])*val+fdata[i];

  if(newfdatalen == 1)
    return newfdata[0];
  else
    return bezier_func1D(newfdata, newfdatalen, val);
}

const float PROGMEM _anim_bezier_easein[] = {0.f, 0.1f, 1.f};
const float PROGMEM _anim_bezier_easein2[] = {0.f, 0.1f, 0.1f, 1.f};
const float PROGMEM _anim_bezier_easeout[] = {0.f, 0.9f, 1.f};
std::pair<const float*, uint16_t> AnimationData::get_animdata(AnimationType animtype){
  switch(animtype){
    case EaseIn:
      return_pairarr(_anim_bezier_easein);
    
    case EaseIn2:
      return_pairarr(_anim_bezier_easein2);

    case EaseOut:
      return_pairarr(_anim_bezier_easeout);
  }

  return {NULL, 0};
}