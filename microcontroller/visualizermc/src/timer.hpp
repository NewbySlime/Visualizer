#ifndef TIMER_HEADER
#define TIMER_HEADER


typedef void (*TimerCallbackArg)(void *obj);

int timer_setInterval(unsigned long delay, TimerCallbackArg cbarg, void *arg);
bool timer_changeInterval(unsigned int timer_id, unsigned long delay);
void timer_deleteTimer(unsigned int timer_id);

// only run it in outside of interrupt context
void timer_update();

#endif