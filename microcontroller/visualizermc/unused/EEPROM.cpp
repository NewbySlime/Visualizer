#include "EEPROM.h"

#include "Wire.h"
#include "timer.hpp"

// FIXME might need to review the code, the problem resides around memory
// ideas:
//    - is it the cause of timer?
//    - don't use delete on isr
void EEPROM_I2C::_callbackOnTimeout(){
  Serial.printf("test %d\n", _sentlen);
  if(_neednewmsg){
    _neednewmsg = false;
    _sentlen = 0;
  }

  size_t sendByteLen = min(_currentmsgdat.datalen-_sentlen, _pagewritesize);
  Serial.printf("test1 %d\n", sendByteLen);
  //_write(_currentmsgdat.memaddress+_sentlen, _currentmsgdat.data+_sentlen, sendByteLen);
  
  _sentlen += sendByteLen;
  Serial.printf("test %d\n", _sentlen);

  bool _doloop = true;
  if(_sentlen >= _currentmsgdat.datalen){
    _neednewmsg = true;
    //free(_currentmsgdat.data);

    if(true){
      _doloop = false;
      _stillWriting = false;
    }
  }

  Serial.printf("test2\n");
  if(_doloop)
    timer_setTimeout(_delaytimems, __callbackOnTimeout, this);
}

void EEPROM_I2C::_write(uint16_t memaddress, char* buffer, size_t bufferlen){
  Wire.beginTransmission(_deviceaddr);
  Wire.write((uint8_t)((memaddress >> 8) & 0xFF));
  Wire.write((uint8_t)(memaddress & 0xFF));
  for(size_t i = 0; i < bufferlen; i++)
    Wire.write((uint8_t)buffer[i]);
    
  Wire.endTransmission();
}

int EEPROM_I2C::read(uint16_t memaddress, char* buffer, size_t bufferlen){
  if(!_stillWriting && ((size_t)memaddress + bufferlen) < _storage_size){
    Wire.beginTransmission(_deviceaddr);
    Wire.endTransmission(false);

    int readlen = 0;
    while(readlen < bufferlen){
      int getlen = Wire.requestFrom(_deviceaddr, bufferlen);
      if(getlen == 0)
        break;
      
      Wire.readBytes(buffer+readlen, getlen);
      readlen += getlen;
    }

    return readlen;
  }

  return 0;
}

void EEPROM_I2C::writeAsync(uint16_t memaddress, char* buffer, size_t bufferlen){
  _currentmsgdat = _message{
    .memaddress = memaddress,
    .data = buffer,
    .datalen = bufferlen
  };

  if(!_stillWriting){
    _stillWriting = true;
    _callbackOnTimeout();
  }
}

uint16_t EEPROM_I2C::getMax(){
  return _storage_size;
}
