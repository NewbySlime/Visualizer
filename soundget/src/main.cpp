#include "socket.hpp"
#include "soundget.hpp"
#include "initquit.hpp"
#include "conio.h"
#include "map"
#include "exception"

const unsigned short
  VISDATA = 0x0000,
  STRDATA = 0x0001,
  TEST = 0x0002;

const unsigned short port = 3022;


struct Socket_MessageCallbacks{
  public:
    SocketHandler_Sync *s_socket;

    // <code, <callback, callback's param count>>
    std::map<unsigned short, std::pair<void (*)(char**, int*, int), int>> callbacks;
};

void checkSocketMessage(Socket_MessageCallbacks *smc){
  smc->s_socket->PingSocket();
  
  bool waitForHeader = true;
  auto cbIter = smc->callbacks.end();
  int messageParamCount = 0;
  int paramCount = 0;
  int *paramLengths = (int*)malloc(0);
  char **paramContainer = (char**)malloc(0);
  while(smc->s_socket->IsAnotherMessage()){
    auto pairMsg = smc->s_socket->GetNextMessage();
    if(waitForHeader){
      if(pairMsg.second != 2){
        printf("Message code data length isn't correct (%d as supposed to be 2).\n", pairMsg.second);
        continue;
      }

      unsigned short msgCode = *reinterpret_cast<unsigned short*>(pairMsg.first);
      printf("Code get: %X\n", msgCode);
      cbIter = smc->callbacks.find(msgCode);
      if(cbIter != smc->callbacks.end()){
        printf("Code get, %d\n", msgCode);
        paramCount = cbIter->second.second;
        paramContainer = (char**)realloc(paramContainer, sizeof(char**)*paramCount);
        paramLengths = (int*)realloc(paramLengths, sizeof(int)*paramCount);
        messageParamCount = 0;
        waitForHeader = false;
      }
      else{
        printf("Listener sent wrong message code of 0x%X", msgCode);
      }
    }
    else{
      bool clearParamContainer = smc->s_socket->IsAnotherMessage()? false: true;

      paramContainer[messageParamCount] = pairMsg.first;
      paramLengths[messageParamCount] = pairMsg.second;
      messageParamCount++;

      if(messageParamCount >= paramCount){
        cbIter->second.first(paramContainer, paramLengths, paramCount);
        clearParamContainer = true;
      }

      if(clearParamContainer)
        for(int i = 0; i < messageParamCount; i++)
          free(paramContainer[i]);
    }
  }

  free(paramLengths);
  free(paramContainer);
}


// int
void testget(char **buffer, int* bufLengths, int length){
  if(length >= 1){
    int msgint = bufLengths[0] == sizeof(int)? *reinterpret_cast<int*>(buffer[0]): 0;
    printf("msgint: %d\n", msgint);
  }
}



SocketHandler_Sync *progSock;


void initProgram(){
  CoInitializeEx(NULL, COINIT_MULTITHREADED);
}
_ProgInitFunc _pifp(initProgram);


void closeProgram(){
  CoUninitialize();
}
_ProgQuitFunc _pqfp(closeProgram);


void SendFreqDataToAnotherProg(const float* amp, int length, const WAVEFORMATEX *wavform){
  LockSocket(progSock);
  progSock->SendData(&VISDATA, sizeof(VISDATA));
  progSock->SendData(&length, sizeof(length));
  progSock->SendData(amp, sizeof(float) * length);
}

void SendFreqDataToMC(const float* amp, int length, const WAVEFORMATEX *wavform){

}


#define TEST_PROG

#ifdef TEST_PROG
unsigned short DO_TEST = 0x3;
unsigned short DO_MTPLY = 0x4;

SocketHandlerUDP_Async sockudp;

void callbackOnUdpmsg(const char* msg, int msglen){
  printf("Message incoming: %s\n", msg);
}
#endif


// TODO add a data iterator for getting data out of raw variables
// FIXME i might did a segfault, since it randomly gets an error
// another hint is it sometimes gets an error in the middle of testing
// and somehow it sends another message despite the program is quitting
// the segfault occurred on the listening thread
// FIXME somehow the code from microcontroller isn't right, it might be because i code it wrong for this
int main(){
#ifdef TEST_PROG
  _onInitProg();

  srand(time(NULL));
  
  printf("test\n");
  sockudp.SetCallback(callbackOnUdpmsg);
  if(sockudp.StartConnecting("192.168.76.227", 3020)){
    printf("Sending message\n");
    // TODO somehow this makes the microcontroller doo doo
    unsigned short *testcode = new unsigned short(DO_TEST);
    sockudp.QueueMessage(reinterpret_cast<char*>(testcode), sizeof(unsigned short));
  }
  else
    printf("Cannot connect to remote host.\n");


  std::this_thread::sleep_for(std::chrono::seconds(20));
  printf("done testing.\n");
  sockudp.Disconnect();
  sockudp.WaitUntilDisconnect();

  _onQuitProg();
#else
  _onInitProg();

  progSock = new SocketHandler_Sync(port);

  Socket_MessageCallbacks *progSock_cbs = new Socket_MessageCallbacks();
  progSock_cbs->s_socket = progSock;
  progSock_cbs->callbacks = {
    {TEST, {testget, 1}}
  };

  progSock->ReconnectSocket();
  //progSock->SendData(&TEST, sizeof(TEST));

  //checkSocketMessage(progSock_cbs);
  
  SoundGet sg;
  sg.CallbackListenerRaw = SendFreqDataToAnotherProg;
  sg.CallbackListener = SendFreqDataToMC;
  auto devices = sg.GetDevices();
  for(int i = 0; i < devices->size(); i++){
    printf("%s\n", devices->operator[](i).deviceName.c_str());
  }

  sg.SetDevice(1);
  sg.StartListening(30);

  /*
  bool _keepLooping = true;
  while(_keepLooping){
    char ch = _getch();
    switch(ch){
      break; case 'q':{
        _keepLooping = false;
      }
    }
  }
  */

  delete progSock_cbs;
  progSock->SafeDelete();

  _onQuitProg();
#endif
}