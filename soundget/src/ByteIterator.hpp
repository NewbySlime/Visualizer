#ifndef BYTEITERATOR_HEADER
#define BYTEITERATOR_HEADER

#include "cstdlib"
using namespace std;

class ByteIterator{
  private:
    size_t _datalength;
    size_t _dataidx;
    const char *_data;
    

  public:
    ByteIterator(const void *data, size_t datalength);
    
    template<typename _type> void getVar(_type &variable);

    size_t getVar(void *buffer, size_t bufferlen){
      return getVar(reinterpret_cast<char*>(buffer), bufferlen);
    }

    // return how many times it reads
    size_t getVar(char *buffer, size_t bufferlen);
};

class ByteIteratorR{
  private:
    size_t _datalength;
    size_t _dataidx;
    char *_data;

  public:
    ByteIteratorR(void *buffer, size_t datalength);

    // if successful, it'll return true
    template<typename _type> bool setVar(_type variable);

    size_t setVar(void *data, size_t datalength){
      return setVar(reinterpret_cast<const char*>(data), datalength);
    }

    // return how many times it writes
    size_t setVar(const char *data, size_t datalength);
};

#endif