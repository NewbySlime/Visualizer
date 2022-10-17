#ifndef COLOR_HEADER
#define COLOR_HEADER

#include "Arduino.h"

class color{
  public:
    uint8_t r,g,b;

    color();
    color(uint8_t r, uint8_t g, uint8_t b);
    color(float r, float g, float b);
    color(uint32_t hexnum);

    // val has to be range between 1.f - 0.f
    static color mixLinear(color col1, color col2, float range);
    color setBrightness(float val);

    color operator+(color col);
    color operator-(color col);
    color operator*(color col);
    color operator*(float f);
};

// just to make the compiler happy
bool operator<(const color &col, const color &col1);

class colorRanges{
  private:
    static bool _compareRange(const std::pair<float, color>&, const std::pair<float, color>&);

  public:
    std::vector<std::pair<float, color>> colors;

    colorRanges();
    colorRanges(color *cols, float *ranges, int length);

    // float should be a range of 1.f - 0.f
    void addColor(color &col, float range);

    // float should be a range of 1.f - 0.f
    color getColor(float range);
};

#endif