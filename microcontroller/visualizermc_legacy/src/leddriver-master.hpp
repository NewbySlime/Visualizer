#ifndef LEDDRIVER_M_HEADER
#define LEDDRIVER_M_HEADER

#include "color.hpp"

#define LEDDRIVER_BAUDRATE 160000
#define LEDDRIVER_MAXSEND 254

void led_init(uint16_t leds);

// col length has to be LEDDRIVER_LEDLEN
void led_sendcols(const color *col);


#endif