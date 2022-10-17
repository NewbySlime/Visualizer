#include "ByteIterator.hpp"
#include "algorithm"
#include "string.h"

ByteIterator::ByteIterator(const void *data, size_t datalength){
  _datalength = data? datalength: 0;
  _data = reinterpret_cast<const char*>(data);
  _dataidx = 0;
}

void ByteIterator::seek(size_t idx){
  if(idx >= 0 && idx < _datalength)
    _dataidx = idx;
}

template<typename _type> void ByteIterator::getVar(_type &variable){
  if((_dataidx+sizeof(_type)) > _datalength)
    return;

  memcpy(&variable, _data+_dataidx, sizeof(_type));
  _dataidx += sizeof(_type);
}

size_t ByteIterator::getVar(char *buf, size_t buflen){
  if(!buflen)
    return 0;

  size_t readlen = min(buflen, _datalength-_dataidx);
  memcpy(buf, _data+_dataidx, readlen);

  _dataidx += readlen;
  return readlen;
}

size_t ByteIterator::available(){
  return _datalength-_dataidx;
}

template void ByteIterator::getVar<float>(float&);
template void ByteIterator::getVar<int>(int&);
template void ByteIterator::getVar<short>(short&);
template void ByteIterator::getVar<unsigned short>(unsigned short&);
template void ByteIterator::getVar<unsigned char>(unsigned char&);


ByteIteratorR::ByteIteratorR(void *buf, size_t datalength){
  _datalength = buf? datalength: 0; // TODO maybe give error
  _data = reinterpret_cast<char*>(buf);
  _dataidx = 0;
}

void ByteIteratorR::seek(size_t idx){
  if(idx >= 0 && idx < _datalength)
    _dataidx = idx;
}

template<typename _type> bool ByteIteratorR::setVar(_type variable){
  if((_dataidx+sizeof(_type)) > _datalength)
    return false;

  memcpy(_data+_dataidx, &variable, sizeof(_type));
  _dataidx += sizeof(_type);
  return true;
}

size_t ByteIteratorR::setVar(const char *data, size_t datalength){
  if(!datalength)
    return 0;
    
  size_t writelen = min(datalength, _datalength-_dataidx);
  if(writelen > 0){
    memcpy(_data+_dataidx, data, writelen);
    _dataidx += writelen;
  }

  return writelen;
}

size_t ByteIteratorR::available(){
  return _datalength-_dataidx;
}

template bool ByteIteratorR::setVar<float>(float);
template bool ByteIteratorR::setVar<int>(int);
template bool ByteIteratorR::setVar<unsigned int>(unsigned int);
template bool ByteIteratorR::setVar<unsigned short>(unsigned short);