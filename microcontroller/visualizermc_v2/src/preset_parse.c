#include "preset_data.h"
#include "defines.h"


bool preset_parsefrombuffer(preset *pdata, void *buffer){
  uint32_t _memaddr = (uint32_t)buffer;

  pdata->preset_data = (preset_data*)_memaddr; _memaddr += sizeof(preset_data);
  pdata->color_range.count = *((uint16_t*)_memaddr); _memaddr += sizeof(uint16_t);
  if(pdata->color_range.count > MAX_COLOR_RANGE_COUNT || pdata->color_range.count == 0)
    return false;

  pdata->color_range.colors = (color*)_memaddr; _memaddr += pdata->color_range.count * sizeof(color);
  pdata->split_parts_count = *((uint16_t*)_memaddr); _memaddr += sizeof(uint16_t);
  if(pdata->split_parts_count > MAX_SPLIT_PARTS_COUNT || pdata->split_parts_count == 0)
    return false;

  pdata->split_parts = (preset_parts*)_memaddr;

  return true;
}