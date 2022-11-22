#include "EEPROM.hpp"

#include "Adafruit_I2CDevice.h"

#include "Arduino.h"
#include "math.h"

#include "timer.hpp"
#include "polling.h"
#include "debug.hpp"

#include "async.hpp"

#define BUF_MAXREAD 16


/*      EEPROM_Class functions      */
size_t EEPROM_Class::_bufWrite(uint32_t address, char *buf, size_t buflen){
  DEBUG_PRINT("address %X, _size %d\n", address, _size);
  if(address >= _size){
    _memlimit = true;
    return 0;
  }

  // the last one is to offset not to exceed page boundary
  size_t bytesend = min(buflen, (size_t)(_pagesize-(address%_pagesize)));
  if((bytesend+address) >= _size){
    _memlimit = true;
    bytesend = _size-address;
  }

  if(bytesend > 0){
    // mem address reversed
    _setAddress(address);
    Wire.beginTransmission(_addr);

    for(int i = _addresssize-1; i >= 0; i--)
      Wire.write((uint8_t)(address >> (8*i)));
      
    Wire.write(buf, bytesend);
    Wire.endTransmission();
  }

  _curmemaddr += bytesend;
  return bytesend;
}

size_t EEPROM_Class::_bufRead(char *buf, size_t buflen){
  if(_curmemaddr >= _size){
    _memlimit = true;
    return 0;
  }

  size_t byteread = min(buflen, (size_t)BUF_MAXREAD);
  if((byteread+_curmemaddr) >= _size){
    _memlimit = true;
    byteread = _size-_curmemaddr;
  }

  size_t reqlen = Wire.requestFrom(_addr, byteread);
  byteread = Wire.readBytes(buf, byteread);

  DEBUG_PRINT("reqlen %d\nbyteread %d\n", reqlen, byteread);
  for(int i = 0; i < byteread; i++){
    DEBUG_PRINT("0x%X ", buf[i]);
  }

  DEBUG_PRINT("\n");

  _curmemaddr += byteread;
  return byteread;
}

void EEPROM_Class::_seekAddress(uint32_t address){
  if(address >= _size){
    _memlimit = true;
    return;
  }

  _memlimit = false;
  _setAddress(address);
  Wire.beginTransmission(_addr);
  
  for(int i = _addresssize-1; i >= 0; i--)
    Wire.write((uint8_t)(address >> (8*i)));
  
  Wire.endTransmission();

  _curmemaddr = address;
}

void EEPROM_Class::_timer_bufWrite(){
  DEBUG_PRINT("timer bufwrite\n");
  if(_t_bytesent < _t_buflen && !_memlimit){
    _t_bytesent += _bufWrite(_t_address+_t_bytesent, _t_buf+_t_bytesent, _t_buflen-_t_bytesent);
    int _t = timer_setTimeout(_delayms, __onTimer, this);
  }
  else{
    _ready_use = true;

    if(_cb)
      _cb(_classobj);
  }
}

void EEPROM_Class::_polling_bufRead(){
  if(_p_byteread < _p_buflen && !_memlimit){
    DEBUG_PRINT("byteread %d\nbuf 0x%X, 0x%X\n", _p_byteread, _p_buf, _p_buf+_p_byteread);
    _p_byteread += _bufRead(_p_buf+_p_byteread, _p_buflen-_p_byteread);
  }
  else{
    polling_removefunc(this);
    _ready_use = true;
    
    if(_cb)
      _cb(_classobj);
  }
}

size_t EEPROM_Class::bufWriteBlock(uint32_t address, char *buf, size_t buflen){
  if(_ready_use){
    /*
    _memlimit = false;

    size_t _bytesent = 0;
    while(_bytesent < buflen && !_memlimit){
      _bytesent += _bufWrite(address+_bytesent, buf+_bytesent, buflen-_bytesent);
      delay(_delayms);
    }
    */

    bufWriteAsync(address, buf, buflen, NULL, NULL);
    YieldWhile(!_ready_use);

    return _t_bytesent;
  }

  return 0;
}

void EEPROM_Class::bufWriteAsync(uint32_t address, char *buf, size_t buflen, EEPROM_Cb cb, void* obj){
  if(_ready_use){
    DEBUG_PRINT("ready use\n");
    _ready_use = false;
    _memlimit = false;
    _t_bytesent = 0;
    _t_address = address;
    _t_buf = buf;
    _t_buflen = buflen;

    _cb = cb;
    _classobj = obj;

    _timer_bufWrite();
  }
}

size_t EEPROM_Class::bufReadBlock_currAddr(char *buf, size_t buflen){
  if(_ready_use){
    size_t _byteread = 0;
    while(_byteread < buflen && !_memlimit)
      _byteread += _bufRead(buf+_byteread, buflen-_byteread);
    
    return _byteread;
  }

  return 0;
}

size_t EEPROM_Class::bufReadBlock(uint32_t address, char *buf, size_t buflen){
  if(_ready_use){
    _memlimit = false;
    _seekAddress(address);
    return bufReadBlock_currAddr(buf, buflen);
  }

  return 0;
}

void EEPROM_Class::bufReadAsync(uint32_t address, char *buf, size_t buflen, EEPROM_Cb cb, void* obj){
  if(_ready_use){
    _ready_use = false;
    _memlimit = false;
    _seekAddress(address);

    _cb = cb;
    _classobj = obj;

    _p_byteread = 0;
    _p_buf = buf;
    _p_buflen = buflen;
    polling_addfunc(__onPoll, this);

    // get data once
    _polling_bufRead();
  }
}

uint32_t EEPROM_Class::getMemAddress(){
  return _curmemaddr;
}

uint32_t EEPROM_Class::getMemSize(){
  return _size;
}

bool EEPROM_Class::getIsReady(){
  return _ready_use;
}