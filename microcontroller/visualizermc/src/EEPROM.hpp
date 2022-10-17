#ifndef EEPROM_HEADER
#define EEPROM_HEADER

#include "Wire.h"

#define S_TO_MS 1000

class EEPROM_Class{
  public:
    typedef void (*EEPROM_Cb)(void*);

  protected:
    uint8_t _addresssize;
    
    // called when needed to use address as memory address
    virtual void _setAddress(uint32_t memaddr){}

  private:
    uint8_t _addr;
    size_t _size, _curmemaddr = 0, _pagesize;
    uint16_t _delayms;
    bool _memlimit = false;
    bool _ready_use = true;

    EEPROM_Cb _cb;
    void *_classobj;

    /*    variables for timer   */
    size_t _t_bytesent;
    uint32_t _t_address;
    char *_t_buf;
    size_t _t_buflen;

    /*    variable for polling  */
    size_t _p_byteread;
    char *_p_buf;
    size_t _p_buflen;

    size_t _bufWrite(uint32_t address, char *buf, size_t buflen);
    size_t _bufRead(char *buf, size_t buflen);
    void _seekAddress(uint32_t address);

    void _timer_bufWrite();
    void _polling_bufRead();

    static void __onTimer(void* obj){
      ((EEPROM_Class*)obj)->_timer_bufWrite();
    }

    static void __onPoll(void *obj){
      ((EEPROM_Class*)obj)->_polling_bufRead();
    }

  public:
    EEPROM_Class(uint8_t address, size_t size, size_t pagesize, uint16_t delay_t){
      _addr = address;
      _size = size;
      _pagesize = pagesize;
      _delayms = delay_t;

      Wire.begin();
    }

    // can write more than the pagesize, but it will block until all data is sent
    size_t bufWriteBlock(uint32_t address, char *buf, size_t buflen);

    // can write more than the pagesize, but it will send in async until all data is sent
    // don't free the buffer, let the function free the buffer
    void bufWriteAsync(uint32_t address, char *buf, size_t buflen, EEPROM_Cb cb, void* obj);

    size_t bufReadBlock_currAddr(char *buf, size_t buflen);
    size_t bufReadBlock(uint32_t address, char *buf, size_t buflen);

    // polling based, after reading bytes of BUFFER_LENGTH, it will pass the cpu resource to another task, until polled again
    void bufReadAsync(uint32_t address, char *buf, size_t buflen, EEPROM_Cb cb, void* obj);

    uint32_t getMemAddress();
    uint32_t getMemSize();
    bool getIsReady();
};

class AT24C256: public EEPROM_Class{
  public:
    AT24C256(uint8_t address): EEPROM_Class(
      address,
      32768,
      64,
      5
    ){
      _addresssize = sizeof(uint16_t);
    }
};


#endif