#ifndef SENSOR4_HEADER
#define SENSOR4_HEADER

#include "Arduino.h"

#include "input_sensor.hpp"

typedef void (*CallbackOnSensorAct)(sensor_actType);

void sensor4_setcallback(CallbackOnSensorAct cb);
void sensor4_init(uint8_t *pindata);

#endif