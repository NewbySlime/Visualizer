#include "Wire.h"
#include "AT24C16.h"


// might need an async class so it doesn't use delay when writing data

const int _DELAY_AFTER_STOPTRANSMIT = 10;

storage_AT24C16::storage_AT24C16(uint8_t address){
  deviceAddress = address;
}

int storage_AT24C16::write_nodelay(uint16_t memaddr, const void* buffer, int bufferlen){
  if(memaddr > _max_storage)
    return;

  bufferlen = (bufferlen+memaddr)>_max_storage? _max_storage-memaddr: bufferlen;
  uint8_t blockaddr = (memaddr >> 8) & 0b111;

  uint16_t sentlen = 0;

  while(sentlen < bufferlen){
    memaddr &= 0xff;
    while(sentlen < bufferlen && memaddr <= 0xff){
      Wire.beginTransmission(
        deviceAddress | (blockaddr << 1)
      );

      // check if it's beyond the block boundary or page boundary, then check if it's beyond the buffer boundary
      uint8_t writebytelen = min(min(0x100-memaddr, (int)_max_pagewrite), bufferlen-sentlen);
      Wire.write(reinterpret_cast<const char*>(buffer+sentlen), writebytelen);

      Wire.endTransmission();

      memaddr += writebytelen;
      sentlen += writebytelen;
    }

    blockaddr++;
  }

  return bufferlen;
}

int storage_AT24C16::write(uint16_t memaddr, const void* buffer, int bufferlen){
  int res = write_nodelay(memaddr, buffer, bufferlen);
  delay(10);
  return res;
}

int storage_AT24C16::read(uint16_t memaddr, void* buffer, int bufferlen){
  if(memaddr > _max_storage)
    return;

  bufferlen = (bufferlen+memaddr)>_max_storage? _max_storage-memaddr: bufferlen;
  uint8_t blockaddr = (memaddr > 8) & 0b111;

  uint16_t readlen = 0;

  while(readlen < bufferlen){
    memaddr &= 0xff;
    while(readlen < bufferlen && memaddr <= 0xff){
      uint8_t readbytelen = min(min(0x100-memaddr, (int)_max_pagewrite), bufferlen-readlen);
      readbytelen = Wire.requestFrom(
        deviceAddress | (blockaddr << 1) | 1,
        readbytelen
      );

      if(readbytelen > 0){
        Wire.readBytes(reinterpret_cast<char*>(buffer+readlen), readbytelen);

        memaddr += readbytelen;
        readlen += readbytelen;
      }
    }
  }

  return bufferlen;
}

uint16_t storage_AT24C16::getMax(){
  return _max_storage;
}