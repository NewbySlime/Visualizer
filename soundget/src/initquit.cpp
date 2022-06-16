#include "initquit.hpp"
#include "vector"
#include "csignal"
#include "cstdlib"
#include "stdio.h"

std::vector<void (*)()> *cbsOnInit, *cbsOnQuit;
bool __firstInitF = true;
bool __firstQuitF = true;

_ProgInitFunc::_ProgInitFunc(void (*cb)()){
  if(__firstInitF){
    __firstInitF = false;
    cbsOnInit = new std::vector<void (*)()>();
  }
  
  cbsOnInit->insert(cbsOnInit->end(), cb);
}

_ProgQuitFunc::_ProgQuitFunc(void (*cb)()){
  if(__firstQuitF){
    __firstQuitF = false;
    cbsOnQuit = new std::vector<void (*)()>();
  }

  cbsOnQuit->insert(cbsOnQuit->end(), cb);
}

void __onProgTerminated(int sig){
  _onQuitProg();
  
  switch(sig){
    break; case SIGINT:
      printf("Program terminated due to interrupted by user.\n");

    break; case SIGTERM:
      printf("Program terminated due to system terminated.\n");
    
    break; case SIGSEGV:
      printf("Program terminated due to segmentation fault occured.\n");
  }
  
  std::exit(sig);
}

void _onInitProg(){
  if(!__firstInitF){
    for(int i = 0; i < cbsOnInit->size(); i++)
      cbsOnInit->operator[](i)();
  }
  
  std::signal(SIGINT, __onProgTerminated);
  std::signal(SIGTERM, __onProgTerminated);
  std::signal(SIGSEGV, __onProgTerminated);
}

void _onQuitProg(){
  if(!__firstQuitF){
    __firstQuitF = true;
    for(int i = 0; i < cbsOnQuit->size(); i++)
      cbsOnQuit->operator[](i)();
    
    delete cbsOnQuit;
  }

  if(!__firstInitF){
    __firstInitF = true;
    delete cbsOnInit;
  }
}