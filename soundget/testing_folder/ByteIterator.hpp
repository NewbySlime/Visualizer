#ifndef BYTEITERATOR_HEADER
#define BYTEITERATOR_HEADER

#include "vector"
#include "cmath"
#include "utility"
#include "data_encryption.hpp"

class ByteIterator{
  protected:
    size_t _datalength;
    size_t _dataidx;
    const char *_data;

  public:
    ByteIterator(const char *data, size_t datalength){
      _datalength = data? datalength: 0;
      _data = data;
      _dataidx = 0;
    }
    
    void seek(size_t idx){
      if(idx >= 0 && idx < _datalength)
        _dataidx = idx;
    }
    
    template<typename _type> bool getVar(_type &variable){
      if((_dataidx+sizeof(_type)) > _datalength)
        return false;

      getVarStr(reinterpret_cast<char*>(&variable), sizeof(_type));
      return true;
    }

    // if len is 0, then get data until end of buffer
    // return how many object retrieved
    template<typename _type> size_t getVar(std::vector<_type> &vars, size_t len = 0){
      if(len > 0)
        if(available() < (sizeof(_type)*len))
          return 0;
        else
          for(size_t i = 0; i < len; i++){
            _type obj; getVar(obj);
            vars.push_back(obj);
          }
      else
        while(available() >= sizeof(_type)){
          _type obj; getVar(obj);
          vars.push_back(obj);

          len++;
        }
      
      return len;
    }

    template<typename _t1, typename _t2> bool getVar(std::pair<_t1, _t2> &p){
      if(available() < (sizeof(_t1)+sizeof(_t2)))
        return false;
      
      _t1 o1; getVar(o1);
      _t2 o2; getVar(o2);

      p = {o1, o2};

      return true;
    }

    size_t getVarStr(void *buffer, size_t bufferlen){
      return getVarStr(reinterpret_cast<char*>(buffer), bufferlen);
    }

    // return how many times it reads
    virtual size_t getVarStr(char *buf, size_t buflen){
      size_t readlen = std::min(buflen, _datalength-_dataidx);
      memcpy(buf, _data+_dataidx, readlen);

      _dataidx += readlen;
      return readlen;
    }
    
    size_t available(){
      return _datalength-_dataidx;
    }
};

class ByteIteratorR{
  protected:
    size_t _datalength;
    size_t _dataidx;
    char *_data;

  public:
    ByteIteratorR(void *buffer, size_t datalength){
      _datalength = buffer? datalength: 0;
      _data = reinterpret_cast<char*>(buffer);
      _dataidx = 0;
    }

    size_t tellidx(){
      return _dataidx;
    }

    void seek(size_t idx){
      if(idx >= 0 && idx < _datalength)
        _dataidx = idx;
    }

    // if successful, it'll return true
    template<typename _type> bool setVar(_type variable){
      if((_dataidx+sizeof(_type)) > _datalength)
        return false;

      setVarStr(reinterpret_cast<const char*>(&variable), sizeof(_type));
      return true;
    }

    // if len is 0, then data will be copied until end of vector
    // return how many object copied to buffer
    template<typename _type> size_t setVar(std::vector<_type> &vars, size_t len = 0){
      if(len > 0)
        if(available() < (sizeof(_type)*len))
          return 0;
        else
          for(size_t i = 0; i < len; i++){
            setVar(vars[i]);
          }
      else
        while(available() >= sizeof(_type) && len < vars.size()){
          setVar(vars[len]);
          len++;
        }
      
      return len;
    }

    template<typename _t1, typename _t2> bool setVar(std::pair<_t1, _t2> &p){
      if(available() < (sizeof(_t1)+sizeof(_t2)))
        return false;
      
      setVar(p.first);
      setVar(p.second);

      return true;
    }

    size_t setVarStr(void *data, size_t datalength){
      return setVarStr(reinterpret_cast<const char*>(data), datalength);
    }

    // return how many times it writes
    virtual size_t setVarStr(const char *data, size_t datalength){
      size_t writelen = std::min(datalength, _datalength-_dataidx);
      memcpy(_data+_dataidx, data, writelen);

      _dataidx += writelen;
      return writelen;
    }

    size_t available(){
      return _datalength-_dataidx;
    }
};


class ByteIterator_Encryption: public ByteIterator{
  private:
    data_encryption *_key;

  public:
    ByteIterator_Encryption(const char *data, size_t datalen, data_encryption *_encryption): ByteIterator(data, datalen){
      _key = _encryption;
    }

    size_t getVarStr(char *data, size_t datalen){
      size_t _len = ByteIterator::getVarStr(data, datalen);
      _key->encryptOrDecryptData(data, _len, _dataidx-_len);
      return _len;
    }
};

class ByteIteratorR_Encryption: public ByteIteratorR{
  private:
    data_encryption *_key;
  
  public:
    ByteIteratorR_Encryption(char *data, size_t datalen, data_encryption *_encryption): ByteIteratorR(data, datalen){
      _key = _encryption;
    }

    size_t setVarStr(const char *data, size_t datalen){
      char *_dataaddr = _data+_dataidx;

      size_t _len = ByteIteratorR::setVarStr(data, datalen);
      _key->encryptOrDecryptData(_dataaddr, _len, _dataidx-_len);
      return _len;
    }
};

#endif