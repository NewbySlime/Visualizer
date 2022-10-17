#include "preset.hpp"
#include "color.hpp"
#include "leddriver-master.hpp"


#define R_IFFALSE(eval)\
if(!eval)\
  return false;

#define _preseterrcheck(eval) \
{preset_error err = eval;\
if(err & 0xff00)\
  return err;}


#if defined(PRESET_USE_EEPROM)
void presetData::_loadData(){

}

void presetData::_saveData(int idx){

}

preset_error presetData::_checkStorage(){
  return preset_error::no_error;
}

#elif defined(PRESET_USE_SDCARD)
void presetData::_loadData(){

}

void presetData::_saveData(int idx){

}

preset_error presetData::_checkStorage(){
  
}

#else
#ifndef PRESET_CONST_LEDNUM
#error Please define how many led if using hardcoded preset
#endif

presetData::presetData(){
  _presets = (preset**)malloc(sizeof(preset*)*MAX_PRESET_NUM);
  for(int i = 0; i < MAX_PRESET_NUM; i++)
    _presets[i] = NULL;
  _presetLen = 0;
}
// if nothing is used for storage, it will use user RAM as storage

// since no data to get from, this will use hardcoded preset
void presetData::_loadData(){
  _presetLen = 1;
  _lastPreset = 0;

  // rainbow representation in hex
  color cols[] = {0xff0000, 0xffe900, 0x20ff00, 0x00fff3, 0x0800ff, 0xff00fb};
  float ranges[] = {0.05f, 0.16f, 0.32f, 0.48f, 0.64f, 0.80f};
  colorRanges colrange{cols, ranges, 6};
  
  
  std::vector<part> splits; splits.push_back(part{
    .start_led = 0,
    .end_led = PRESET_CONST_LEDNUM,
    .range_start = 0.0f,
    .range_end = 1.0f,
    .reversed = false,
    .channel = 0
  });

  _presets[0] = new preset{
    .presetName = "Default",
    .colorMode = preset_colorMode::Sound,
    .brightness = 1.0f,
    .ValuePosx = 0.0f,
    .speedChange = 0.1f,
    .windowSlide = 0.1f,
    .inUnion = false,
    .colorShifting = preset_colorMode::RGB_p,
    .brightnessMode = preset_brightnessMode::Individual,
    .maxIntensity = 20.0f,
    .minIntensity = 0.0f,
    .colorRange = colrange,
    .splitParts = splits
  };
}

preset_error presetData::_setData(int idx, preset &p){
  if(idx < 0 || idx >= _presetLen)
    return preset_error::max_preset_exceeded;
  
  preset *pre = _presets[idx];
  if(!pre)
    pre = new preset{}; 
  *pre = p;
  _presets[idx] = pre;

  return preset_error::no_error;
}

preset_error presetData::_resizePreset(uint16_t len){
  if(len <= 0 || len >= MAX_PRESET_NUM)
    return preset_error::max_preset_exceeded;

  // just in case if the not all data can be received
  for(int i = 0; i < len; i++)
    if(_presets[i]){
      delete _presets[i];
      _presets[i] = NULL;
    }

  return preset_error::no_error;
}

preset *presetData::_getData(int idx){
  if(idx >= 0 && idx < _presetLen)
    return _presets[idx];
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

preset_error presetData::updatePreset(uint16_t idx){
  if(idx >= 0 && idx < _presetLen)
    _saveData(idx);
  else
    return preset_error::wrong_index;
  
  return preset_error::no_error;
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

size_t presetData::presetSizeInBytes(preset &p){
  return
    sizeof(preset) +

    // subtract the dynamic storage size
    -sizeof(p.presetName) +
    -sizeof(p.colorRange) +
    -sizeof(p.splitParts) +

    // then add the real size
    MAX_PRESET_NAME_LENGTH +

    // the dynamic storages will have a number in the front of the data, as the length
    sizeof(uint16_t) + (p.colorRange.colors.size() * (sizeof(color)+sizeof(float))) +
    sizeof(uint16_t) + (p.splitParts.size() * sizeof(part))
  ;
}

size_t presetData::copyToMemory(ByteIteratorR &memwrite, preset& p){
  Serial.printf("Calling bir's preset\n");

  if(memwrite.available() < presetSizeInBytes(p))
    return 0;
  
  Serial.printf("presetName, %s\n", p.presetName.c_str());
  R_IFFALSE(memwrite.setVar(p.presetName.c_str(), p.presetName.size()));

  char *dummychar = (char*)calloc(sizeof(char), MAX_PRESET_NAME_LENGTH-p.presetName.size());
  R_IFFALSE(memwrite.setVar(dummychar, MAX_PRESET_NAME_LENGTH-p.presetName.size()));
  free(dummychar);
  
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

  R_IFFALSE(memwrite.setVar((uint16_t)p.colorRange.colors.size()));
  R_IFFALSE(memwrite.setVar(p.colorRange.colors, p.colorRange.colors.size()));
  R_IFFALSE(memwrite.setVar((uint16_t)p.splitParts.size()));
  for(int i = 0; i < p.splitParts.size(); i++)
    R_IFFALSE(presetData::copyPartToMemory(memwrite, p.splitParts[i]));

  return memwrite.tellidx();
}

bool __cmp1(const part &p1, const part &p2){
  return p1.start_led < p2.start_led;
}

bool presetData::setPresetFromMem(ByteIterator &bi, preset &p){
  if(bi.available() < MAX_PRESET_NAME_LENGTH)
    return false;
  
  char _tmp[MAX_PRESET_NAME_LENGTH+1];
  if(bi.getVar(_tmp, MAX_PRESET_NAME_LENGTH) != MAX_PRESET_NAME_LENGTH)
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
  p.colorRange.colors.reserve(colorlen);
  for(int i = 0; i < colorlen; i++){
    float range{}; R_IFFALSE(bi.getVar(range));
    color col{}; R_IFFALSE(bi.getVar(col));

    p.colorRange.addColor(col, range);
  }
  

  // getting split parts data
  p.splitParts.clear();
  uint16_t partlen; R_IFFALSE(bi.getVar(partlen));
  p.splitParts.resize(partlen);
  for(int i = 0; i < partlen; i++)
    R_IFFALSE(presetData::setPartFromMem(bi, p.splitParts[i]));

  // just in case
  std::sort(p.splitParts.begin(), p.splitParts.end(), __cmp1);

  return true;
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