#include "password.hpp"
#include "file_system.hpp"
#include "ByteIterator.hpp"
#include "utility"


#define PASSWORD_FILEID 0x12
#define PASSWORD_MAX_ALTERNATES 3


std::vector<std::pair<std::string, std::string>> _passdata;



void _PWD_saveToStorage(){
  size_t _datasize = sizeof(uint16_t) * (_passdata.size()*2+1);
  for(int i = 0; i < _passdata.size(); i++)
    _datasize += _passdata[i].first.size() + _passdata[i].second.size();
  
  char *_data = (char*)malloc(_datasize);
  ByteIteratorR _bir(_data, _datasize);

  _bir.setVar((uint16_t)_passdata.size());
  for(int i = 0; i < _passdata.size(); i++){
    auto *_p = &_passdata[i];

    _bir.setVar((uint16_t)_p->first.size());
    _bir.setVarStr(_p->first.c_str(), _p->first.size());

    _bir.setVar((uint16_t)_p->second.size());
    _bir.setVarStr(_p->second.c_str(), _p->second.size());
  }

  EEPROM_FS.write_file(PASSWORD_FILEID, _data, _datasize);
  //free(_data);
}


void PWD_init(){
  if(EEPROM_FS.file_exist(PASSWORD_FILEID)){
    Serial.printf("Password will be read from EEPROM\n");
    size_t _fsize = EEPROM_FS.file_size(PASSWORD_FILEID);
    char _data[_fsize];
    EEPROM_FS.read_file(PASSWORD_FILEID, _data, NULL, NULL);
    EEPROM_FS.complete_tasks();


    // parsing from raw data
    ByteIterator _bi(_data, _fsize);
    uint16_t passsize; _bi.getVar(passsize);

    for(int i = 0; i < passsize; i++){
      uint16_t _dataread;
      uint16_t ssid_len; _bi.getVar(ssid_len);

      char _str[ssid_len+1];
      _dataread = _bi.getVarStr(_str, ssid_len);
      if(_dataread != ssid_len)
        break;

      uint16_t pwd_len; _bi.getVar(pwd_len);

      char _strpwd[pwd_len+1];
      _dataread = _bi.getVarStr(_strpwd, pwd_len);
      if(_dataread != pwd_len)
        break;

      _str[ssid_len] = '\0';
      _str[pwd_len] = '\0';

      _passdata.insert(_passdata.end(), std::pair<std::string, std::string>(_str, _strpwd));
    }
  }
  else{
    Serial.printf("Password will be initiated\n");
    // create new file then write the file with default password
    _passdata.clear();
    _passdata.resize(1);

    _passdata[0] = std::pair(PWD_getDefaultSSID(), PWD_getDefaultPassword());
    _PWD_saveToStorage();
  }
}


size_t PWD_alternateCount(){
  return _passdata.size();
}

size_t PWD_maxAlternates(){
  return PASSWORD_MAX_ALTERNATES;
}


std::pair<const char*, const char*> PWD_getAuth(int idx){
  if(idx < 0 || idx >= _passdata.size())
    return {NULL, NULL};

  return {_passdata[idx].first.c_str(), _passdata[idx].second.c_str()};
}

bool PWD_setAuth(int idx, std::string ssid, std::string pwd){
  if(idx < 0 || idx > _passdata.size())
    return false;
  
  _passdata.insert(_passdata.begin()+idx, {ssid, pwd});
  _PWD_saveToStorage();
  return true;
}


bool PWD_removeAuth(int idx){
  if(idx < 0 || idx >= _passdata.size())
    return false;

  _passdata.erase(_passdata.begin()+idx);
  _PWD_saveToStorage();
  return true;
}

bool PWD_addAuth(std::string ssid, std::string pwd){
  if(_passdata.size() >= PASSWORD_MAX_ALTERNATES)
    return false;

  _passdata.insert(_passdata.begin(), {ssid, pwd});
  _PWD_saveToStorage();
  return true;
}