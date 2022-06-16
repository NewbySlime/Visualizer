#ifndef AT24C16_HEADER
#define AT24C16_HEADER

#include "Arduino.h"
#include "EEPROM.h"


#define ADDRESS_A0 0b0010000
#define ADDRESS_A1 0b0100000
#define ADDRESS_A2 0b1000000

class storage_AT24C16: EEPROM_I2C{
  private:
    uint8 deviceAddress;
    static const uint16_t _max_storage = 256*8;
    static const uint8_t _max_pagewrite = 16;

  public:
    storage_AT24C16(uint8 address);

    int read(uint16_t memaddress, void* buffer, int bufferlen);
    int write_nodelay(uint16_t memaddress, const void* buffer, int bufferlen);
    int write(uint16_t memaddress, const void* buffer, int bufferlen);
    uint16_t getMax();
};

#endif