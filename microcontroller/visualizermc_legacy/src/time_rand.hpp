#ifndef TIME_RAND_HEADER
#define TIME_RAND_HEADER

#include "time.h"
#include "utility"

#define MAX_RANDOMBUFFER 32

class time_rand{
  private:
    int *rbuffer;
    int rbufferlen;

  public:
    time_rand();

    std::pair<const int*, int> getRandomBuffer();

    // this should be supplied whenever there's input from user
    // and will discard the last random in the buffer
    void supplyTime(unsigned long currentTime);

    // if n is bigger than MAX_RANDOMBUFFER or is equal to zero, this function will return false and thus not changing the buffer
    // if n is smaller than previous buffer len, then this function will not change the buffer, but will return true
    bool setBufferLength(int n);
};

extern time_rand TimeRandom;

#endif