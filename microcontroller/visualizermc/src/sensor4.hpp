#ifndef SENSOR4_HEADER
#define SENSOR4_HEADER

#include "Arduino.h"

enum sensor_actType{
  click,
  double_click,
  slide_left,
  slide_right
};

typedef void (*CallbackOnSensorAct)(sensor_actType);

void sensor4_setcallback(CallbackOnSensorAct cb);
void sensor4_init(uint8_t *pindata);

#endif