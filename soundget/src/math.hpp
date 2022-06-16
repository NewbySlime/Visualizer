#ifndef MATHHEADER
#define MATHHEADER

#include "math.h"

#define PI_NUM 22.f/7

template<typename _num> class vec2{
  public:
    _num x, y;

    vec2(_num x = 0, _num y = 0){
      this->x = x;
      this->y = y;
    }

    vec2<_num> operator+(const vec2<_num> &v){
      return vec2<_num>(this->x + v.x, this->y + v.y);
    }

    void operator+=(const vec2<_num> &v){
      this->x += v.x;
      this->y += v.y;
    }

    vec2<_num> operator-(const vec2<_num> &v){
      return vec2<_num>(this->x - v.x, this->y - v.y);
    }

    void operator-=(const vec2<_num> &v){
      this->x -= v.x;
      this->y -= v.y;
    }

    vec2<_num> operator*(float f){
      return vec2<_num>(this->x * f, this->y * f);
    }

    void operator*=(float f){
      this->x *= f;
      this->y *= f;
    }

    double magnitude(){
      return pow(pow(this->x, 2) + pow(this->y, 2), 0.5f);
    }
};

typedef vec2<float> vec2f;
typedef vec2<double> vec2d;

// return a 2D vector with x as the real number and y as the imaginary number
vec2d eulerFunction(int k, int n, int manySamples){
  double part1 = -((double)2*PI_NUM*k*n)/manySamples;
  return vec2d(cos(part1), sin(part1));
}

// return a 2D vector with x as the real number and y as the imaginary number
vec2d DFT_partFunction(int k, int n, int manySamples, float Xn){
  auto vres = eulerFunction(k, n, manySamples);
  return vres * Xn;
}

float f_max(float *arr, int from, int to, int incr){
  float res = arr[from];
  for(int i = from+1; i < to; i += incr){
    if(res < arr[i])
      res = arr[i];
  }

  return res;
}

#endif