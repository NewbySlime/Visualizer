#ifndef DEBUG_HEADER
#define DEBUG_HEADER

#include "HardwareSerial.h"


#define VA_ARGS(...) , ##__VA_ARGS__

#ifdef DO_DEBUG
  #define DEBUG_PRINT(str, ...) Serial.printf(str  VA_ARGS(__VA_ARGS__))
#else
  #define DEBUG_PRINT(str, ...) 
#endif

#endif