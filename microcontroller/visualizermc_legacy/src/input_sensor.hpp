#ifndef INPUT_SENSOR_HEADER
#define INPUT_SENSOR_HEADER

// since enum has the same type of 32-bit integer
// it will use another 16-bit int as a regular number but still related to the enum
enum sensor_actType{
  pressed,
  released,
  click,
  double_click,
  slide_left,
  slide_right,

  ui_accept,
  ui_backing,
  ui_left,
  ui_right
};

#endif