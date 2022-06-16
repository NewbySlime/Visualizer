#ifndef EEPROM_HEADER
#define EEPROM_HEADER

#include "Arduino.h"

#ifdef ESP8266
#include "ESP8266TimerInterrupt.h"
#endif

class EEPROM_I2C{
  public:
    virtual int read(uint16_t memaddress, void* buffer, int bufferlen);
    virtual int write_nodelay(uint16_t memaddress, const void* buffer, int bufferlen);
    virtual int write(uint16_t memaddress, const void* buffer, int bufferlen);
    virtual uint16_t getMax();
};


class EEPROM_asyncWrite{
  private:
    EEPROM_I2C *currobj;

  public:
    typedef void (*Callback1)();

    void begin();
    void end();
    void safeEnd();

    // call this after calling safeEnd or before begin
    void bindEEPROM(EEPROM_I2C *obj);
    
    // buffer variable should not be using stack based memory
    // after safeEnd() called, the function will call callback onSentAll to handle the buffer
    void writeData(uint16_t memaddress, const void* buffer, int bufferlen);
    
    // when data is send and the program need to free it
    void attachCallback_onSendMessage(Callback1 cb);

    // when all data is send and ready to end its use
    // only called after calling safeEnd
    void attachCallback_onSentAll(Callback1 cb);
};


extern EEPROM_asyncWrite EEPROMWriter;

#endif