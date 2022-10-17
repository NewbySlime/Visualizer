#include "leddriver-master.hpp"
#include "Arduino.h"
#include "polling.h"

#define CODE_PROG_STARTDATA 0x07
#define CODE_PROG_DATA 0x08

#define LEDDRIVER_INITTIME 3000
// checked using digital analyzer
#define LEDDRIVER_WAITRXUS 34800
// 60us reset delay
#define LEDDRIVER_WAITANOTHERSENDUS 60

#define LEDDRIVER_POLLID 0x13


uint16_t _manyLed = 0;
bool _initiated = false;
bool _sending = false;
unsigned long _lastSend = 0;
uint16_t idx = 0;
const color *_cols = nullptr;


void _resetledComm(){
  color *dummydata = (color*)calloc(sizeof(color), LEDDRIVER_MAXSEND);
  Serial1.write(reinterpret_cast<const uint8_t*>(&dummydata), sizeof(color)*LEDDRIVER_MAXSEND);

  free(dummydata);
}

void _update_sendled(void*);
void led_init(uint16_t leds){
  if(!_initiated){
    _initiated = true;
    if(millis() < LEDDRIVER_INITTIME)
      delay(LEDDRIVER_INITTIME-millis());

    _manyLed = leds;
    _resetledComm();

    polling_addfunc(_update_sendled, (void*)LEDDRIVER_POLLID);
  }
}

void led_sendcols(const color *cols){
  unsigned long _currus = micros();
  unsigned long deltaus = _currus < _lastSend? _currus+(0xffffffffU-_lastSend): _currus-_lastSend;

  _lastSend = _currus;

  // then wait for certain us until the driver is ready
  if(deltaus < LEDDRIVER_WAITANOTHERSENDUS)
    delayMicroseconds(LEDDRIVER_WAITANOTHERSENDUS-deltaus);

  Serial1.write((uint8_t)CODE_PROG_STARTDATA);
  Serial1.write(reinterpret_cast<const char*>(&_manyLed), sizeof(uint16_t));

  _lastSend -= LEDDRIVER_WAITRXUS;

  idx = 0;
  _sending = true;
  _cols = cols;
}

void _update_sendled(void*){
  unsigned long _currus = micros();
  unsigned long deltaus = _currus < _lastSend? _currus+(0xffffffffU-_lastSend): _currus-_lastSend;

  if(_sending && idx < _manyLed && deltaus > LEDDRIVER_WAITRXUS){
    uint16_t sentlen = min(LEDDRIVER_MAXSEND, _manyLed-idx);
    Serial1.write((uint8_t)CODE_PROG_DATA);
    Serial1.write(reinterpret_cast<const uint8_t*>(&_cols[idx]), sentlen*3);
    idx += sentlen;

    if(idx >= _manyLed)
      _sending = false;

    _lastSend = _currus;
  }
}