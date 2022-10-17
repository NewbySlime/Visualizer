#include "sensor4.hpp"
#include "timer.hpp"


uint8_t pindata_sensor[4] = {0};
bool sensorPressed[4];
uint8_t firstPressed = 1;
int8_t _itercheck = 0;
uint8_t sensorPollrate = 20;
bool _spress = false;
bool _opress = false;
// if the buffer is cleared, true when there's a slide
bool _aclear2 = true;
// if the buffer is cleared, true when the buffer is written
bool _aclear1 = true;
unsigned long timePress = 0;
unsigned long timeLastPress = 0;
unsigned long timeRelease = 0;

const uint16_t timems_slide = 400;
const uint16_t timems_click = 250;
const uint16_t timems_clear = 100;

void _dmpfunc(sensor_actType){}

CallbackOnSensorAct cbsens = _dmpfunc;

uint8_t _flagact = 0;
void _doflag(sensor_actType sat){
  _flagact |= (1 << (int)sat);
}


// the reason why it needs a flag is to not distrupt the callbackSensor by calling cbsens as it uses time-accurate checkings
void _callcb(){
  for(int i = 0; i < (sizeof(_flagact)*8) && _flagact > 0; i++)
    if((_flagact & (1 << i)) > 0){
      cbsens((sensor_actType)i);
      _flagact &= ~(1 << i);
    }
}

void callbackSensor(void *obj){
  bool stillPressing = false;
  for(uint8_t i = 0; i < 4 && _aclear2; i++){
    bool pressed = digitalRead(pindata_sensor[i]);
    if(pressed){

      // this will call once the hand is touching the sensor
      if(!_spress){
        _aclear1 = false;
        _spress = true;
        firstPressed = i;
        _itercheck = firstPressed >= 2? -1: 1;
        Serial.printf("_itercheck %d\n", _itercheck);
        timePress = millis();

        _doflag(sensor_actType::pressed);  
      }
      
      sensorPressed[i] = true;
      stillPressing = true;
    }
  }

  if(_spress && !stillPressing){
    _aclear2 = false;
    _spress = false;
    unsigned long timeBetween = millis()-timePress;
    timeRelease = millis();

    int8_t _check = firstPressed + (_itercheck*2), _check1 = _check + _itercheck;
    Serial.printf("_check %d %d\n", _check, _check1);
    Serial.printf("timems %d\n", timeBetween);
    if(timeBetween <= timems_slide && (sensorPressed[_check] || (_check1 >= 0 && _check1 < 4 && sensorPressed[_check1])))
      _doflag(_itercheck < 0? sensor_actType::slide_left : sensor_actType::slide_right);
    else if(timeBetween <= timems_click){
      Serial.printf("time last Press %d, %d\n", millis()-timeLastPress, timeBetween);
      if((millis()-timeLastPress) < (timems_click*2)){
        _opress = false;
        _doflag(sensor_actType::double_click);
      }
      else
        _opress = true;

      timeLastPress = millis();
    }

    _doflag(sensor_actType::released);
  }

  if(_opress && (millis()-timeLastPress) > timems_click){
    _opress = false;
    _doflag(sensor_actType::click);
  }

  if(!_spress && (!_aclear1 || !_aclear2) && (millis()-timeRelease) > timems_clear){
    printf("cleared.\n");
    _aclear2 = true;
    _aclear1 = true;
    for(uint8_t i = 0; i < 4; i++)
      sensorPressed[i] = false;
  }

  _callcb();
}

void sensor4_setcallback(CallbackOnSensorAct cb){
  cbsens = cb;
}

void sensor4_init(uint8_t *pindata){
  memcpy(pindata_sensor, pindata, sizeof(uint8_t)*4);

  timer_setInterval(1000/sensorPollrate, callbackSensor, NULL);
}