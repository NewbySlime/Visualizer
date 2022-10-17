#ifndef PRESET_HEADER
#define PRESET_HEADER

#include "color.hpp"
#include "ByteIterator.hpp"
#include "string"

#if defined(PRESET_USE_EEPROM) || defined(PRESET_USE_SDCARD)
  #define MAX_PRESET_NUM 16
#else
  #define MAX_PRESET_NUM 4
#endif

#define MAX_PRESET_NAME_LENGTH 16
#define MAX_PRESET_COLOR 32
#define MAX_PRESET_PART 8



enum preset_error{
  no_error = 255,
  max_preset_exceeded,
  max_name_exceeded,
  wrong_index,
  data_insufficient
};

enum preset_colorMode{
  Static_c,
  RGB_p,
  Sound
};

enum preset_brightnessMode{
  Static_b,
  Union,
  Individual
};

struct part{
  public:
    int start_led, end_led;
    float range_start, range_end;
    bool reversed;
    int channel;
};

struct preset{
  public:
    // general data
    std::string presetName; // static storage of 16 bytes
    preset_colorMode colorMode;
    float brightness;

    // static
    float ValuePosx;

    // RGB
    float speedChange, windowSlide;
    bool inUnion;

    // Sound
    preset_colorMode colorShifting;
    preset_brightnessMode brightnessMode;
    float maxIntensity, minIntensity;

    colorRanges colorRange; // dynamic storage
    std::vector<part> splitParts; // dynamic storage
};

class presetData{
  private:
    int _lastPreset;

    #if !defined(PRESET_USE_EEPROM) && !defined(PRESET_USE_SDCARD)
    preset **_presets;
    int _presetLen;
    #endif

    preset_error _checkStorage();
    void _loadData();
    preset_error _saveData(int idx);
    preset_error _setData(int idx, preset &p);
    preset_error _resizePreset(uint16_t len);
    preset *_getData(int idx);

  public:
    presetData();

    preset_error initPreset();
    preset_error setPreset(uint16_t idx, preset &p);
    preset_error updatePreset(uint16_t idx);

    // this will delete all preset data
    preset_error resizePreset(uint16_t len);

    // this will check the datas, indicating that the data no longer can be received
    void checkPreset();
    int manyPreset();
    preset *getPreset(uint16_t idx);
    preset *getLastUsedPreset();
    int getLastUsedPresetIdx();

    static size_t copyToMemory(ByteIteratorR &memwrite, preset &data);
    static size_t copyToMemory(char *buffer, size_t datalen, preset &data){
      ByteIteratorR _bir{buffer, datalen};
      return copyToMemory(_bir, data);
    }

    size_t copyToMemory(ByteIteratorR &memwrite, uint16_t idx){
      preset *data = getPreset(idx);
      if(!data)
        return 0;

      return copyToMemory(memwrite, *getPreset(idx));
    }

    size_t copyToMemory(char *buffer, size_t datalen, uint16_t idx){
      preset *data = getPreset(idx);
      if(!data)
        return 0;
      
      return copyToMemory(buffer, datalen, *getPreset(idx));
    }

    static bool setPresetFromMem(ByteIterator &bi, preset &p);
    static bool setPresetFromMem(const char *buffer, size_t datasize, preset& p){
      ByteIterator _bi{buffer, datasize};
      return setPresetFromMem(_bi, p);
    }

    static size_t presetSizeInBytes(preset &p);
    size_t presetSizeInBytes(int idx){
      preset *data = getPreset(idx);
      if(!data)
        return 0;

      return presetSizeInBytes(*getPreset(idx));
    }

    static size_t copyPartToMemory(ByteIteratorR &bir, part &p);
    static size_t copyPartToMemory(char *buffer, size_t datasize, part &p){
      ByteIteratorR _bir{buffer, datasize};
      return copyPartToMemory(_bir, p);
    }

    static bool setPartFromMem(ByteIterator &bi, part &p);
    static bool setPartFromMem(const char *buffer, size_t datasize, part &p){
      ByteIterator _bi{buffer, datasize};
      return setPartFromMem(_bi, p);
    }
};

extern presetData PresetData;

#endif