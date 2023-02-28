#include "visualizer.h"
#include "peripheral.h"

#include "RGBLED_struct.h"
#include "math.h"


static const color black_col = (color){.r = 0, .g = 0, .b = 0};
void visualizer_process(visualizer_data *data){
  const color static_col = color_ranges_get(data->preset.color_range, data->preset.preset_data->value_posx);
  color rgb_col;
  color *_colarr = (color*)data->color_data;
  preset_data *pd = data->preset.preset_data;

  for(uint16_t is = 0; is < data->preset.split_parts_count; is++){
    preset_parts *_pp = &data->preset.split_parts[is];
    float _uvxoffset = pd->speed_change * data->current_time * (pd->in_union? 1: pd->window_color);
    for(uint16_t ip = _pp->led_start; ip < _pp->led_end && ip < data->led_count; ip++){
      switch(pd->colormode){
        break; case preset_colmode_static:
          _colarr[ip] = static_col;
          continue;

        break; case preset_colmode__stop:
          _colarr[ip] = black_col;
          continue;
      }
      
      // TODO: use range_end and range_start
      float _uvx = ((0.5f+(pd->in_union? _pp->led_start: ip))-_pp->led_start)/(_pp->led_end-_pp->led_start);
      float _uvxoffset_rgb = _uvx * _uvxoffset;
      if(_pp->reversed)
        _uvx = 1.0f - _uvx;

      color _end_col;
      if(pd->colormode == preset_colmode_rgb || (pd->colormode == preset_colmode_sound && pd->colorshift == preset_colmode_rgb))
        rgb_col = color_ranges_get(data->preset.color_range, _uvxoffset_rgb);

      switch(pd->colormode){
        break; case preset_colmode_rgb:{
          _end_col = rgb_col;
        }

        break; case preset_colmode_sound:{
          switch(pd->colorshift){
            break; case preset_colorshift_static:{
              _end_col = rgb_col;
            }

            break; case preset_colorshift_union:{
              _end_col = color_mult(rgb_col, data->sound_data->avg_wvf);
            }

            break; case preset_colorshift_individual:{
              float _uvxoffset_sound;
              _end_col = color_mult(rgb_col, data->sound_data->wvf_data[(uint16_t)floorf(_uvxoffset_sound)]);
            }
          }
        }
      }
    }
  }
}


static void _visualizer_event(peripheral_event event, peripheral_info *pinfo){
  visualizer_data *vdata = (visualizer_data*)pinfo->function_param;
  RGBLED_datastruct *led_data = (RGBLED_datastruct*)pinfo->additional_param;
  switch(event & __pe_flags){
    break; case pe_donecommand:{
      switch(event & __pe_data){
        break; case pc_write:{
          // in bytes
          uint32_t _currentidx = ((uint32_t)led_data->colarr.colors-(uint32_t)vdata->color_data);
          if(_currentidx >= vdata->color_datasize){
            pinfo->command = pc_stop;
            pinfo->peripheralcb_command(pinfo);

            break;
          }

          // in color
          uint32_t _sendlen = (vdata->color_datasize-_currentidx)/sizeof(color);
          _sendlen = _sendlen > led_data->max_recv? led_data->max_recv: _sendlen;

          led_data->colarr.count = _sendlen;
          led_data->colarr.colors = &((color*)vdata->color_data)[_sendlen];

          pinfo->command = pc_async | pc_write;
          pinfo->peripheralcb_command(pinfo);
        }
      }
    }
  }
}

bool visualizer_draw(visualizer_data *data, peripheral_info *io_info){
  io_info->command = pc_start;
  if(io_info->peripheralcb_command(io_info) != pr_ok)
    return false;
  
  RGBLED_datastruct *led_data = (RGBLED_datastruct*)io_info->additional_param;
  led_data->colarr.colors = (color*)data->color_data;

  io_info->function_param = data;
  io_info->peripheralcb_onevent = _visualizer_event;
  _visualizer_event(pe_donecommand | pc_write, io_info);
  return true;
}