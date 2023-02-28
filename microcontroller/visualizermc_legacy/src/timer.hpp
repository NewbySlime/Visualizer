#ifndef TIMER_HEADER
#define TIMER_HEADER


typedef void (*TimerCallbackArg)(void *obj);

int timer_setTimer(unsigned long delay, TimerCallbackArg cbarg, void *arg, bool oneshot);
int timer_setInterval(unsigned long delay, TimerCallbackArg cbarg, void *arg);
int timer_setTimeout(unsigned long delay, TimerCallbackArg cbarg, void *arg);
bool timer_changeInterval(unsigned int timer_id, unsigned long delay);
void timer_deleteTimer(unsigned int timer_id);

void timer_update();

#endif