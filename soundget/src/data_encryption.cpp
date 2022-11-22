#include "data_encryption.hpp"

#define SETIFZERO(data) (data? data: 1)
#define EN_RANDOMBUFFER 5

data_encryption::data_encryption(){
  _key = NULL;
  _keylen = 0;
}

void data_encryption::_deletePrevious(){
  if(_key != NULL){
    free(_key);
    _key = NULL;
  }
}

void data_encryption::useKey(const char *key, size_t len, bool _sameaddr){
  _deletePrevious();

  if(_sameaddr)
    _keyaddr = key;
  else{
    _key = (char*)malloc(len);
    memcpy(_key, key, len);
  }

  _keylen = len;
  _usesameaddr = _sameaddr;
}

void data_encryption::useKey(const std::string &key){
  useKey(key.c_str(), key.size());
}

void data_encryption::encryptOrDecryptData(char *buffer, size_t bufferlen){
  encryptOrDecryptData(buffer, bufferlen, 0);
}

void data_encryption::encryptOrDecryptData(char *buffer, size_t bufferlen, size_t idx_offset){
  if(_keylen == 0)
    return;

  const char *__key = _usesameaddr? _keyaddr: _key;
  for(size_t i = 0; i < bufferlen; i++)
    buffer[i] ^= __key[(i+idx_offset)%_keylen];
}

std::pair<const char*, size_t> data_encryption::getKey(){
  return {_key, _keylen};
}