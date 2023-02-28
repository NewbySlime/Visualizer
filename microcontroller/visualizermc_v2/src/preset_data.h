#ifndef PRESETDATA_HEADER
#define PRESETDATA_HEADER

#include "stdint.h"
#include "stdbool.h"
#include "color.h"


typedef enum preset_colormode{
  preset_colmode_static,
  preset_colmode_rgb,
  preset_colmode_sound,
  preset_colmode__stop
} preset_colormode;

typedef enum preset_colorshift{
  preset_colorshift_static,
  preset_colorshift_union,
  preset_colorshift_individual
} preset_colorshift;


typedef struct preset_parts{
  uint32_t led_start, led_end;
  float range_start, range_end;
  bool reversed;
  uint16_t channel;
} preset_parts;

typedef struct preset_data{
  // general data
  char presetname[16];    // static storage of 16 bytes
  uint8_t colormode;
  float brightness;

  // static
  float value_posx;

  // RGB
  float speed_change, window_color;
  bool in_union;  // if this is true, window_color will be ignored

  // Sound
  uint8_t colorshift;
  uint8_t brightnessmode;
  float intensity_max, intensity_min;
} preset_data;


typedef struct preset{
  preset_data *preset_data;
  color_ranges color_range;
  uint16_t split_parts_count;
  preset_parts *split_parts;
} preset;


/**
 * Parsing from buffer that contains data about the preset to a struct
 * 
 * NOTE: the struct only contains the pointer to certain position in the buffer
 *  so do not free the buffer
 */
bool preset_parsefrombuffer(preset *pdata, void *buffer);


#endif