#ifndef VISUALIZERLED_HEADER
#define VISUALIZERLED_HEADER

#include "iostream"
#include "preset.hpp"

class visualizer{
  private:
    long _lastMs, _lastMs_sound;
    int _ledlen;
    uint8_t _manyChannel;
    color *_coldata;
    float *_sounddata;
    
    // per channel
    float *_unionSoundData;
    preset *_currentPreset;

    float _colorOffset;
    

  public:
    visualizer(int ledlen, int channellen = 1);
    ~visualizer();

    void update();
    // data should have a length of ledlen * channels
    void updateSound(float *data);
    void setChannelNum(int many);
    const color *getData();
    void usePreset(int idx);
    void useLoadingPreset();
    void useErrorPreset();
};

#endif