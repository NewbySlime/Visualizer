#include "data_encryption.hpp"
#include "time_rand.hpp"

#include "Arduino.h"

#define SETIFZERO(data) (data? data: 1)
#define EN_RANDOMBUFFER 5

#define EN_MINKEYLEN 32
#define EN_MAXKEYLEN 64

data_encryption::data_encryption(){
  _key = "";
  TimeRandom.setBufferLength(EN_RANDOMBUFFER);
}

void data_encryption::useKey(const char *key, size_t len){
  _key.clear();
  _key.append(key, len);
}

void data_encryption::useKey(const std::string &key){
  _key = key;
}

std::string data_encryption::createKey(){
  // do something when one of the random is below the treshold
  auto _pr = TimeRandom.getRandomBuffer();
 
  int _rand1 = SETIFZERO(_pr.first[0]) * SETIFZERO(_pr.first[2]);

  std::string newkey = "";
  int newlen = min(max((_rand1 & 0xff) * ((_rand1 >> 8) & 0xff) & 0xff, EN_MINKEYLEN), EN_MAXKEYLEN);

  newkey.resize(newlen, ((_rand1 >> 16) & 0xff));

  int _intcharlen = sizeof(int) * EN_RANDOMBUFFER;
  const char *_intchar = reinterpret_cast<const char*>(&_pr.first[1]);
  for(int i = 0; i < newlen; i++){
    int n = (int)newkey[i];
    n += _intchar[i % _intcharlen];
    n *= _intchar[(i+(i/sizeof(int))) % _intcharlen];
    n /= SETIFZERO(_intchar[(i+2) % _intcharlen]);

    newkey[i] = (char)(n & 0xff);
  }

  return newkey;
}

const std::string &data_encryption::createAndUseKey(){
  _key = createKey();
  return _key;
}

const std::string *data_encryption::getKey(){
  return &_key;
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