#ifndef INITQUITHEADER
#define INITQUITHEADER

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