#ifndef POLLING_HEADER
#define POLLING_HEADER


typedef void (*PollingCallbackArg)(void *obj);

bool polling_addfunc(PollingCallbackArg cb, void *obj);
bool polling_removefunc(PollingCallbackArg cb, void *obj);

void polling_update();

#endif