#include "socket.hpp"
#include "initquit.hpp"
#include "ByteIterator.hpp"

#include "stdio.h"

#include "cmath"
#include "thread"
#include "chrono"


#define WS_SELECTREAD 0
#define WS_SELECTWRITE 1
#define WS_SELECTEXCEPT 2

#define SOCKET_TIMEOUTMS 5000  // 5 s

#define SOCKET_RECONNECTTRIES 10


const int MAXTIMESRESPUDP = 32;
const int SIZEOFMSGHEADER = 4;


const unsigned short
  PINGMESSAGE = 0x1,
  SENDMESSAGE = 0x2,
  CLSOCKMESSAGE = 0x3,
  EOBULKMSG = 0x4,
  // for sync, it will be handled by the main program, or thread
  CHECKRATE = 0x5
;


// messages for UDP sockets
const unsigned short 
  UDPS_MSG = 0x7000,
  UDPS_CONNECTMSG = UDPS_MSG + 0x1,
  UDPS_DISCONNECTMSG = UDPS_MSG + 0x2,
  UDPS_SETPOLLRATE = UDPS_MSG + 0x3,
  UDPS_MAXMSGSIZE = UDPS_MSG + 0x5,
  UDPS_PINGMESSAGEDATA = UDPS_MSG + 0x10,
  UDPS_PACKETRECEIVED = UDPS_MSG + 0x11
;

// 1 s
TIMEVAL _sock_disconnect_timeout = {
  .tv_sec = 1,
  .tv_usec = 0
};

// 10 ms
TIMEVAL _sock_timeout = {
  .tv_sec = 0,
  .tv_usec = 10000
};

WSAData *wsadat = new WSAData();

void initSocket(){
  wsadat = new WSADATA();
  int errcode = WSAStartup(MAKEWORD(2,2), wsadat);
  if(errcode != 0){
    printf("Cannot initialize windows socket.\n");
    throw;
  }
}
_ProgInitFunc _pifs(initSocket);

void closeSocket(){
  printf("closing socket");
  delete wsadat;
  WSACleanup();
}
_ProgQuitFunc _pqfs(closeSocket);




bool operator==(sockaddr_in &s1, sockaddr_in &s2){
  return s1.sin_addr.S_un.S_addr == s2.sin_addr.S_un.S_addr &&
    s1.sin_port == s2.sin_port;
}

bool operator!=(sockaddr_in &s1, sockaddr_in &s2){
  return !(s1 == s2);
}




/* SocketHandler_Sync class */
SocketHandler_Sync::SocketHandler_Sync(unsigned short port, const char *ip){
  ChangeAddress(port, ip);
  ReconnectSocket();
}

SocketHandler_Sync::~SocketHandler_Sync(){
  SafeDelete();
}

bool SocketHandler_Sync::_checkrecv(){
  fd_set sset;
  FD_ZERO(&sset);
  FD_SET(currSock, &sset);
  select(0, &sset, NULL, NULL, &_sock_timeout);
  if(FD_ISSET(currSock, &sset) && lastErrcode != SOCKET_ERROR && lastErrcode != 0){
    _onresponding();
    return true;
  }
  else{
    _onnotresponding();
    return false;     
  }
}

void SocketHandler_Sync::_onnotresponding(){
  if(_responded){
    _responded = false;
    _starttimeout = std::chrono::high_resolution_clock::now();
  }
  else{
    auto _finishtime = std::chrono::high_resolution_clock::now();
    long deltams = (_finishtime-_starttimeout).count()/1000000;
    //printf("deltams %ld\n", deltams);

    if(deltams > SOCKET_TIMEOUTMS){
      stopSending = true;
      lastErrcode = SOCKET_ERROR;
    }
  }
}

void SocketHandler_Sync::_onresponding(){
  _responded = true;
}

std::mutex &SocketHandler_Sync::GetMutex(){
  return sMutex;
}

std::pair<char*, int> SocketHandler_Sync::GetNextMessage(){
  auto res = messageQueues.front();
  messageQueues.pop();
  
  return res;
}

bool SocketHandler_Sync::IsAnotherMessage(){
  return messageQueues.size() > 0;
}

void SocketHandler_Sync::PingSocket(){
  if(!stopSending){
    std::lock_guard<std::mutex> _lock(sMutex);

    if(!_checkingDelay){
      _checkingDelay = true;
      _starttime = std::chrono::high_resolution_clock::now();
    }
    
    lastErrcode = send(currSock, reinterpret_cast<const char*>(&PINGMESSAGE), sizeof(unsigned short), 0);
    bool keepRecv = true;
    while(keepRecv){
      unsigned short msgcode = 0;

      if(_checkrecv()){
        if(_checkingDelay){
          _checkingDelay = false;
          auto _finishtime = std::chrono::high_resolution_clock::now();
          pingDelayMs = (_finishtime - _starttime).count()/1000000;
        }
      }
      else
        break;

      lastErrcode = recv(currSock, reinterpret_cast<char*>(&msgcode), sizeof(unsigned short), 0);
      if(lastErrcode != SOCKET_ERROR){
        switch(msgcode){
          break; case SENDMESSAGE:{
            int length = 0;
            char *msgdat;

            if(!_checkrecv())
              break;
            
            lastErrcode = recv(currSock, reinterpret_cast<char*>(&length), sizeof(length), 0);

            if(!_checkrecv())
              break;
            
            msgdat = new char[length];
            lastErrcode = recv(currSock, msgdat, length, 0);
            
            messageQueues.push(std::pair<char*, int>(msgdat, length));
          }

          break; case CLSOCKMESSAGE:{
            shutdown(currSock, SD_BOTH);
            lastErrcode = SOCKET_ERROR;
            keepRecv = false;
          }
        }
      }
    }
  }
}

bool SocketHandler_Sync::IsConnected(){
  return !stopSending;
}

const auto _reconnectTimeDelay = std::chrono::milliseconds(100);
const int _reconnectTimes = 5;
void SocketHandler_Sync::ReconnectSocket(){
  if(!stopSending)
    return;

  std::lock_guard<std::mutex> _lock(sMutex);
  currSock = socket(AF_INET, SOCK_STREAM, 0);
  
  lastErrcode = connect(currSock, (sockaddr*)hostAddress, sizeof(*hostAddress));
  for(int i = 0; i < SOCKET_RECONNECTTRIES; i++){
    lastErrcode = send(currSock, reinterpret_cast<const char*>(&PINGMESSAGE), sizeof(unsigned short), 0);
    if(_checkrecv()){
      _responded = true;
      stopSending = false;
    }
  }
}

void SocketHandler_Sync::Disconnect(){
  std::lock_guard<std::mutex> _lock(sMutex);
  send(currSock, reinterpret_cast<const char*>(&CLSOCKMESSAGE), sizeof(unsigned short), 0);
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  shutdown(currSock, SD_BOTH);
  closesocket(currSock);
  stopSending = true;
  lastErrcode = SOCKET_ERROR;
}

void SocketHandler_Sync::SendData(const void *data, int datalength){
  if(stopSending)
    return;

  int buflen =
    sizeof(uint16_t) +
    sizeof(int) +
    datalength
  ;

  char buf[buflen];
  ByteIteratorR _bir{buf, buflen};
  _bir.setVar(SENDMESSAGE);
  _bir.setVar(datalength);
  _bir.setVar(data, datalength);

  lastErrcode = send(currSock, buf, buflen, 0);
}

void SocketHandler_Sync::SafeDelete(){
  if(stopSending)
    return;

  Disconnect();
  
  delete hostAddress;
  hostAddress = NULL;
}

unsigned long SocketHandler_Sync::LastPing(){
  return pingDelayMs;
}

void SocketHandler_Sync::ChangeAddress(unsigned short port, const char *ip){
  if(stopSending){
    if(!hostAddress)
      hostAddress = new sockaddr_in();

    hostAddress->sin_addr.S_un.S_addr = inet_addr(ip);
    hostAddress->sin_family = AF_INET;
    hostAddress->sin_port = htons(port);
  }
}