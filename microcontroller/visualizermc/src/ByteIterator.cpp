#include "ByteIterator.hpp"

ByteIterator::ByteIterator(const char *data, size_t datalength){
  _datalength = datalength;
  _data = data;
  _dataidx = 0;
}

template<typename _type> void ByteIterator::getVar(_type &variable){
  if((_dataidx+sizeof(_type)) > _datalength)
    return;

  memcpy(&variable, _data+_dataidx, sizeof(_type));
  _dataidx += sizeof(_type);
}

size_t ByteIterator::getVar(char *buf, size_t buflen){
  size_t readlen = min(buflen, _datalength-_dataidx);
  memcpy(buf, _data+_dataidx, readlen);

  _dataidx += readlen;
  return readlen;
}

template void ByteIterator::getVar<float>(float&);
template void ByteIterator::getVar<int>(int&);
template void ByteIterator::getVar<unsigned short>(unsigned short&);


ByteIteratorR::ByteIteratorR(void *buf, size_t datalength){
  _data = reinterpret_cast<char*>(buf);
  _datalength = datalength;
  _dataidx = 0;
}

template<typename _type> bool ByteIteratorR::setVar(_type &variable){
  if((_dataidx+sizeof(_type)) > _datalength)
    return false;

  memcpy(_data+_dataidx, &variable, sizeof(_type));
  _dataidx += sizeof(_type);
  return true;
}

size_t ByteIteratorR::setVar(const char *data, size_t datalength){
  size_t writelen = min(datalength, _datalength-_dataidx);
  memcpy(_data, data, writelen);

  _dataidx += writelen;
  return writelen;
}

template bool ByteIteratorR::setVar<float>(float&);
template bool ByteIteratorR::setVar<int>(int&);
template bool ByteIteratorR::setVar<unsigned short>(unsigned short&);