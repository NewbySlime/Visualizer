#ifndef EEPROM_HEADER
#define EEPROM_HEADER

#include "Arduino.h"
#include "queue"
#include "vector"

class EEPROM_I2C{
  private:
    struct _message{
      uint16_t memaddress;
      char *data;
      size_t datalen;
    };

    _message _currentmsgdat;

    size_t _sentlen = 0;
    bool _neednewmsg = true;

    bool _stillWriting = false;

    static void __callbackOnTimeout(void *cobj){((EEPROM_I2C*)cobj)->_callbackOnTimeout();}
    void _callbackOnTimeout();
    void _write(uint16_t memaddress, char* buffer, size_t bufferlen);

  protected:
    size_t _storage_size;
    size_t _pagewritesize;
    uint8_t _deviceaddr;

    size_t _delaytimems;

  public:
    // returns -1 if still pending write
    int read(uint16_t memaddress, char* buffer, size_t bufferlen);

    // if there's still pending write, it will append the buffer
    // this will not write to the EEPROM if the buffer length is exceeding the maximum storage size
    // and the buffer will be deleted once the contents of the buffer is sent
    void writeAsync(uint16_t memaddress, char* buffer, size_t bufferlen);
    uint16_t getMax();
};


class EEPROM_AT24C256: public EEPROM_I2C{
  public:
    EEPROM_AT24C256(uint8_t devaddr){
      _storage_size = 32768;
      _pagewritesize = 16;
      
      _deviceaddr = 0b1010000 | devaddr;

      _delaytimems = 5;
    }
};

#endif