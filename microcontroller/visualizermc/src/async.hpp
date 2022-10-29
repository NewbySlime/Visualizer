#ifndef ASYNC_HEADER
#define ASYNC_HEADER

#include "polling.h"
#include "timer.hpp"

#define YieldWhile(eval)\
while(eval)\
  _run_anothertask()

void _run_anothertask();

#endif