#include "time_rand.hpp"
#include "stdlib.h"

#include "Arduino.h"

#define DEFAULT_RANDOMBUFFER 2

time_rand::time_rand(){
  rbufferlen = DEFAULT_RANDOMBUFFER;
  rbuffer = (int*)malloc(sizeof(int)*rbufferlen);

  for(int i = 0; i < rbufferlen; i++)
    rbuffer[i] = rand();
}

std::pair<const int*, int> time_rand::getRandomBuffer(){
  return {rbuffer, rbufferlen};
}

void time_rand::supplyTime(unsigned long ct){
  srand((unsigned int)(ct & 0xffffffff));
  
  for(int i = rbufferlen-2; i >= 0; i--)
    rbuffer[i+1] = rbuffer[i];
  
  rbuffer[0] = rand();
}

bool time_rand::setBufferLength(int n){
  if(n > MAX_RANDOMBUFFER || n == 0)
    return false;
  
  if(n <= rbufferlen)
    return true;
  
  rbufferlen = n;
  rbuffer = (int*)realloc(rbuffer, sizeof(int) * rbufferlen);
  return true;
}

time_rand TimeRandom;