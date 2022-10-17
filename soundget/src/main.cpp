#include "socket.hpp"
#include "soundget.hpp"
#include "initquit.hpp"
#include "ByteIterator.hpp"
#include "conio.h"
#include "map"
#include "exception"

#include "windowsx.h"
#include "libloaderapi.h"
#include "commctrl.h"
#include "strsafe.h"


/*      CONSTANT AND DEFINES      */
#define SENDFLAG_VIS 0x1
#define SENDFLAG_MC 0x2

#define CHECKFLAG(num, flag) (num & flag) == flag
#define CHECKDATALEN(num, size) if(num != size) return

#define SOCKET_MSGPOLL 10

#define MAIN_WINDOWCLASS L"viswndclass"

#define IDM_QUIT 0


// codes for controlling this program
const unsigned short
  // stop this soundget program
  STOP_PROG = 0x0001,
  // temporarily stop soundget
  STOP_SOUNDGET = 0x0002,
  START_SOUNDGET = 0x0003,
  // sending sound data (both mcu and tool)
  // only used when it doesn't use the sound color type
  SOUNDDATA_DATA = 0x0010,
  // stop sending the data (from and only mcu)
  SOUNDDATA_STOPSENDING = 0x0011,
  // start sending the data (from and only mcu)
  SOUNDDATA_STARTSENDING = 0x0012,
  // forwarding to mcu / to the tool
  FORWARD = 0x0020,
  // configuring the soundget system
  SET_CONFIG_SG = 0x0021,
  GET_CONFIG_SG = 0x0022,
  // vis tool -> soundget prog
  //    asking for state, or asking to reconnect or disconnect
  // soundget prog -> vis tool
  //    telling the vis tool current connection state of mcu
  CONNECT_MCU_STATE = 0x0030,
  // setting mcu ip address and port
  SETMCU_ADDRESS = 0x0031,
  GETMCU_ADDRESS = 0x0032
;


// codes for soundget configuration
const unsigned short
  // [SET] the input device (index based)
  CFG_SG_INPUTDEVICE = 0x0001,
  // [GET] asking and send input device name
  CFG_SG_INPUTDEVICESNAME = 0x0002,
  // [GET] asking and send current or specified input device data (index, name, channels, and such) (mcu can use this)
  //    if the index specified is -1, then it will send the current device data
  //    if the index is above or same as 0, then it will send a device data based on the index
  CFG_SG_INPUTDEVICEDATA = 0x0003,
  // [GET] asking and send how many input device this computer has
  CFG_SG_MANYINPUTDEV = 0x0004,
  // [SET] the sendrate (mcu can use this)
  // [GET]
  CFG_SG_SENDRATE = 0x0005,
  // [SET] the delay of the sound
  CFG_SG_DELAYMS = 0x0006,
  // [GET] asking and send the numbers of channels
  CFG_SG_CHANNELS = 0x0007,
  // [SET] many band to send to mcu (only for mcu)
  // [GET] (only for tool)
  CFG_SG_MCUBAND = 0x0021
;


// codes for connecting state
const unsigned char
  MCUCONNSTATE_CONNECTED = 0x01,
  MCUCONNSTATE_DISCONNECTED = 0x02,
  MCUCONNSTATE_RECONNECT = 0x03,
  MCUCONNSTATE_REQUESTSTATE = 0x04
;

// port for connecting to tool or mcu
const unsigned short port = 3022;
const unsigned short mcuport = 3020;

const auto time_sleepsockthread = std::chrono::milliseconds(1000/SOCKET_MSGPOLL);


struct Socket_MessageCallbacks{
  public:
    SocketHandler_Sync *s_socket;

    // <code, <callback, callback's param count>>
    std::map<unsigned short, std::pair<void (*)(char**, int*), int>> callbacks;
};


/*      GLOBAL VARIABLES      */
HINSTANCE hInst;
HWND mWnd;
NOTIFYICONDATAA visIconData;

unsigned short DO_TEST = 0x3;
unsigned short DO_MTPLY = 0x4;
int DO_MTPLY_sizemsg = sizeof(unsigned short)+sizeof(int)+sizeof(int);

// led count is the same as num band count
uint32_t _mcu_ledcount = 0;
uint32_t _mcu_ipaddr;
uint16_t _mcu_port = mcuport;
uint8_t _mcu_connstate = MCUCONNSTATE_DISCONNECTED;
bool _mcu_sendsg = false;
bool _mcu_previouslyConnected = false;
bool _vis_previouslyConnected = false;

bool _mcu_threadrun = false;
bool _vis_threadrun = false;
bool _vis_keepconnection = true;
std::thread t_mcusock;
std::thread t_vissock;

SocketHandler_Sync *progSock;
SocketHandler_Sync *mcuSock;
SoundGet *sg;

Socket_MessageCallbacks *progSock_cbs;






/*      FUNCTIONS     */
LRESULT mainWindProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
void initProgram(){
  CoInitializeEx(NULL, COINIT_MULTITHREADED);

  hInst = GetModuleHandle(NULL);

  WNDCLASSW wndclass = {
    .lpfnWndProc = mainWindProc,
    .hInstance = hInst,
    .lpszClassName = MAIN_WINDOWCLASS
  };
  RegisterClassW(&wndclass);

  mWnd = CreateWindowExW(
    0,
    MAIN_WINDOWCLASS,
    L"NullWind",
    0,
    0,
    0,
    0,
    0,
    NULL,
    NULL,
    hInst,
    NULL
  );

  if(mWnd == NULL){
    DWORD errcode = GetLastError();
    printf("Error when creating window. Errcode: 0x%X\n", errcode);
    exit(errcode);
  }

  HICON hIcon = (HICON)LoadImageW(NULL, L"../../icon.ico", IMAGE_ICON, 0, 0, LR_LOADFROMFILE);
  visIconData = NOTIFYICONDATAA{
    .hWnd = mWnd,
    .uID = 0,
    .uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP,
    .uCallbackMessage = WM_APP+1,
    .hIcon = hIcon
  };

  visIconData.cbSize = sizeof(visIconData);
  StringCbCopyA(visIconData.szTip, sizeof(visIconData.szTip), "Soundget");
  Shell_NotifyIconA(NIM_ADD, &visIconData);

  DestroyIcon(hIcon);
}
_ProgInitFunc _pifp(initProgram);


void closeProgram(){
  mcuSock->Disconnect();
  if(t_mcusock.joinable())
    t_mcusock.join();
  
  _vis_keepconnection = false;
  progSock->Disconnect();
  if(t_vissock.joinable())
    t_vissock.join();

  Shell_NotifyIconA(NIM_DELETE, &visIconData);
  CloseWindow(mWnd);
  UnregisterClassW(MAIN_WINDOWCLASS, hInst);

  CoUninitialize();
}
_ProgQuitFunc _pqfp(closeProgram);

// TODO
// checking if there's a change in the devices of the soundget
void CheckSGDevices(){

}

void QuitProgram(){
  PostMessageW(mWnd, WM_QUIT, 0, 0);
}





/*      Connection stuff      */
void checkMcuMessage();
void checkSocketMessage(Socket_MessageCallbacks*);
void _sendMCUState();
void _threadMCU(){
  // connecting first
  char ipaddr[16];
  sprintf_s(ipaddr, "%d.%d.%d.%d",
    _mcu_ipaddr & 0xff,
    (_mcu_ipaddr >> 8) & 0xff,
    (_mcu_ipaddr >> 16) & 0xff,
    (_mcu_ipaddr >> 24) & 0xff
  );

  printf("ip: %s\n", ipaddr);

  _mcu_connstate = MCUCONNSTATE_RECONNECT;

  mcuSock->ChangeAddress(mcuport, ipaddr);
  mcuSock->ReconnectSocket();
  if(mcuSock->IsConnected())
    _mcu_connstate = MCUCONNSTATE_CONNECTED;
  else
    _mcu_connstate = MCUCONNSTATE_DISCONNECTED;
  
  printf("state %d\n", _mcu_connstate);
  _sendMCUState();

  if(_mcu_connstate == MCUCONNSTATE_DISCONNECTED){
    _mcu_threadrun = false;
    return;
  }

  // then handling the socket
  while(mcuSock->IsConnected()){
    checkMcuMessage();
    std::this_thread::sleep_for(time_sleepsockthread);
  }
  
  _mcu_connstate = MCUCONNSTATE_DISCONNECTED;
  _sendMCUState();
  _mcu_threadrun = false;
}

void _threadVis(){
  while(_vis_keepconnection){
    if(progSock->IsConnected())
      checkSocketMessage(progSock_cbs);
    else
      progSock->ReconnectSocket();
    
    std::this_thread::sleep_for(time_sleepsockthread);
  }
}

void _startthreadMCU(){
  if(!_mcu_threadrun){
    _mcu_threadrun = true;
    t_mcusock = std::thread(_threadMCU);
  }
}

void _startthreadVis(){
  if(!_vis_threadrun){
    _vis_threadrun = true;
    _vis_keepconnection = true;
    t_vissock = std::thread(_threadVis);
  }
}





/*      Sending function for tool     */
void SendChannelSize(unsigned short channelSize, unsigned short flag){
  if(CHECKFLAG(flag, SENDFLAG_VIS)){
    LockSocket(progSock);
    progSock->SendData(&GET_CONFIG_SG, sizeof(short));
    progSock->SendData(&CFG_SG_CHANNELS, sizeof(short));
    progSock->SendData(&channelSize, sizeof(channelSize));
  }
}

void SendFreqDataToAnotherProg(const float* amp, int length, const WAVEFORMATEX *wavform){
  if(progSock->IsConnected()){
    LockSocket(progSock);
    progSock->SendData(&SOUNDDATA_DATA, sizeof(SOUNDDATA_DATA));
    progSock->SendData(&length, sizeof(length));
    progSock->SendData(amp, sizeof(float) * length);
  }
}

void SendInDevicesName(){
  auto devdata = sg->GetDevices();
  
  size_t datasize = 0;
  datasize += sizeof(short);

  for(int i = 0; i < devdata->size(); i++){
    datasize += sizeof(int);
    datasize += devdata->operator[](i).deviceName.size();
  }

  char *datastr = (char*)malloc(datasize);

  ByteIteratorR _bir(datastr, datasize);
  _bir.setVar((uint16_t)devdata->size());

  for(int i = 0; i < devdata->size(); i++){
    const string *devname = &devdata->operator[](i).deviceName;
    _bir.setVar((uint32_t)devname->size());
    _bir.setVar(devname->c_str(), devname->size());
  }

  {LockSocket(progSock);
    progSock->SendData(&GET_CONFIG_SG, sizeof(short));
    progSock->SendData(&CFG_SG_INPUTDEVICESNAME, sizeof(short));
    progSock->SendData(datastr, datasize);
  }

  free(datastr);
}

void SendInDeviceData(int idx = -1){
  int currentIndex = idx < 0 || idx >= sg->GetDevices()->size()? sg->GetIndex(): idx;
  const string *devname = &sg->GetDevices()->operator[](currentIndex).deviceName;
  auto wvf = sg->GetWaveFormat();

  int datalen = 
    sizeof(int) +
    sizeof(int) + // length of the devname
    devname->size() +
    sizeof(wvf->nChannels) // ushort
  ;

  char *datastr = (char*)malloc(datalen);

  ByteIteratorR _bir(datastr, datalen);
  _bir.setVar(currentIndex);
  _bir.setVar((int)devname->size());
  _bir.setVar(devname->c_str(), devname->size());
  _bir.setVar(wvf->nChannels);

  {LockSocket(progSock);
    progSock->SendData(&GET_CONFIG_SG, sizeof(short));
    progSock->SendData(&CFG_SG_INPUTDEVICEDATA, sizeof(short));
    progSock->SendData(datastr, datalen);
  }

  free(datastr);
}

void SendNumBandMCU(){
  int datalen = sizeof(uint32_t);
  char *datastr = (char*)malloc(datalen);

  ByteIteratorR _bir(datastr, datalen);
  _bir.setVar(_mcu_ledcount);

  {LockSocket(progSock);
    progSock->SendData(&GET_CONFIG_SG, sizeof(uint16_t));
    progSock->SendData(&CFG_SG_MCUBAND, sizeof(uint16_t));
    progSock->SendData(datastr, datalen);
  }

  free(datastr);
}





/*      Msg processing tool     */
void onMsgGet_SetConfigSG(char **buffer, int *buflength){
  CHECKDATALEN(buflength[0], sizeof(uint16_t));
  unsigned short code = 0;
  code = *reinterpret_cast<unsigned short*>(buffer[0]);

  switch(code){
    // TODO
    break; case CFG_SG_INPUTDEVICE:{
      CHECKDATALEN(buflength[1], sizeof(int));

      int idx = 0;
      idx = *reinterpret_cast<int*>(buffer[1]);

      sg->StopListening();
      sg->SetDevice(idx);
      sg->StartListening();

      SendInDeviceData();
    }

    break; case CFG_SG_SENDRATE:{
      CHECKDATALEN(buflength[1], sizeof(uint16_t));
      sg->SetUpdate(*reinterpret_cast<uint16_t*>(buffer[1]));
    }

    break; case CFG_SG_DELAYMS:{

    }
  }
}

void onMsgGet_StopProg(char **buffer, int *buflength){
  QuitProgram();
}

void onMsgGet_StopSoundget(char **buffer, int *buflength){
  sg->StopListening();
}

void onMsgGet_StartSoundget(char **buffer, int *buflength){
  sg->StartListening();
}

void onMsgGet_ForwardMC(char **buffer, int *buflength){
  int len = sizeof(uint16_t)+buflength[0];
  char *_buf = (char*)malloc(len);

  printf("datalen %d\n", buflength[0]);
  ByteIteratorR _bir(_buf, len);
  _bir.setVar(FORWARD);
  _bir.setVar(buffer[0], buflength[0]);

  {LockSocket(mcuSock);
    mcuSock->SendData(_buf, len);
  }
}

void onMsgGet_GetConfigSG(char **buffer, int *buflength){
  CHECKDATALEN(buflength[0], sizeof(uint16_t));
  unsigned short code = *reinterpret_cast<unsigned short*>(buffer[0]);
  switch(code){
    // TODO
    break; case CFG_SG_INPUTDEVICESNAME:{
      SendInDevicesName();
    }

    break; case CFG_SG_INPUTDEVICEDATA:{
      SendInDeviceData();
    }

    break; case CFG_SG_MANYINPUTDEV:{
      
    }

    break; case CFG_SG_CHANNELS:{
      SendChannelSize(sg->GetWaveFormat()->nChannels, SENDFLAG_VIS);
    }

    break; case CFG_SG_MCUBAND:{

    }
  }
}

void _sendMCUState(){
  {LockSocket(progSock);
    progSock->SendData(&CONNECT_MCU_STATE, sizeof(uint16_t));
    progSock->SendData(&_mcu_connstate, sizeof(_mcu_connstate));
  }
}

void onMsgGet_ConnectMCUState(char **buffer, int *buflength){
  CHECKDATALEN(buflength[0], sizeof(uint8_t));
  
  uint8_t code = *reinterpret_cast<uint8_t*>(buffer[0]);
  switch(code){
    // means try connecting
    break; case MCUCONNSTATE_CONNECTED:{
      _startthreadMCU();
    }

    // means disconnecting from mcu
    break; case MCUCONNSTATE_DISCONNECTED:{
      if(mcuSock->IsConnected())
        mcuSock->Disconnect();
      
      if(t_mcusock.joinable())
        t_mcusock.join();
      
      _mcu_connstate = MCUCONNSTATE_DISCONNECTED;
      _sendMCUState();
    }

    // means asking for connection state
    break; case MCUCONNSTATE_REQUESTSTATE:{
      printf("state %d\n", _mcu_connstate);
      _sendMCUState();
    }
  }
}

void onMsgGet_SetMCUAddress(char **buffer, int *buflength){
  CHECKDATALEN(buflength[0], sizeof(uint32_t));
  CHECKDATALEN(buflength[1], sizeof(uint16_t));
  _mcu_ipaddr = *reinterpret_cast<uint32_t*>(buffer[0]);
  _mcu_port = *reinterpret_cast<uint16_t*>(buffer[1]);

  printf("get ip, %d\n", _mcu_ipaddr);
}

void onMsgGet_GetMCUAddress(char **buffer, int *buflength){
  {LockSocket(progSock);
    progSock->SendData(&GETMCU_ADDRESS, sizeof(uint16_t));
    progSock->SendData(&_mcu_ipaddr, sizeof(uint32_t));
    progSock->SendData(&_mcu_port, sizeof(uint16_t));
  }
}





/*      Sending functions for mcu     */
void SendFreqDataToMC_mcu(const float* amp, int length, const WAVEFORMATEX *wavform){
  if(_mcu_sendsg){
    int len =
      sizeof(uint16_t) +
      sizeof(int) +
      (sizeof(float) * length);

    char *buffer = (char*)malloc(len);

    ByteIteratorR _bir(buffer, len);
    _bir.setVar(SOUNDDATA_DATA);
    _bir.setVar(length);
    _bir.setVar(reinterpret_cast<const char*>(amp), sizeof(float)*length);

    mcuSock->SendData(buffer, len);
  }
}





/*      Msg processing mcu      */
void onMsgGetMcu_Forward(char *data, int datalen){
  if(datalen > 0){
    {LockSocket(progSock);
      progSock->SendData(&FORWARD, sizeof(uint16_t));
      progSock->SendData(data, datalen);
    }
  }
}

void onMsgGetMcu_SetConfigSG(char *data, int datalen){
  ByteIterator b_iter(data, datalen);
  if(b_iter.available() < sizeof(uint16_t))
    return;

  uint16_t confcode; b_iter.getVar(confcode);
  switch(confcode){
    break; case CFG_SG_SENDRATE:{
      if(b_iter.available() < sizeof(uint16_t))
        return;
      
      uint16_t rate; b_iter.getVar(rate);
      printf("rate %d\n", rate);
      sg->SetUpdate(rate);
    }

    break; case CFG_SG_MCUBAND:{
      if(b_iter.available() < sizeof(uint16_t))
        return;

      uint16_t ledcount; b_iter.getVar(ledcount);
      _mcu_ledcount = ledcount;

      sg->SetNumBands(_mcu_ledcount);
    }
  }
}

void onMsgGetMcu_GetConfigSG(char *data, int datalen){
  ByteIterator b_iter(data, datalen);
  if(b_iter.available() < sizeof(uint16_t))
    return;

  uint16_t confcode; b_iter.getVar(confcode);
  switch(confcode){
    break; case CFG_SG_INPUTDEVICEDATA:{
      int16_t sidx; b_iter.getVar(sidx);
      sidx = sidx < 0 || sidx >= sg->GetDevices()->size()? sg->GetIndex(): sidx;
      const string *devname = &sg->GetDevices()->operator[](sidx).deviceName;
      auto wvf = sg->GetWaveFormat();

      int datalen =
        sizeof(uint16_t) +
        sizeof(uint16_t) +
        sizeof(uint16_t) +
        sizeof(int) +
        devname->size() +
        sizeof(wvf->nChannels)
      ;

      char *datastr = (char*)malloc(datalen);
      
      ByteIteratorR _bir(datastr, datalen);
      _bir.setVar(GET_CONFIG_SG);
      _bir.setVar(CFG_SG_INPUTDEVICEDATA);
      _bir.setVar((uint16_t)sidx);
      _bir.setVar((int)devname->size());
      _bir.setVar(devname->c_str(), devname->size());
      _bir.setVar(wvf->nChannels);

      mcuSock->SendData(datastr, datalen);
    }
  }
}





/*      callback functions      */
void _callbackOnMcuDc(){
  _mcu_connstate = MCUCONNSTATE_DISCONNECTED;
  _mcu_sendsg = false;
  _sendMCUState();
}

void _callbackOnMcumsg(const char* msg, int msglen){
  if(msglen >= sizeof(uint16_t)){
    ByteIterator b_iter(msg, msglen);

    uint16_t msgcode = 0; b_iter.getVar(msgcode);
    int buflen = msglen-sizeof(uint16_t);
    char *buf = new char[buflen];

    b_iter.getVar(buf, buflen);
    
    switch(msgcode){
      break; case SOUNDDATA_STOPSENDING:{
        _mcu_sendsg = false;
      }

      break; case SOUNDDATA_STARTSENDING:{
        _mcu_sendsg = true;
      }

      break; case FORWARD:{
        onMsgGetMcu_Forward(buf, buflen);
      }

      break; case SET_CONFIG_SG:{
        onMsgGetMcu_SetConfigSG(buf, buflen);
      }

      break; case GET_CONFIG_SG:{
        onMsgGetMcu_GetConfigSG(buf, buflen);
      }
    }

    delete[] buf;
  }
}

void checkMcuMessage(){
  if(mcuSock->IsConnected()){
    // send visualizer signal that the mcu is connected
    if(!_mcu_previouslyConnected){
      _mcu_connstate = MCUCONNSTATE_CONNECTED;
      _sendMCUState();
      _mcu_previouslyConnected = true;
    }

    mcuSock->PingSocket();
    while(mcuSock->IsAnotherMessage()){
      auto p = mcuSock->GetNextMessage();
      _callbackOnMcumsg(p.first, p.second);
      
      if(p.first)
        free(p.first);
    }
  }
  else if(_mcu_previouslyConnected){
    printf("MCU disconnected\n");
    _mcu_connstate = MCUCONNSTATE_DISCONNECTED;
    _mcu_sendsg = false;
    _sendMCUState();
    _mcu_previouslyConnected = false;
  }
}

void checkSocketMessage(Socket_MessageCallbacks *smc){
  if(smc->s_socket->IsConnected()){
    smc->s_socket->PingSocket();
    if(!smc->s_socket->IsAnotherMessage())
      return;
    
    bool waitForHeader = true;
    auto cbIter = smc->callbacks.end();
    int messageParamCount = 0;
    int paramCount = 0;
    int *paramLengths = (int*)malloc(0);
    char **paramContainer = (char**)malloc(0);
    while(smc->s_socket->IsAnotherMessage()){
      auto pairMsg = smc->s_socket->GetNextMessage();
      for(int i = 0; i < pairMsg.second; i++)
        printf("%x ", pairMsg.first[i]);
      printf("\n");
      if(waitForHeader){
        if(pairMsg.second != 2){
          printf("Message code data length isn't correct (%d as supposed to be 2).\n", pairMsg.second);
          continue;
        }

        unsigned short msgCode = *reinterpret_cast<unsigned short*>(pairMsg.first);
        cbIter = smc->callbacks.find(msgCode);
        if(cbIter->second.second == 0){
          cbIter->second.first(NULL, NULL);
          continue;
        }

        if(cbIter != smc->callbacks.end()){
          paramCount = cbIter->second.second;
          paramContainer = (char**)realloc(paramContainer, sizeof(char**)*paramCount);
          paramLengths = (int*)realloc(paramLengths, sizeof(int)*paramCount);
          messageParamCount = 0;
          waitForHeader = false;
        }
        else{
          printf("Listener sent wrong message code of 0x%X\n", msgCode);
        }
      }
      else{
        printf("appending msg\n");
        bool clearParamContainer = smc->s_socket->IsAnotherMessage()? false: true;

        paramContainer[messageParamCount] = pairMsg.first;
        paramLengths[messageParamCount] = pairMsg.second;
        messageParamCount++;

        printf("msgCode %d, count %d, buflen %d\n", cbIter->first, messageParamCount, pairMsg.second);

        if(messageParamCount >= paramCount){
          printf("calling code\n");
          cbIter->second.first(paramContainer, paramLengths);
          clearParamContainer = true;
          waitForHeader = true;
        }

        if(clearParamContainer)
          for(int i = 0; i < messageParamCount; i++)
            free(paramContainer[i]);
      }
    }

    free(paramLengths);
    free(paramContainer);
  }
}

LRESULT mainWindProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam){
  switch(msg){
    break; case WM_APP+1:{
      UINT16 id = HIWORD(lParam);
      UINT16 act = LOWORD(lParam);
      switch(act){
        break; case WM_RBUTTONDOWN:{
          HMENU hMenu = CreatePopupMenu();
          
          AppendMenuW(hMenu, MF_STRING | MF_BYPOSITION, 0, L"Quit");
          AppendMenuW(hMenu, MF_STRING | MF_BYPOSITION, 1, L"Test");

          POINT pt;
          GetCursorPos(&pt);

          SetForegroundWindow(hWnd);
          TrackPopupMenu(hMenu, TPM_LEFTBUTTON | TPM_LEFTALIGN | TPM_TOPALIGN, pt.x, pt.y, 0, hWnd, NULL);
          DestroyMenu(hMenu);
        }

        // run visualizer
        break; case WM_LBUTTONDBLCLK:{

        }
      }
    }

    break; case WM_COMMAND:{
      UINT16 type = HIWORD(wParam);
      UINT16 id = LOWORD(wParam);
      switch(type){
        // menu
        break; case 0:{
          printf("id pressed %d\n", id);
          switch(id){
            break; case IDM_QUIT:{
              QuitProgram();
            }
          }
        }

        break; default:
          DefWindowProc(hWnd, msg, wParam, lParam);
      }
    }

    break; default:
      DefWindowProc(hWnd, msg, wParam, lParam);
  }
}

int main(){
  _onInitProg();

  // setting up socket to vis tool
  progSock = new SocketHandler_Sync(port);
  mcuSock = new SocketHandler_Sync(mcuport);
  progSock_cbs = new Socket_MessageCallbacks{
    .s_socket = progSock,
    .callbacks = {
      {STOP_PROG, {onMsgGet_StopProg, 0}},
      {STOP_SOUNDGET, {onMsgGet_StopSoundget, 0}},
      {START_SOUNDGET, {onMsgGet_StartSoundget, 0}},
      {FORWARD, {onMsgGet_ForwardMC, 1}},
      {SET_CONFIG_SG, {onMsgGet_SetConfigSG, 2}},
      {GET_CONFIG_SG, {onMsgGet_GetConfigSG, 1}},
      {CONNECT_MCU_STATE, {onMsgGet_ConnectMCUState, 1}},
      {SETMCU_ADDRESS, {onMsgGet_SetMCUAddress, 2}},
      {GETMCU_ADDRESS, {onMsgGet_GetMCUAddress, 0}}
    }
  };
  
  // setting up the soundget class
  sg = new SoundGet();
  sg->CallbackListenerRaw = SendFreqDataToAnotherProg;
  sg->CallbackListener = SendFreqDataToMC_mcu;
  auto devices = sg->GetDevices();
  for(int i = 0; i < devices->size(); i++){
    printf("%s\n", devices->operator[](i).deviceName.c_str());
  }


  // starting threads
  sg->SetDevice(0);
  sg->StartListening();

  _startthreadVis();

  MSG msg;
  while(GetMessage(&msg, mWnd, 0, 0)){
    UpdateWindow(mWnd);
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }

  sg->StopListening();

  mcuSock->SafeDelete();
  progSock->SafeDelete();
  delete progSock_cbs;

  _onQuitProg();
}