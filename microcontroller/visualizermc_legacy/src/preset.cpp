#include "preset.hpp"
#include "color.hpp"
#include "leddriver-master.hpp"

#include "misc.hpp"


#define R_IFFALSE(eval)\
if(!eval){\
  Serial.printf("false when " #eval "\n");\
  return false;\
}

#define _preseterrcheck(eval) \
{preset_error err = eval;\
if(err & 0xff00)\
  return err;}



preset *_new_def_preset(){
  preset *_p = new preset{
    .presetName = "Default",
    .colorMode = preset_colorMode::RGB_p,
    .brightness = 1.0f,
    .ValuePosx = 0.0f,
    .speedChange = 0.1f,
    .windowSlide = 0.1f,
    .inUnion = false,
    .colorShifting = preset_colorMode::RGB_p,
    .brightnessMode = preset_brightnessMode::Individual,
    .maxIntensity = 20.0f,
    .minIntensity = 0.0f,
  };
  
  _p->splitParts.push_back(part{
    .start_led = 0,
    .end_led = CONST_LEDNUM,
    .range_start = 0.0f,
    .range_end = 1.0f,
    .reversed = false,
    .channel = 0
  });

  // rainbow representation in hex
  color cols[] = {0xff0000, 0xffe900, 0x20ff00, 0x00fff3, 0x0800ff, 0xff00fb};
  float ranges[] = {0.05f, 0.16f, 0.32f, 0.48f, 0.64f, 0.80f};
  _p->colorRange = colorRanges{cols, ranges, 6};
  
  return _p;
}


#if defined(PRESET_USE_EEPROM)

#define _PRESET_FILECODE 0x0f00
#define _PRESET_LASTFILECODE 0x1000
#define _PRESET_MAXCOUNT 0xff


presetData::presetData(){}

int presetData::_getPresetNamelen(uint16_t idx){
  char _tmp[MAX_PRESET_NAME_LENGTH+1];
  EEPROM_FS.read_file(_getFileCode(idx), _tmp, NULL, NULL, MAX_PRESET_NAME_LENGTH);
  EEPROM_FS.complete_tasks();

  _tmp[MAX_PRESET_NAME_LENGTH] = '\0';
  return strlen(_tmp);
}

uint16_t presetData::_getFileCode(uint16_t idx){
  return _PRESET_FILECODE | _fpreset_codes[idx];
}

void presetData::_updateFileCode(){
  char *_datap = (char*)malloc(_presetLen);
  for(size_t i = 0; i < _presetLen; i++)
    _datap[i] = _fpreset_codes[i];
  
  EEPROM_FS.write_file(_PRESET_FILECODE, _datap, _presetLen);
}

void presetData::_loadData(){
  if(EEPROM_FS.file_exist(_PRESET_FILECODE)){
    EEPROM_FS.read_file(_PRESET_LASTFILECODE, reinterpret_cast<char*>(&_lastPreset), NULL, NULL);
    EEPROM_FS.complete_tasks();

    _presetLen = EEPROM_FS.file_size(_PRESET_FILECODE);
    char *_datapc = (char*)malloc(_presetLen);
    EEPROM_FS.read_file(_PRESET_FILECODE, _datapc, NULL, NULL);
    EEPROM_FS.complete_tasks();

    _fpreset_codes.resize(_presetLen);
    for(size_t i = 0; i < _presetLen; i++)
      _fpreset_codes[i] = _datapc[i];
    
    free(_datapc);
  }
  else{
    _lastPreset = 0;
    _presetLen = 1;
    preset *_currentP = _new_def_preset();
    _fpreset_codes.resize(1);
    _fpreset_codes[0] = 1;

    _updateFileCode();

    size_t _datalen = presetSizeInBytes(*_currentP);
    char *_data = (char*)malloc(_datalen);
    copyToMemory(_data, _datalen, *_currentP);
    
    Serial.printf("_data %d\n", _datalen);
    for(int i = 0; i < _datalen; i++){
      if((i % 16) == 0)
        Serial.printf("\nidx %d 0x%X\n\t", i, i);
      
      Serial.printf("0x%X ", _data[i]);
    }

    EEPROM_FS.write_file(_PRESET_FILECODE | 1, _data, _datalen);

    uint8_t *_lastp = (uint8_t*)malloc(sizeof(uint8_t));
    *_lastp = _lastPreset;

    EEPROM_FS.write_file(_PRESET_LASTFILECODE, reinterpret_cast<char*>(_lastp), sizeof(uint8_t));
    
    EEPROM_FS.complete_tasks();

    delete _currentP;
  }
}

preset_error presetData::_setData(int idx, preset &p){
  if(idx >= _PRESET_MAXCOUNT)
    return preset_error::max_preset_exceeded;
  
  size_t _datalen = presetSizeInBytes(p);
  char *_data = (char*)malloc(_datalen);

  copyToMemory(_data, _datalen, p);

  auto _err = EEPROM_FS.write_file(_getFileCode(idx), _data, _datalen);
  if(_err != fs_error::ok)
    return preset_error::storage_fault;
  return preset_error::no_error;
}

preset_error presetData::_resizePreset(uint16_t len){
  if(_presetLen == len)
    return preset_error::no_error;

  if(len >= _PRESET_MAXCOUNT)
    return preset_error::max_preset_exceeded;
  
  int _im = _presetLen-len;
  for(int i = 0; i < _im; i++)
    EEPROM_FS.delete_file(_getFileCode(_presetLen-i-1));
  
  if(_im > 0)
    _fpreset_codes.resize(len);
    
  std::sort(_fpreset_codes.begin(), _fpreset_codes.end());

  _im = len-_presetLen;
  int prevn = 1;
  while(_im > 0){
    auto _iter = std::lower_bound(_fpreset_codes.begin(), _fpreset_codes.end(), prevn);
    if(_iter == _fpreset_codes.end() || *_iter != prevn){
      Serial.printf("_iter %d prevn %d\n", *_iter, prevn);
      _fpreset_codes.insert(_iter, prevn);
      _im--;
    }
    
    prevn++;
  }

  for(int i = 0; i < _fpreset_codes.size(); i++)
    Serial.printf("_fc %x\n", _getFileCode(i));

  _lastPreset = min((int)len, _lastPreset);
  _presetLen = len;

  _updateFileCode();
  return preset_error::no_error;
}

preset *presetData::_getData(int idx){
  if(idx < 0 && idx >= _presetLen)
    return NULL;

  uint16_t _fpcode = _getFileCode(idx);
  size_t _datalen = EEPROM_FS.file_size(_fpcode);
  char *_data = (char*)malloc(_datalen);
  
  EEPROM_FS.read_file(_fpcode, _data, NULL, NULL);
  EEPROM_FS.complete_tasks();
  Serial.printf("done reading\n");

  preset *_currentPreset = new preset();
  setPresetFromMem(_data, _datalen, *_currentPreset);

  free(_data);
  return _currentPreset;
}

int presetData::_getPresetName(uint16_t idx, char *str){
  // getting string
  if(str){
    int _strlen = _getPresetNamelen(idx);
    
    EEPROM_FS.read_file(_getFileCode(idx), str, NULL, NULL, _strlen);
    EEPROM_FS.complete_tasks();

    str[_strlen] = '\0';
  }

  // getting size of str
  else
    return _getPresetNamelen(idx);
  
  return 0;
}

preset_error presetData::_checkStorage(){
  if(EEPROM_FS.is_storage_bound())
    return preset_error::no_error;
  else
    return preset_error::storage_fault;
}

#else

//  in getPreset, it will create another object, so don't use 
presetData::presetData(){
  _presetLen = 0;
  _lastPreset = 0;

  _presetDatas = (char**)malloc(0);
  _presetDatasLen = (size_t*)malloc(0);
}
// if nothing is used for storage, it will use user RAM as storage

// since no data to get from, this will use hardcoded preset
void presetData::_loadData(){
  _presetLen++;

  preset *_p = _new_def_preset();
  size_t _pbuflen = presetSizeInBytes(*_p);
  char *_pbuf = (char*)malloc(_pbuflen);

  copyToMemory(_pbuf, _pbuflen, *_p);

  _presetDatas = (char**)realloc(_presetDatas, sizeof(char*)*_presetLen);
  _presetDatas[_presetLen-1] = _pbuf;

  _presetDatasLen = (size_t*)realloc(_presetDatasLen, sizeof(size_t)*_presetLen);
  _presetDatasLen[_presetLen-1] = _pbuflen;

  delete _p;
}

preset_error presetData::_setData(int idx, preset &p){
  if(idx < 0 || idx >= _presetLen)
    return preset_error::max_preset_exceeded;

  if(_presetDatas[idx])
    free(_presetDatas[idx]);

  size_t _pbuflen = presetSizeInBytes(p);
  char *_pbuf = (char*)malloc(_pbuflen);
  
  copyToMemory(_pbuf, _pbuflen, p);

  _presetDatas[idx] = _pbuf;
  _presetDatasLen[idx] = _pbuflen;

  return preset_error::no_error;
}

void presetData::_setLastUsedpreset(int idx){
  _lastPreset = idx;
}

preset_error presetData::_resizePreset(uint16_t len){
  if(len <= 0 || len >= MAX_PRESET_NUM)
    return preset_error::max_preset_exceeded;

  // just in case if the not all data can be received
  // note, visualizer will send all preset data, so better free all the buffer
  for(int i = 0; i < len; i++){
    if(_presetDatas[i])
      free(_presetDatas[i]);

    _presetDatas[i] = NULL;
    _presetDatasLen[i] = 0;
  }

  _presetDatas = (char**)realloc(_presetDatas, sizeof(char*)*len);
  _presetDatasLen = (size_t*)realloc(_presetDatasLen, sizeof(size_t)*len);

  _presetLen = len;

  return preset_error::no_error;
}

preset *presetData::_getData(int idx){
  if(idx >= 0 && idx < _presetLen){
    preset *_p = new preset();
    setPresetFromMem(_presetDatas[idx], _presetDatasLen[idx], *_p);

    return _p;
  }
  else
    return NULL;
}

// nothing to check, leave no_error
preset_error presetData::_checkStorage(){
  return preset_error::no_error;
}
#endif



preset_error presetData::initPreset(){
  _preseterrcheck(_checkStorage());
  _loadData();
  return preset_error::no_error;
}

preset_error presetData::setPreset(uint16_t idx, preset &p){
  return _setData(idx, p);
}

void presetData::setLastUsedPreset(int idx){
  _setLastUsedpreset(idx);
}

preset_error presetData::resizePreset(uint16_t len){
  _lastPreset = 0;
  return _resizePreset(len);
}

int presetData::manyPreset(){
  return _presetLen;
}

preset *presetData::getPreset(uint16_t idx){
  _lastPreset = idx;
  return _getData(idx);
}

preset *presetData::getLastUsedPreset(){
  return getPreset(_lastPreset);
}

int presetData::getLastUsedPresetIdx(){
  return _lastPreset;
}

int presetData::getPresetName(uint16_t idx, char *str){
  return _getPresetName(idx, str);
}

size_t presetData::presetSizeInBytes(preset &p){
  return
    sizeof(preset) +
  
    // recalculate some variables
    -sizeof(int) + sizeof(preset::inUnion) +

    // subtract the dynamic storage size
    -sizeof(p.presetName) +
    -sizeof(p.colorRange) +
    -sizeof(p.splitParts) +

    // then add the real size
    MAX_PRESET_NAME_LENGTH +

    // the dynamic storages will have a number in the front of the data, as the length
    sizeof(uint16_t) + (p.colorRange.colors.size() * (sizeof(color)+sizeof(float))) +
    sizeof(uint16_t) + (p.splitParts.size() * (
      sizeof(part)

      // recalculate some variables
      -sizeof(int) + sizeof(part::reversed)
    ));
  ;
}

size_t presetData::copyToMemory(ByteIteratorR &memwrite, preset& p){
  Serial.printf("Calling bir's preset\n");
  Serial.printf("memwrite %d\n", memwrite.available());
  

  if(memwrite.available() < presetSizeInBytes(p))
    return 0;
  
  Serial.printf("presetName, %s\n", p.presetName.c_str());
  R_IFFALSE(memwrite.setVarStr(p.presetName.c_str(), p.presetName.size()));

  char *dummychar = (char*)calloc(sizeof(char), MAX_PRESET_NAME_LENGTH-p.presetName.size());
  R_IFFALSE(memwrite.setVarStr(dummychar, MAX_PRESET_NAME_LENGTH-p.presetName.size()));
  free(dummychar);
  Serial.printf("memwrite %d\n", memwrite.available());

  R_IFFALSE(memwrite.setVar(p.colorMode));
  R_IFFALSE(memwrite.setVar(p.brightness));
  R_IFFALSE(memwrite.setVar(p.ValuePosx));
  R_IFFALSE(memwrite.setVar(p.speedChange));
  R_IFFALSE(memwrite.setVar(p.windowSlide));
  R_IFFALSE(memwrite.setVar(p.inUnion));
  R_IFFALSE(memwrite.setVar(p.colorShifting));
  R_IFFALSE(memwrite.setVar(p.brightnessMode));
  R_IFFALSE(memwrite.setVar(p.maxIntensity));
  R_IFFALSE(memwrite.setVar(p.minIntensity));

  Serial.printf("memwrite %d\n", memwrite.available());

  Serial.printf("colrange %d\n", p.colorRange.colors.size());
  R_IFFALSE(memwrite.setVar((uint16_t)p.colorRange.colors.size()));
  for(auto col: p.colorRange.colors){
    R_IFFALSE(memwrite.setVar(col.first));
    R_IFFALSE(memwrite.setVar(col.second));
  }

  Serial.printf("memwrite %d\n", memwrite.available());

  Serial.printf("parts %d\n", p.splitParts.size());
  R_IFFALSE(memwrite.setVar((uint16_t)p.splitParts.size()));
  for(int i = 0; i < p.splitParts.size(); i++)
    R_IFFALSE(presetData::copyPartToMemory(memwrite, p.splitParts[i]));

  Serial.printf("memwrite %d\n", memwrite.available());
  return memwrite.tellidx();
}

bool __cmp1(const part &p1, const part &p2){
  return p1.start_led < p2.start_led;
}

bool presetData::setPresetFromMem(ByteIterator &bi, preset &p){
  if(bi.available() < MAX_PRESET_NAME_LENGTH)
    return false;
  
  char _tmp[MAX_PRESET_NAME_LENGTH+1];
  if(bi.getVarStr(_tmp, MAX_PRESET_NAME_LENGTH) != MAX_PRESET_NAME_LENGTH)
    return false;
  _tmp[MAX_PRESET_NAME_LENGTH] = '\0';

  Serial.printf("name %s\n", _tmp);

  p.presetName = _tmp;

  R_IFFALSE(bi.getVar(p.colorMode));
  R_IFFALSE(bi.getVar(p.brightness));
  R_IFFALSE(bi.getVar(p.ValuePosx));
  R_IFFALSE(bi.getVar(p.speedChange));
  R_IFFALSE(bi.getVar(p.windowSlide));
  R_IFFALSE(bi.getVar(p.inUnion));
  R_IFFALSE(bi.getVar(p.colorShifting));
  R_IFFALSE(bi.getVar(p.brightnessMode));
  R_IFFALSE(bi.getVar(p.maxIntensity));
  R_IFFALSE(bi.getVar(p.minIntensity));


  // getting color range data
  p.colorRange.colors.clear();
  uint16_t colorlen; R_IFFALSE(bi.getVar(colorlen));
  if(colorlen > MAX_PRESET_COLOR)
    return false;

  p.colorRange.colors.reserve(colorlen);
  for(int i = 0; i < colorlen; i++){
    float range{}; R_IFFALSE(bi.getVar(range));
    color col{}; R_IFFALSE(bi.getVar(col));

    p.colorRange.addColor(col, range);
  }
  

  // getting split parts data
  p.splitParts.clear();
  uint16_t partlen; R_IFFALSE(bi.getVar(partlen));
  if(partlen > MAX_PRESET_PART)
    return false;

  p.splitParts.resize(partlen);
  for(int i = 0; i < partlen; i++)
    R_IFFALSE(presetData::setPartFromMem(bi, p.splitParts[i]));

  // just in case
  std::sort(p.splitParts.begin(), p.splitParts.end(), __cmp1);

  return true;
}

size_t presetData::presetSizeInBytes(int idx){
#if defined(PRESET_USE_EEPROM)
  if(idx >= _PRESET_MAXCOUNT)
    return 0;

  return EEPROM_FS.file_size(_PRESET_FILECODE | _fpreset_codes[idx]);
#else
  preset *data = getPreset(idx);
  if(!data)
    return 0;

  size_t res = presetSizeInBytes(*data);
  delete data;

  return res;
#endif
}

size_t presetData::copyPartToMemory(ByteIteratorR &bir, part &p){
  size_t _curidx = bir.tellidx();
  R_IFFALSE(bir.setVar(p.start_led));
  R_IFFALSE(bir.setVar(p.end_led));
  R_IFFALSE(bir.setVar(p.range_start));
  R_IFFALSE(bir.setVar(p.range_end));
  R_IFFALSE(bir.setVar(p.reversed));
  R_IFFALSE(bir.setVar(p.channel));

  return _curidx-bir.tellidx();
}

bool presetData::setPartFromMem(ByteIterator &bi, part &p){
  R_IFFALSE(bi.getVar(p.start_led));
  R_IFFALSE(bi.getVar(p.end_led));
  R_IFFALSE(bi.getVar(p.range_start));
  R_IFFALSE(bi.getVar(p.range_end));
  R_IFFALSE(bi.getVar(p.reversed));
  R_IFFALSE(bi.getVar(p.channel));

  return true;
}


presetData PresetData;