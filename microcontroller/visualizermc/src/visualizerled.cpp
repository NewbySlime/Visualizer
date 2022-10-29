#include "visualizerled.hpp"
#include "misc.hpp"

#include "Arduino.h"


preset *_loadingPreset;
preset *_errorPreset;

int __vis_init(){
  // red to black fade
  color cols1[] = {0xff0000, 0x000000};
  float range1[] = {0.0f, 0.75f};

  _errorPreset = new preset{
    .presetName = "",
    .colorMode = preset_colorMode::RGB_p,
    .brightness = 1.0f,
    .speedChange = 0.5f,
    .windowSlide = 1.0f,
    .inUnion = false,
    .colorRange = {cols1, range1, 2}
  };

  _errorPreset->splitParts = std::vector<part>{
    part{
      .start_led = 0,
      .end_led = PRESET_CONST_LEDNUM,
      .range_start = 0.0f,
      .range_end = 1.0f,
      .reversed = false,
      .channel = 0
    }
  };


  // white to black fade
  color cols2[] = {0xffffff, 0x000000, 0xffffff, 0x000000};
  float range2[] = {0.25f, 0.5f, 0.75f, 1.0f};

  _loadingPreset = new preset{
    .presetName = "",
    .colorMode = preset_colorMode::RGB_p,
    .brightness = 1.0f,
    .speedChange = 0.5f,
    .windowSlide = 2.0f,
    .inUnion = false,
    .colorRange = {cols2, range2, 4}
  };

  _loadingPreset->splitParts = std::vector<part>{
    part{
      .start_led = 0,
      .end_led = PRESET_CONST_LEDNUM,
      .range_start = 0.0f,
      .range_end = 1.0f,
      .reversed = false,
      .channel = 0
    }
  };

  return 0;
}

int __dmp = __vis_init();

visualizer::visualizer(int ledlen, int channellen){
  _ledlen = ledlen;
  _manyChannel = channellen;
  _currentPreset = PresetData.getLastUsedPreset();
  _coldata = (color*)malloc(ledlen*sizeof(color));
  _sounddata = (float*)malloc(ledlen*channellen*sizeof(float));
  _unionSoundData = (float*)malloc(channellen*sizeof(float));
}

visualizer::~visualizer(){
  free(_coldata);
  free(_sounddata);
}

void visualizer::update(){
  unsigned long _currMs = millis();
  unsigned long deltaMs = _currMs < _lastMs? _currMs+(0xffffffffU-_lastMs): _currMs-_lastMs;
  _lastMs = _currMs;

  _colorOffset += _currentPreset->speedChange*deltaMs/1000.0f;
  
  float _dmp;
  _colorOffset = modff(_colorOffset, &_dmp);

  for(int i = 0; i < _currentPreset->splitParts.size(); i++){
    auto &split = _currentPreset->splitParts[i];
    // getting color in static color mode
    color static_col = _currentPreset->colorRange.getColor(_currentPreset->ValuePosx);
    // number associated with calculating position in rgb mode
    float _n = (split.range_end - split.range_start)/(float)(split.end_led-split.start_led);

    for(int ledidx = split.start_led; ledidx < split.end_led; ledidx++){
      color _col(0xffffff);
      preset_colorMode __newcolmode = _currentPreset->colorMode;

      // setting the brightness
      _col = _col * _currentPreset->brightness;

      float _intensity = 0.0f;
      if(_currentPreset->colorMode == preset_colorMode::Sound){
        float posrange = (float)(ledidx-split.start_led)*_n+split.range_start;
        _intensity = _sounddata[(int)floor(posrange*_ledlen+split.channel*_ledlen)];

        switch(_currentPreset->brightnessMode){
          // not used
          break; case preset_brightnessMode::Static_b:{

          }

          break; case preset_brightnessMode::Union:{
            float f = (_unionSoundData[split.channel]-_currentPreset->minIntensity)/(_currentPreset->maxIntensity-_currentPreset->minIntensity);

            _col = _col * f;
          }

          break; case preset_brightnessMode::Individual:{
            float f = (_intensity-_currentPreset->minIntensity)/(_currentPreset->maxIntensity-_currentPreset->minIntensity);

            _col = _col * f;
          }
        }

        __newcolmode = _currentPreset->colorShifting;
      }

      switch(__newcolmode){
        break; case preset_colorMode::Static_c:{
          _col = _col * static_col;
        }

        break; case preset_colorMode::RGB_p:{
          float posrange = 
            (_currentPreset->inUnion? 1.0f: (float)(ledidx-split.start_led)*_n)
            *_currentPreset->windowSlide+split.range_start+_colorOffset;

          posrange = modff(posrange, &_dmp);
          _col = _col * _currentPreset->colorRange.getColor(posrange);
        }

        break; case preset_colorMode::Sound:{
          _col = _col * _currentPreset->colorRange.getColor(_intensity);
        }
      }

      _coldata[ledidx] = _col;
    }
  }
}

void visualizer::updateSound(float *data){
  unsigned long _currMs = millis();
  float deltaMs = (float)(_currMs < _lastMs_sound? _currMs+(0xffffffffU-_lastMs_sound): _currMs-_lastMs_sound);
  _lastMs_sound = _currMs;

  for(int ci = 0; ci < _manyChannel; ci++){
    float sumf = 0.0f;
    for(int i = 0; i < _ledlen; i++){
      int idx = ci*_ledlen+i;
      if(_sounddata[idx] > data[idx])
        _sounddata[idx] /= deltaMs/100.0f+1.0f;
      else
        _sounddata[idx] = min(data[idx], _currentPreset->maxIntensity);
      
      sumf = _sounddata[idx];
    }

    _unionSoundData[ci] = sumf/_ledlen;
  }
}

void visualizer::setChannelNum(int many){
  _manyChannel = many;
  _sounddata = (float*)realloc(_sounddata, _ledlen*many*sizeof(float));
}

const color *visualizer::getData(){
  return _coldata;
}

void visualizer::usePreset(int idx){
  if(!_currentPreset)
    delete _currentPreset;

  _currentPreset = PresetData.getPreset(idx);
  if(!_currentPreset){
    importantText("Can't use preset, could be corrupted.");
    useErrorPreset();
  }
}

void visualizer::useLoadingPreset(){
  _currentPreset = _loadingPreset;
}

void visualizer::useErrorPreset(){
  _currentPreset = _errorPreset;
}