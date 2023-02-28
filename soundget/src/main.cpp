#include "socket.hpp"
#include "soundget.hpp"
#include "initquit.hpp"
#include "ByteIterator.hpp"

#include "algorithm"
#include "conio.h"
#include "map"
#include "exception"
#include "fstream"
#include "string.h"

#include "windowsx.h"
#include "libloaderapi.h"
#include "commctrl.h"
#include "strsafe.h"

#include "mmdeviceapi.h"


/*      CONSTANT AND DEFINES      */
#define CHECKDATALEN(num, size) if(num != size) return

#define SOCKET_MSGPOLL 10

#define MAIN_WINDOWCLASS L"viswndclass"

#define MCU_ENCRYPTIONKEY_LISTMAX 16
#define MCU_ENCRYPTIONKEY_MAXSTR 128
#define MCU_ENCRYPTIONKEY_DATAFILEPATH "key.bin"
#define MCU_HANDSHAKETIMOUT 5

#define WND_MAINCODEMENU 0x00
#define WND_DEVCODEMENU 0x01
#define WND_FLOWCODEMENU 0x02
#define WND_STARTSOUNDGET 0x03

#define WM_APP_NOTIFYICON WM_APP+0x01
#define WM_APP_SOUNDGETNOTIFICATION WM_APP+0x02

#define NOTIFYICON_UID 0

#define SOUNDGET_SAFECHANGEDEFAULTWAIT std::chrono::seconds(1)


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
  // or can be used to set mcu variables without using tool
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
  GETMCU_ADDRESS = 0x0032,

  // --   Codes that will not be encrypted    --
  // MCU pairing
  // will be send first when connecting to MCU when it's on pairing mode
  MCU_PAIR = 0x0100,
  // Message to connect to MCU
  MCU_CONNECT = 0x101
;


// codes for soundget configuration
const unsigned short
  // [SET] the input device (index based)
  // [GET]
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
  // [SET] setting the flow
  // [GET] asking what the flow is
  CFG_SG_FLOW = 0x0008,
  // [SET] many band to send to mcu (only for mcu)
  // [GET] (only for tool)
  CFG_SG_MCUBAND = 0x0021,
  CFG_SG_MCU_CAN_CHANGE_ADEV = 0x0022
;


// codes for forward codes / setting up mcu variables
const unsigned short
  MCU_SETTIME = 0x103
;


// codes for connecting state
const unsigned char
  MCUCONNSTATE_CONNECTED = 0x01,
  MCUCONNSTATE_DISCONNECTED = 0x02,
  MCUCONNSTATE_RECONNECT = 0x03,
  MCUCONNSTATE_REQUESTSTATE = 0x04
;


const auto time_sleepsockthread = std::chrono::milliseconds(1000/SOCKET_MSGPOLL);


typedef void(*SendMessagesCb)(std::vector<std::pair<const void*, size_t>> msgs);

struct Socket_MessageCallbacks{
  public:
    SocketHandler_Sync *s_socket;

    // <code, <callback, callback's param count>>
    std::map<unsigned short, std::pair<void (*)(char**, int*), int>> callbacks;
};


enum PopupMenu_ChoiceEnum{
  ce_runeditor,
  ce_startsoundget,
  ce_separator1,
  ce_changeDev,
  ce_changeflow,
  ce_separator2,
  ce_quit,
  ce___enumlen
};


/*      GLOBAL VARIABLES      */
HINSTANCE hInst;
HWND mWnd;
NOTIFYICONDATAA visIconData;

unsigned short DO_TEST = 0x3;
unsigned short DO_MTPLY = 0x4;
int DO_MTPLY_sizemsg = sizeof(unsigned short)+sizeof(int)+sizeof(int);

// port for connecting to tool or mcu
unsigned short port = 3022;
unsigned short mcuport = 3020;

// led count is the same as num band count
uint32_t _mcu_ledcount = 0;
uint32_t _mcu_ipaddr;
uint16_t _mcu_port = mcuport;
uint8_t _mcu_connstate = MCUCONNSTATE_DISCONNECTED;
bool _mcu_sendsg = false;
bool _mcu_previouslyConnected = false;
bool _mcu_handshaked = false;
bool _vis_previouslyConnected = false;

data_encryption _mcu_cryptkey;
std::vector<std::pair<char*, size_t>> _mcu_list_cryptkey;

bool _mcu_threadrun = false;
bool _vis_threadrun = false;
bool _vis_keepconnection = true, _vis_closeconnection = false;
std::thread t_mcusock;
std::thread t_vissock;

SocketHandler_Sync *progSock;
SocketHandler_Sync *mcuSock;
SoundGet *sg;

Socket_MessageCallbacks *progSock_cbs;

STARTUPINFOA _vissinfo;
PROCESS_INFORMATION _vispinfo;
std::string _vispathstr;

std::thread _rv_thread;
bool _rv_isrunning = false;


//  config variables
bool _mcu_can_change_audiodev = false;

LPCWSTR _sg_dataflow_str[] = {
  L"Render Device",
  L"Capture Device"
};

int _sg_dataflow_strarrlen = sizeof(_sg_dataflow_str)/sizeof(const char*);




/*      FUNCTION DECLARATIONS     */
void sendMcuData(const void *data, size_t datalen);
void SendMessagesMCU(std::vector<std::pair<const void*, size_t>> msg);
void SendMessagesProg(std::vector<std::pair<const void*, size_t>> msg);
void SendInDeviceData(SendMessagesCb cb, int idx = -1);
void SendInDeviceCurrentIdx(SendMessagesCb cb);
void SendInDevicesName(SendMessagesCb cb);
void SendFlowType(EDataFlow flow, SendMessagesCb cb);




/*      FUNCTIONS     */
LRESULT mainWindProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
void initProgram(){
  CoInitializeEx(NULL, COINIT_MULTITHREADED);

  ZeroMemory(&_vissinfo, sizeof(_vissinfo));
  ZeroMemory(&_vispinfo, sizeof(_vispinfo));

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
    .uID = NOTIFYICON_UID,
    .uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP,
    .uCallbackMessage = WM_APP_NOTIFYICON,
    .hIcon = hIcon
  };

  visIconData.cbSize = sizeof(visIconData);
  StringCbCopyA(visIconData.szTip, sizeof(visIconData.szTip), "Soundget");
  Shell_NotifyIconA(NIM_ADD, &visIconData);

  DestroyIcon(hIcon);


  // setting up socket to vis tool
  progSock = new SocketHandler_Sync(port);
  mcuSock = new SocketHandler_Sync(mcuport);
}
PROGINITFUNC(initProgram);


void closeProgram(){
  Shell_NotifyIconA(NIM_DELETE, &visIconData);
  CloseWindow(mWnd);
  UnregisterClassW(MAIN_WINDOWCLASS, hInst);

  CoUninitialize();

  mcuSock->SafeDelete();
  progSock->SafeDelete();
  delete progSock_cbs;

  printf("done closing\n");
}
PROGQUITFUNC(closeProgram);

void QuitProgram(){
  PostMessageW(mWnd, WM_QUIT, 0, 0);
}


/*      Getting Config      */
void _getConfigData(){
  /*    Data set    */
  std::map<std::string, void(*)(std::string&)> _cfgmap{
    {"vis_path", [](std::string& v){
        _vispathstr = v;
      }
    },

    {"editor_port", [](std::string &v){
        port = stoi(v);
      }
    },

    {"default_mcuport", [](std::string &v){
        mcuport = stoi(v);
      }
    },

    {"mcu_can_change_audiodev", [](std::string &v){
        _mcu_can_change_audiodev = stoi(v);
      }
    }
  };



  /*    Start of Function   */
  std::ifstream ifs;
  ifs.open("config.cfg");
  
  bool loop = true;
  while(loop){
    std::string attr_name = "", attr_value = "";
    bool _assignment = false;
    char c = 0;

    while(true){
      c = ifs.peek();
      if((c == ' ' || c == '\n') && !ifs.eof())
        ifs.read(&c, 1);
      else
        break;
    }

    ifs >> attr_name;

    while(true){
      c = ifs.peek();
      if((c == ' ' || c == '\n' || c == '=') && !ifs.eof()){
        ifs.read(&c, 1);

        if(c == '=')
          _assignment = true;
      }
      else
        break;
    }

    if(_assignment){
      ifs >> attr_value;

      while(true){
        c = ifs.peek();
        if(c != ' ' && c != '\n' && !ifs.eof())
          ifs.read(&c, 1);
        else
          break;
      }
    }

    auto _iter = _cfgmap.find(attr_name);
    if(_iter != _cfgmap.end()){
      try{
        _iter->second(attr_value);
      }
      catch(std::exception e){
        printf("Config's attribute \'%s\' value is invalid.\n", attr_name.c_str());
      }
    }
    else
      printf("Config's attribute name \'%s\' is invalid.\n", attr_name.c_str());
    
    if(ifs.eof())
      break;
  }
}
PROGINITFUNC(_getConfigData);


void _getMCUkeyList(){
  _mcu_list_cryptkey.resize(0);
  _mcu_list_cryptkey.reserve(MCU_ENCRYPTIONKEY_LISTMAX);

  std::ifstream _ifs; _ifs.open(MCU_ENCRYPTIONKEY_DATAFILEPATH, _ifs.binary);
  if(_ifs.fail()){
    printf("Key file %s cannot be opened.\n", MCU_ENCRYPTIONKEY_DATAFILEPATH);
    return;
  }

  uint16_t _listsize; _ifs.read(reinterpret_cast<char*>(&_listsize), sizeof(uint16_t));
  for(int i = 0; i < _listsize && !_ifs.eof() && _mcu_list_cryptkey.size() < MCU_ENCRYPTIONKEY_LISTMAX; i++){
    uint16_t _keylen; _ifs.read(reinterpret_cast<char*>(&_keylen), sizeof(_keylen));
    if(_keylen > MCU_ENCRYPTIONKEY_MAXSTR || _keylen == 0){
      printf("%s seems like to be corrupted. But will use some of the key.\n", MCU_ENCRYPTIONKEY_DATAFILEPATH);
      break;
    }
    
    char *_key = (char*)malloc(_keylen);
    _ifs.read(_key, _keylen);

    _mcu_list_cryptkey.insert(_mcu_list_cryptkey.end(), {_key, _keylen});
  }

  _ifs.close();
}
PROGINITFUNC(_getMCUkeyList);


void _closing_MCUkeyList(){
  for(int i = 0; i < _mcu_list_cryptkey.size(); i++)
    free(_mcu_list_cryptkey[i].first);
}
PROGQUITFUNC(_closing_MCUkeyList);





/*      Updating Functions      */
void _update_mcu_list_cryptkey(){
  std::ofstream _ofs; _ofs.open(MCU_ENCRYPTIONKEY_DATAFILEPATH, _ofs.trunc | _ofs.binary);

  uint16_t _listsize = _mcu_list_cryptkey.size();
  _ofs.write(reinterpret_cast<const char*>(&_listsize), sizeof(uint16_t));
  printf("cryptkey list %d\n", _mcu_list_cryptkey.size());

  for(int i = 0; i < _mcu_list_cryptkey.size(); i++){
    auto _pk = _mcu_list_cryptkey[i];
    uint16_t _keylen = _pk.second;
    
    printf("writing len %d key %s\n", _keylen, _pk.first);

    _ofs.write(reinterpret_cast<const char*>(&_keylen), sizeof(uint16_t));
    _ofs.write(_pk.first, _keylen);

    _ofs.flush();
  }

  _ofs.close();
}




/*      Audio stuff     */
void SetInDevIdx(int idx){
  if(sg->IsListening()){
    sg->StopListening();
    sg->SetDevice(idx);
    sg->StartListening();
  }
  else
    sg->SetDevice(idx);

  SendInDeviceData(SendMessagesProg);

  if(_mcu_can_change_audiodev)
    SendInDeviceData(SendMessagesMCU);
}

void SetInDevFlow(EDataFlow flow){
  if(sg->IsListening()){
    sg->StopListening();
    sg->SetDeviceFlow(flow);
    sg->SetDevice();
    sg->StartListening();
  }
  else{
    sg->SetDeviceFlow(flow);
    sg->SetDevice();
  }

  SendInDevicesName(SendMessagesProg);
  SendInDeviceData(SendMessagesProg);
  SendFlowType(flow, SendMessagesProg);

  if(_mcu_can_change_audiodev){
    SendInDevicesName(SendMessagesMCU);
    SendInDeviceData(SendMessagesMCU);
    SendFlowType(flow, SendMessagesMCU);
  }
}






/*      Connection stuff      */
void checkMcuMessage();
void checkSocketMessage(Socket_MessageCallbacks*);
void _sendMCUState();
void _threadMCU(){
  try{
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
    if(!mcuSock->IsConnected()){
      _mcu_connstate = MCUCONNSTATE_DISCONNECTED;
      _sendMCUState();
    }

    if(_mcu_connstate == MCUCONNSTATE_DISCONNECTED){
      _mcu_threadrun = false;
      return;
    }

    // then handling the socket
    auto _timestart = std::chrono::high_resolution_clock::now();
    while(mcuSock->IsConnected()){
      if(!_mcu_handshaked){
        auto _currtime = std::chrono::high_resolution_clock::now();
        if((_currtime-_timestart) > std::chrono::seconds(MCU_HANDSHAKETIMOUT)){
          mcuSock->Disconnect();
          break;
        }
      }

      checkMcuMessage();
      std::this_thread::sleep_for(time_sleepsockthread);
    }
    
    _mcu_connstate = MCUCONNSTATE_DISCONNECTED;
    _sendMCUState();
    _mcu_threadrun = false;
    _mcu_handshaked = false;
  }
  catch(std::system_error e){
    std::cout << e.what() << std::endl;
  }
}

void _threadVis(){
  while(_vis_keepconnection){
    if(!_vis_closeconnection){
      if(progSock->IsConnected())
        checkSocketMessage(progSock_cbs);
      else
        progSock->ReconnectSocket();
    }
    else{
      _vis_keepconnection = false;
      progSock->Disconnect();
    }
    
    std::this_thread::sleep_for(time_sleepsockthread);
  }
}

void _startthreadMCU(){
  if(!_mcu_threadrun){
    _mcu_threadrun = true;

    if(t_mcusock.joinable())
      t_mcusock.join();
    t_mcusock = std::thread(_threadMCU);
  }
}

void _startthreadVis(){
  if(!_vis_threadrun){
    _vis_threadrun = true;
    _vis_keepconnection = true;
    _vis_closeconnection = false;

    if(t_vissock.joinable())
      t_vissock.join();
    t_vissock = std::thread(_threadVis);
  }
}





/*      Sending functions to tool and mcu     */
void SendInDeviceCurrentIdx(SendMessagesCb cb){
  int16_t idx = sg->GetIndex();
  
  cb({
    {&GET_CONFIG_SG, sizeof(uint16_t)},
    {&CFG_SG_INPUTDEVICESNAME, sizeof(uint16_t)},
    {&idx, sizeof(int16_t)}
  });
}

void SendInDevicesName(SendMessagesCb cb){
  auto devdata = sg->GetDevices();
  
  size_t datasize = 0;
  datasize += sizeof(uint16_t);

  for(int i = 0; i < devdata->size(); i++){
    datasize += sizeof(int);
    datasize += devdata->operator[](i).deviceName.size();
  }

  char *datastr = (char*)malloc(datasize);

  ByteIteratorR _bir(datastr, datasize);
  _bir.setVar((uint16_t)devdata->size());

  for(int i = 0; i < devdata->size(); i++){
    const std::string *devname = &devdata->operator[](i).deviceName;
    _bir.setVar((uint32_t)devname->size());
    _bir.setVarStr(devname->c_str(), devname->size());
  }

  cb({
    {&GET_CONFIG_SG, sizeof(uint16_t)},
    {&CFG_SG_INPUTDEVICESNAME, sizeof(uint16_t)},
    {datastr, datasize}
  });

  free(datastr);
}

void SendInDeviceData(SendMessagesCb cb, int idx){
  int currentIndex = idx < 0? sg->GetIndex(): idx;
  const SoundDevice *_currdev = sg->GetDevice(currentIndex);
  const WAVEFORMATEX *pwvf = sg->GetWaveFormat();

  std::string devname = "NULL";
  WAVEFORMATEX wvf = WAVEFORMATEX();

  if(pwvf && _currdev){
    devname = _currdev->deviceName;
    wvf = *pwvf;
  }

  int datalen = 
    sizeof(int) +
    sizeof(int) + // length of the devname
    devname.size() +
    sizeof(wvf.nChannels) // ushort
  ;

  char *datastr = (char*)malloc(datalen);

  ByteIteratorR _bir(datastr, datalen);
  _bir.setVar(currentIndex);
  _bir.setVar((int)devname.size());
  _bir.setVarStr(devname.c_str(), devname.size());
  _bir.setVar(wvf.nChannels);

  cb({
    {&GET_CONFIG_SG, sizeof(uint16_t)},
    {&CFG_SG_INPUTDEVICEDATA, sizeof(uint16_t)},
    {datastr, datalen}
  });

  free(datastr);
}

void SendInDeviceMany(SendMessagesCb cb){
  uint16_t devMany = sg->GetDevices()->size();

  cb({
    {&GET_CONFIG_SG, sizeof(uint16_t)},
    {&CFG_SG_MANYINPUTDEV, sizeof(uint16_t)},
    {&devMany, sizeof(uint16_t)}
  });
}

void SendChannelSize(unsigned short channelSize, SendMessagesCb cb){
  cb({
    {&GET_CONFIG_SG, sizeof(uint16_t)},
    {&CFG_SG_CHANNELS, sizeof(uint16_t)},
    {&channelSize, sizeof(channelSize)}
  });
}

void SendNumBandMCU(SendMessagesCb cb){
  int datalen = sizeof(uint32_t);
  char *datastr = (char*)malloc(datalen);

  ByteIteratorR _bir(datastr, datalen);
  _bir.setVar(_mcu_ledcount);

  cb({
    {&GET_CONFIG_SG, sizeof(uint16_t)},
    {&CFG_SG_MCUBAND, sizeof(uint16_t)},
    {datastr, datalen}
  });

  free(datastr);
}

void SendFlowType(EDataFlow flow, SendMessagesCb cb){
  cb({
    {&GET_CONFIG_SG, sizeof(uint16_t)},
    {&CFG_SG_FLOW, sizeof(uint16_t)},
    {&flow, sizeof(int16_t)}
  });
}






/*      Sending function for tool     */
void SendFreqDataToAnotherProg(const float* amp, int length, const WAVEFORMATEX *wavform){
  if(progSock->IsConnected()){
    LockSocket(progSock);
    progSock->SendData(&SOUNDDATA_DATA, sizeof(SOUNDDATA_DATA));
    progSock->SendData(&length, sizeof(length));
    progSock->SendData(amp, sizeof(float) * length);
  }
}





/*      Msg processing tool     */
void onMsgGet_SetConfigSG(char **buffer, int *buflength){
  CHECKDATALEN(buflength[0], sizeof(uint16_t));
  unsigned short code = 0;
  code = *reinterpret_cast<unsigned short*>(buffer[0]);

  switch(code){
    break; case CFG_SG_INPUTDEVICE:{
      CHECKDATALEN(buflength[1], sizeof(int16_t));
      SetInDevIdx(*reinterpret_cast<int16_t*>(buffer[1]));
    }

    break; case CFG_SG_SENDRATE:{
      CHECKDATALEN(buflength[1], sizeof(uint16_t));
      sg->SetUpdate(*reinterpret_cast<uint16_t*>(buffer[1]));
    }

    // TODO
    break; case CFG_SG_DELAYMS:{

    }

    break; case CFG_SG_FLOW:{
      CHECKDATALEN(buflength[1], sizeof(int16_t));
      SetInDevFlow((EDataFlow)*reinterpret_cast<uint16_t*>(buffer[1]));
    }
  }
}

void onMsgGet_StopSoundget(char **buffer, int *buflength){
  sg->StopListening();
}

void onMsgGet_StartSoundget(char **buffer, int *buflength){
  sg->StartListening();
}

void onMsgGet_ForwardMC(char **buffer, int *buflength){
  int len = sizeof(uint16_t)+buflength[0];
  char _buf[len];

  printf("datalen %d\n", buflength[0]);
  printf("byte dump: ");
  for(int i = 0; i < buflength[0]; i++)
    printf("%X ", buffer[0][i]);
    
  ByteIteratorR_Encryption _bir(_buf, len, &_mcu_cryptkey);
  _bir.setVar(FORWARD);
  _bir.setVarStr(buffer[0], buflength[0]);

  sendMcuData(_buf, len);
}

void onMsgGet_GetConfigSG(char **buffer, int *buflength){
  CHECKDATALEN(buflength[0], sizeof(uint16_t));
  unsigned short code = *reinterpret_cast<unsigned short*>(buffer[0]);
  switch(code){
    break; case CFG_SG_INPUTDEVICESNAME:{
      SendInDevicesName(SendMessagesProg);
    }

    break; case CFG_SG_INPUTDEVICEDATA:{
      SendInDeviceData(SendMessagesProg);
    }

    break; case CFG_SG_MANYINPUTDEV:{
      SendInDeviceMany(SendMessagesProg);
    }

    break; case CFG_SG_CHANNELS:{
      SendChannelSize(sg->GetWaveFormat()->nChannels, SendMessagesProg);
    }

    break; case CFG_SG_MCUBAND:{
      SendNumBandMCU(SendMessagesProg);
    }

    break; case CFG_SG_FLOW:{
      SendFlowType(sg->GetDataFlow(), SendMessagesProg);
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

void onMsgGet_StopProg(char **buffer, int *buflength){
  QuitProgram();
}





/*      Sending functions for mcu     */
void SendFreqDataToMCU(const float* amp, int length, const WAVEFORMATEX *wavform){
  if(_mcu_sendsg){
    int len =
      sizeof(uint16_t) +
      sizeof(int) +
      (sizeof(float) * length);

    char buffer[len];

    ByteIteratorR_Encryption _bir(buffer, len, &_mcu_cryptkey);
    _bir.setVar(SOUNDDATA_DATA);
    _bir.setVar(length);
    _bir.setVarStr(reinterpret_cast<const char*>(amp), sizeof(float)*length);

    sendMcuData(buffer, len);
  }
}

void SendTimeDataToMCU(){
  time_t _utime = time(NULL);
  printf("unixtime %d\n", _utime);

  int _buflen =
    sizeof(uint16_t) +
    sizeof(tm)
  ;

  char _buf[_buflen];
  ByteIteratorR _bir(_buf, _buflen);
  _bir.setVar(MCU_SETTIME);
  _bir.setVar(*localtime(&_utime));

  char *__buf = _buf;
  onMsgGet_ForwardMC(&__buf, &_buflen);
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
    break; case CFG_SG_INPUTDEVICE:{
      if(_mcu_can_change_audiodev && b_iter.available() >= sizeof(uint16_t)){
        int16_t sidx; b_iter.getVar(sidx);
        SetInDevIdx(sidx);
      }
    }

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
    break; case CFG_SG_INPUTDEVICE:{
      if(_mcu_can_change_audiodev)
        SendInDeviceCurrentIdx(SendMessagesMCU);
    }
    
    break; case CFG_SG_INPUTDEVICEDATA:{
      if(_mcu_can_change_audiodev){
        int16_t sidx; b_iter.getVar(sidx);
        SendInDeviceData(SendMessagesMCU, sidx);
      }
    }

    break; case CFG_SG_MANYINPUTDEV:{
      if(_mcu_can_change_audiodev)
        SendInDeviceMany(SendMessagesMCU);
    }

    break; case CFG_SG_MCU_CAN_CHANGE_ADEV:{
      bool _b = _mcu_can_change_audiodev;

      SendMessagesMCU({
        {&GET_CONFIG_SG, sizeof(uint16_t)},
        {&CFG_SG_MCU_CAN_CHANGE_ADEV, sizeof(uint16_t)},
        {&_b, sizeof(bool)}
      });
    }
  }
}

void _onMCUHandshaked(){
  printf("Handshaked\n");
  _mcu_handshaked = true;
  _mcu_connstate = MCUCONNSTATE_CONNECTED;
  _sendMCUState();

  // then send some data to MCU
  SendTimeDataToMCU();
}

void _connectMessageToMcu(const char *random, int randomlen){
  // then send an encrypted key to mcu
  auto _pk = _mcu_cryptkey.getKey();

  size_t _maxlen = _pk.second/4*3;
  size_t _minlen = _pk.second/4;

  uint32_t _rand; memcpy(&_rand, random, std::min((int)sizeof(uint32_t), randomlen));
  size_t _len = sizeof(uint16_t) + std::max(_rand % _maxlen, _minlen);
  char _enkey[_len];

  ByteIteratorR _bir(_enkey, _len);
  _bir.setVar(MCU_CONNECT);

  memcpy(_enkey+_bir.tellidx(), _pk.first, _bir.available());

  printf("decrypted hskey: %s", _enkey+_bir.tellidx());

  _mcu_cryptkey.encryptOrDecryptData(_enkey+_bir.tellidx(), _bir.available(), _bir.available());

  printf("encrypted hskey: %s\n", _enkey+_bir.tellidx());

  mcuSock->SendData(_enkey, _len);

  _onMCUHandshaked();
}

void onMsgGetMcu_McuPair(char *data, int datalen){
  printf("pairing\n");
  if(datalen > 0){
    printf("using key data\n");
    char *_key = (char*)malloc(datalen);
    memcpy(_key, data, datalen);

    if(_mcu_list_cryptkey.size() >= MCU_ENCRYPTIONKEY_LISTMAX)
      _mcu_list_cryptkey.erase(_mcu_list_cryptkey.end());

    printf("key from mcu: %s\n", _key);
    _mcu_list_cryptkey.insert(_mcu_list_cryptkey.begin(), {_key, datalen});
    _update_mcu_list_cryptkey();

    _mcu_cryptkey.useKey(_key, datalen);
    _connectMessageToMcu(_key, datalen);
  }
  else{
    printf("asking key data\n");
    size_t buflen = sizeof(uint16_t);
    char buf[buflen];

    ByteIteratorR _bir(buf, buflen);
    _bir.setVar(MCU_PAIR);

    // note: not using sendMcuData since we want to handshake
    mcuSock->SendData(buf, buflen);
  }
}

void onMsgGetMcu_McuConnect(char *data, int datalen){
  // then add the key with the ones that stored in a file
  // if match, then handshake

  printf("Connecting\n");
  if(datalen > 0){
    int _foundidx = -1;
    for(size_t i = 0; i < _mcu_list_cryptkey.size(); i++){
      auto _pk = _mcu_list_cryptkey[i];
      data_encryption _crypt;
      _crypt.useKey(_pk.first, _pk.second, true);

      printf("key to compare: %s\n", _pk.first);

      if(_pk.second < datalen)
        continue;

      size_t o = 0;
      for(; o < datalen; o++){
        char _c = data[o];
        _crypt.encryptOrDecryptData(&_c, 1, o+datalen);

        printf("comparing: %X %X\n", (int)_c, (int)_pk.first[o]);

        if(_c != _pk.first[o])
          break;
      }

      if(o == datalen){
        _foundidx = i;
        break;
      }
    }

    if(_foundidx >= 0){
      printf("key found!\n");
      auto _pk = _mcu_list_cryptkey[_foundidx];
    
      _mcu_cryptkey.useKey(_pk.first, _pk.second);
      _connectMessageToMcu(data, datalen);
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
  printf("oncbmsg\n");
  if(msglen >= sizeof(uint16_t)){
    if(_mcu_handshaked){
      printf("handshaked\n");
      ByteIterator_Encryption b_iter(msg, msglen, &_mcu_cryptkey);

      printf("key: %s\nmsgdata: %s\n", _mcu_cryptkey.getKey().first, msg);

      uint16_t msgcode = 0; b_iter.getVar(msgcode);
      printf("msgcode %X\n", msgcode);
      int buflen = b_iter.available();
      char buf[buflen];

      b_iter.getVarStr(buf, buflen);

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
    }
    else{
      printf("Not handshaked\n");
      ByteIterator b_iter(msg, msglen);

      uint16_t msgcode = 0; b_iter.getVar(msgcode);

      printf("msgcode %d\n");
      int buflen = b_iter.available();
      char buf[buflen];

      b_iter.getVarStr(buf, buflen);
      
      switch(msgcode){
        break; case MCU_PAIR:{
          onMsgGetMcu_McuPair(buf, buflen);
        }

        break; case MCU_CONNECT:{
          onMsgGetMcu_McuConnect(buf, buflen);
        }
      }
    }
  }
}

void sendMcuData(const void *buf, size_t buflen){
  if(_mcu_handshaked){
    {LockSocket(mcuSock);
      mcuSock->SendData(buf, buflen);
    }
  }
}

void SendMessagesMCU(std::vector<std::pair<const void*, size_t>> msg){
  size_t datalen = 0;
  for(size_t i = 0; i < msg.size(); i++)
    datalen += msg[i].second;
  
  char data[datalen];
  ByteIteratorR_Encryption _bir(data, datalen, &_mcu_cryptkey);
  for(size_t i = 0; i < msg.size(); i++)
    _bir.setVarStr(reinterpret_cast<const char*>(msg[i].first), msg[i].second);

  sendMcuData(data, datalen);
}

void SendMessagesProg(std::vector<std::pair<const void*, size_t>> msg){
  {LockSocket(progSock);
    for(size_t i = 0; i < msg.size(); i++)
      progSock->SendData(msg[i].first, msg[i].second);
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
    _mcu_handshaked = false;
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

void _runVisualizer(){
  // checking if the editor is running
  for(int i = 0; i < 10 && progSock->IsConnected(); i++){
    progSock->PingSocket();
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }

  if(!progSock->IsConnected()){
    CloseHandle(_vissinfo.hStdInput);
    CloseHandle(_vissinfo.hStdOutput);
    CloseHandle(_vissinfo.hStdError);
    CloseHandle(_vispinfo.hProcess);
    CloseHandle(_vispinfo.hThread);

    ZeroMemory(&_vissinfo, sizeof(_vissinfo));
    _vissinfo.cb = sizeof(_vissinfo);

    ZeroMemory(&_vispinfo, sizeof(_vispinfo));

    char *_strpath = (char*)malloc(_vispathstr.size()+1);
    strncpy(_strpath, _vispathstr.c_str(), _vispathstr.size());
    _strpath[_vispathstr.size()] = '\0';

    CreateProcessA(NULL, (LPSTR)_strpath, NULL, NULL, false, NORMAL_PRIORITY_CLASS, NULL, NULL, &_vissinfo, &_vispinfo);

    free(_strpath);

    // then wait until the program is connected
    for(int i = 0; i < 10 && progSock->IsConnected(); i++){
      progSock->PingSocket();
      std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    if(!progSock->IsConnected())
      printf("Editor can't be connected. The program specified might not be the editor.");
  }
  else
    printf("User has already run visualizer!\n");

  _rv_isrunning = false;
}

void runVisualizer(){
  if(!_rv_isrunning){
    _rv_isrunning = true;
    if(_rv_thread.joinable())
      _rv_thread.join();
    
    _rv_thread = std::thread(_runVisualizer);
  }
}

LRESULT mainWindProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam){
  switch(msg){
    break; case WM_APP_NOTIFYICON:{
      UINT16 id = HIWORD(lParam);
      UINT16 act = LOWORD(lParam);
      switch(act){
        break; case WM_RBUTTONDOWN:{
          HMENU hMenu = CreatePopupMenu();
          
          for(int i = 0; i < ce___enumlen; i++){
            switch((PopupMenu_ChoiceEnum)i){
              break; case ce_runeditor:
                AppendMenuW(hMenu, MF_STRING | MF_BYPOSITION, (int)ce_runeditor | (WND_MAINCODEMENU << 8), L"Run Editor");

              break; case ce_startsoundget:
                AppendMenuW(hMenu, MF_STRING | MF_BYPOSITION, (int)ce_startsoundget | (WND_STARTSOUNDGET << 8), sg->IsListening()? L"Stop Listening": L"Start Listening");
              
              break; case ce_changeDev:{
                HMENU _hsubmenu = CreatePopupMenu();
                
                auto _devdata = sg->GetDevices();
                for(int i = 0; i <= _devdata->size(); i++){
                  const char *_devstr = (i == 0)? "Default": _devdata->operator[](i-1).deviceName.c_str();
                  int _strsize = MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, _devstr, -1, NULL, 0);

                  LPWSTR _wstr = (LPWSTR)malloc(_strsize*sizeof(WCHAR));
                  MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, _devstr, -1, _wstr, _strsize);


                  AppendMenuW(
                    _hsubmenu,
                    MF_STRING | MF_BYPOSITION | ((i == (sg->GetIndex()+1))? MF_CHECKED: 0),
                    i | (WND_DEVCODEMENU << 8),
                    _wstr
                  );

                  if(i == 0)
                    AppendMenuW(_hsubmenu, MF_SEPARATOR, NULL, NULL);

                  free(_wstr);
                }

                AppendMenuW(hMenu, MF_STRING | MF_BYPOSITION | MF_POPUP, (UINT_PTR)_hsubmenu, L"Change Dev");

                DestroyMenu(_hsubmenu);
              }

              break; case ce_changeflow:{
                HMENU _hsubmenu = CreatePopupMenu();

                for(int i = 0; i < _sg_dataflow_strarrlen; i++)
                  AppendMenuW(
                    _hsubmenu,
                    MF_STRING | MF_BYPOSITION | ((i == sg->GetDataFlow())? MF_CHECKED: 0),
                    i | (WND_FLOWCODEMENU << 8),
                    _sg_dataflow_str[i]
                  );

                AppendMenuW(hMenu, MF_STRING | MF_BYPOSITION | MF_POPUP, (UINT_PTR)_hsubmenu, L"Change Flow");
                DestroyMenu(_hsubmenu);
              }
              
              break; case ce_quit:
                AppendMenuW(hMenu, MF_STRING | MF_BYPOSITION, (int)ce_quit | (WND_MAINCODEMENU << 8), L"Quit");
              
              break;
                case ce_separator1:
                case ce_separator2:
                  AppendMenuW(hMenu, MF_SEPARATOR, NULL, NULL);
            }
          }

          POINT pt;
          GetCursorPos(&pt);

          SetForegroundWindow(hWnd);
          TrackPopupMenu(hMenu, TPM_LEFTBUTTON | TPM_LEFTALIGN | TPM_TOPALIGN, pt.x, pt.y, 0, hWnd, NULL);
          DestroyMenu(hMenu);
        }

        // run visualizer
        break; case WM_LBUTTONDBLCLK:{
          runVisualizer();
        }
      }
    }

    break; case WM_APP_SOUNDGETNOTIFICATION:
      std::this_thread::sleep_for(SOUNDGET_SAFECHANGEDEFAULTWAIT);
      sg->GetNotificationHandler()->CheckNotifications();

    break; case WM_COMMAND:{
      UINT16 type = HIWORD(wParam);
      UINT16 id = LOWORD(wParam);
      switch(type){
        // menu
        break; case 0:{
          uint8_t _code = id >> 8;
          id &= 0x00ff;
          switch(_code){
            break; case WND_MAINCODEMENU:{
              switch((PopupMenu_ChoiceEnum)id){
                break; case ce_runeditor:{
                  runVisualizer();
                }

                break; case ce_quit:{
                  QuitProgram();
                }
              }
            }

            break; case WND_DEVCODEMENU:{
              SetInDevIdx(id-1);
            }

            break; case WND_FLOWCODEMENU:{
              SetInDevFlow((EDataFlow)id);
            }

            break; case WND_STARTSOUNDGET:{
              if(sg->IsListening())
                sg->StopListening();
              else
                sg->StartListening();
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

void _soundget_callbackOnChanged(SoundGet::SoundGetNotification_changeStatus status){
  printf("soundget status %d\n", status);
  switch(status){
    break; case SoundGet::SoundGetNotification_changeStatus::SGN_STATUS_DEFAULTCHANGED:
      SendInDeviceData(SendMessagesProg);
    
    break;
      case SoundGet::SoundGetNotification_changeStatus::SGN_STATUS_ADDED:
      case SoundGet::SoundGetNotification_changeStatus::SGN_STATUS_REMOVED:
        SendInDevicesName(SendMessagesProg);
  }
}


int main(){
  _onInitProg();

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
  sg->CallbackListener = SendFreqDataToMCU;

  SoundGet::_NotificationHandling *_sghandler = sg->GetNotificationHandler();
  _sghandler->UseCallback_onDeviceChanged(_soundget_callbackOnChanged);
  _sghandler->BindProcess(mWnd, WM_APP_SOUNDGETNOTIFICATION);

  auto devices = sg->GetDevices();
  for(int i = 0; i < devices->size(); i++){
    printf("%s\n", devices->operator[](i).deviceName.c_str());
  }

  // starting threads
  sg->SetDevice();
  sg->StartListening();

  _startthreadVis();

  _mcu_ipaddr = 0xe39ea8c0;
  _startthreadMCU();

  MSG msg;
  while(GetMessage(&msg, mWnd, 0, 0)){
    UpdateWindow(mWnd);
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }


  sg->StopListening();
  mcuSock->Disconnect();
  if(t_mcusock.joinable())
    t_mcusock.join();
  
  _vis_closeconnection = true;
  if(t_vissock.joinable())
    t_vissock.join();

  _onQuitProg();
}