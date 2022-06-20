#include "SPI.h"
#include <Arduino.h>
#include "timer.hpp"


class test{
  private:
    int num = 0;
    int _timerid = 0;

  public:
    test(int n){
      num = n;
      _timerid = timer_setInterval(1000, _cb, this);
    }

    static void _cb(void* obj){
      ((test*)obj)->cb();
    }

    void cb(){
      Serial.printf("Callback called, with num %d\n", num);

      timer_changeInterval(_timerid, 100*num);
      if(--num <= 0){
        timer_deleteTimer(_timerid);
      }
    }
};

test *t;

void setup(){
  Serial.begin(9600);
  t = new test(10);
}

void loop(){
  timer_update();
}