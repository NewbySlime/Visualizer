#ifndef ANIMATION_HEADER
#define ANIMATION_HEADER

#include "Arduino.h"
#include "vector"

// 1 dimensional function of bezier curve
// fdata is an array of control points (0.0 ... 1.0 (can be lower than minimum or higher than maximum))
// while val is a value between 0.0 to 1.0 for getting function result
float bezier_func1D(const float *fdata, size_t fdatalen, float val);

class AnimationData{
  public:
    enum AnimationType{
      Linear,
      EaseIn,
      EaseIn2,
      EaseOut
    };

    static std::pair<const float*, uint16_t> get_animdata(AnimationType animtype);
};

#endif