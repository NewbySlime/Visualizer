#include "color.hpp"
#include "math.h"



/*      color class function      */

color::color(){
  r = g = b = 0;
}

color::color(uint8_t r, uint8_t g, uint8_t b){
  this->r = r;
  this->g = g;
  this->b = b;
}

color::color(float r, float g, float b){
  this->r = (uint8_t)round(r*0xff);
  this->g = (uint8_t)round(g*0xff);
  this->b = (uint8_t)round(b*0xff);
}

color::color(uint32_t hexnum){
  this->r = (hexnum >> 16) & 0xff;
  this->g = (hexnum >> 8) & 0xff;
  this->b = hexnum & 0xff;
}

color color::setBrightness(float val){
  return *this * val;
}

color color::mixLinear(color col1, color col2, float range){
  int _r = range*((int)col2.r-col1.r)+col1.r;
  int _g = range*((int)col2.g-col1.g)+col1.g;
  int _b = range*((int)col2.b-col1.b)+col1.b;

  return color((uint8_t)_r, (uint8_t)_g, (uint8_t)_b);
}

bool operator<(const color &col, const color &col1){
  return true;
}

color color::operator+(color col){
  return color{(uint8_t)(r+col.r), (uint8_t)(g+col.g), (uint8_t)(b+col.b)};
}

color color::operator-(color col){
  return color{(uint8_t)(r-col.r), (uint8_t)(g-col.g), (uint8_t)(b-col.b)};
}

color color::operator*(color col){
  return color{(uint8_t)(r*col.r/0xff), (uint8_t)(g*col.g/0xff), (uint8_t)(b*col.b/0xff)};
}

color color::operator*(float f){
  return color{(uint8_t)round(f*r), (uint8_t)round(f*g), (uint8_t)round(f*b)};
}




/*      colorRanges class function      */

bool colorRanges::_compareRange(const std::pair<float, color> &r1, const std::pair<float, color> &r2){
  return r1.first < r2.first;
}

colorRanges::colorRanges(){

}

colorRanges::colorRanges(color *cols, float *ranges, int length){
  colors.resize(length);
  for(int i = 0; i < length; i++)
    colors[i] = std::pair<float, color>(ranges[i], cols[i]);
  
  std::sort(colors.begin(), colors.end(), _compareRange);
}

void colorRanges::addColor(color &col, float range){
  auto p = std::pair<float, color>(range, col);
  colors.insert(
    std::lower_bound(
      colors.begin(),
      colors.end(),
      p
    ),
    p
  );
}

color colorRanges::getColor(float range){
  auto col = std::lower_bound(colors.begin(), colors.end(), std::pair<float, color>(range, color()), _compareRange);


  //the range is below the first color
  std::pair<float, color> prevcol = *(col-1);
  float _range = col->first, _prevrange = prevcol.first;
  if(col == colors.begin()){
    if(range < col->first){
      prevcol = *(colors.end()-1);
      _prevrange = prevcol.first-1.0f;
    }
    else{
      prevcol = *col;
      col++;

      _prevrange = prevcol.first;
    }

    _range = col->first;
  }
  else if(col == colors.end()){
    prevcol = *(col-1);
    col = colors.begin();

    _range = 1.0f+col->first;
    _prevrange = prevcol.first;
  }
  
  return color::mixLinear(prevcol.second, col->second, (range-_prevrange)/(_range-_prevrange));
}