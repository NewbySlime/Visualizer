#ifndef POLLING_HEADER
#define POLLING_HEADER

bool polling_addfunc(void(*fn)(void*), void *obj);
bool polling_removefunc(void *obj);

void polling_update();

#endif