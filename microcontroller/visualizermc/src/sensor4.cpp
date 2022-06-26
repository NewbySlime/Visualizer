#include "sensor4.hpp"
#include "timer.hpp"



uint8_t pindata_sensor[4] = {0};
bool sensorPressed[4];
uint8_t firstPressed = 1;
int8_t _itercheck = 0;
uint8_t sensorPollrate = 20;
bool _spress = false;
bool _opress = false;
bool _aclear = false;
unsigned long timePress = 0;
unsigned long timeLastPress = 0;
unsigned long timeRelease = 0;

const uint16_t timems_slide = 400;
const uint16_t timems_click = 250;
const uint16_t timems_clear = 100;

void _dmpfunc(sensor_actType){}

CallbackOnSensorAct cbsens = _dmpfunc;

void callbackSensor(void *obj){
  bool stillPressing = false;
  for(uint8_t i = 0; i < 4; i++){
    bool pressed = digitalRead(pindata_sensor[i]);
    if(pressed){
      if(!_spress){
        _aclear = false;
        _spress = true;
        firstPressed = i;
        _itercheck = firstPressed < 2? 1: -1;
        timePress = millis();  
      }
      
      sensorPressed[i] = true;
      stillPressing = true;
    }
  }

  if(_spress && !stillPressing){
    _spress = false;
    unsigned long timeBetween = millis()-timePress;
    timeRelease = millis();

    int8_t _check = firstPressed + (_itercheck*2), _check1 = _check + _itercheck;
    if(timeBetween <= timems_slide && (sensorPressed[_check] || (_check1 >= 0 && _check1 < 4 && sensorPressed[_check1])))
      cbsens(_itercheck < 0? sensor_actType::slide_left : sensor_actType::slide_right);
    else if(timeBetween <= timems_click){
      if((millis()-timeLastPress) < timems_click){
        _opress = false;
        cbsens(sensor_actType::double_click);
      }
      else
        _opress = true;

      timeLastPress = millis();
    }
  }

  if(_opress && (millis()-timeLastPress) > timems_click){
    _opress = false;
    cbsens(sensor_actType::click);
  }

  if(!_spress && !_aclear && (millis()-timeRelease) > timems_clear){
    _aclear = true;
    for(uint8_t i = 0; i < 4; i++)
      sensorPressed[i] = false;
  }
}

void sensor4_setcallback(CallbackOnSensorAct cb){
  cbsens = cb;
}

void sensor4_init(uint8_t *pindata){
  memcpy(pindata_sensor, pindata, sizeof(uint8_t)*4);

  timer_setInterval(1000/sensorPollrate, callbackSensor, NULL);
}