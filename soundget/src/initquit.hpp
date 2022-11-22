#ifndef INITQUITHEADER
#define INITQUITHEADER


#define PROGINITFUNC(func) _ProgInitFunc _pi_##func(func)
#define PROGQUITFUNC(func) _ProgQuitFunc _pq_##func(func)

class _ProgInitFunc{
  public:
    _ProgInitFunc(void (*cbWhenInit)());
};

class _ProgQuitFunc{
  public:
    _ProgQuitFunc(void (*cbWhenQuit)());
};

void _onInitProg();
void _onQuitProg();

#endif