#include "data_encryption.hpp"

#define SETIFZERO(data) (data? data: 1)
#define EN_RANDOMBUFFER 5

data_encryption::data_encryption(){
  _key = "";
}

void data_encryption::useKey(std::string key){
  _key = key;
}

void data_encryption::encryptOrDecryptData(char *buffer, size_t bufferlen){
  encryptOrDecryptData(buffer, bufferlen, 0);
}

void data_encryption::encryptOrDecryptData(char *buffer, size_t bufferlen, size_t idx_offset){
  if(_key.size() == 0)
    return;

  for(size_t i = 0; i < bufferlen; i++)
    buffer[i] ^= _key[(i+idx_offset)%_key.size()];
}