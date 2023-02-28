#ifndef VISUALIZER_HEADER
#define VISUALIZER_HEADER

#include "stdint.h"
#include "preset_data.h"

#include "peripheral.h"


typedef struct visualizer_data{
  uint32_t led_count;
  uint32_t _iter;

  float current_time; // in seconds

  preset preset;
  waveform_data *sound_data;

  uint32_t color_datasize;    // in bytes
  char *color_data;  // make sure the size is 3 bytes (RGB) * many colors
} visualizer_data;


/**
 * The function process the data contained in the visualizer_data._sound_data
 * with preset data to take account in order to process it to color form. The
 * function will process until _color_data is filled or _iter is the same as
 * _led_count (it means all LED's color has been processed)
 * 
 * Each frame, if this is the first time calling make sure to set all the
 * variables in visualizer_data
 * 
 * Do not change the variables when still processing a frame
 */
void visualizer_process(visualizer_data *data);


/**
 * Draws the color data contained in visualizer_data to a LED strip or a display
 * 
 * The function only transfer data to the display, not processing it. Call visualizer_process
 * to get data of colors from sound data
 * 
 * Returns true if the data is sent, false if not sent
 */
bool visualizer_draw(visualizer_data *data, peripheral_info *io_info);

#endif