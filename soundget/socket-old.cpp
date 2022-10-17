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


// TODO make a listener (or not)
// TODO add a timeout to the socket
// if the socket isn't connected n many times, the socket will stop updating
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
  UDPS_MAXMSGSIZE = UDPS_MSG + 0x5
;

// 1 s
TIMEVAL _sock_disconnect_timeout = {
  .tv_sec = 1,
  .tv_usec = 0
};

// 100 ms
TIMEVAL _sock_timeout = {
  .tv_sec = 0,
  .tv_usec = 100000
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


// for now, just ignore this
// FIXME something wrong here
//  idea, try to use dgram sockets
/* SocketListener_Async class */
SocketListener_Async::SocketListener_Async(){
  CallbackOnMessage = dumpcb1;
  CallbackOnConnected = dumpcb2;
  CallbackOnDisconnected = dumpcb2;

  _timewaitms = 1.f/DEFAULT_SOCKETCHECKRATE;
}

SocketListener_Async::~SocketListener_Async(){
  StopListening(true);
}

bool SocketListener_Async::_CheckForConnection(SOCKET s, int mode){
  FD_SET setr, setw, sete;
  FD_ZERO(&setr);
  FD_ZERO(&setw);
  FD_ZERO(&sete);

  switch(mode){
    break; case WS_SELECTREAD:
      FD_SET(s, &setr);
    
    break; case WS_SELECTWRITE:
      FD_SET(s, &setw);
    
    break; case WS_SELECTEXCEPT:
      FD_SET(s, &sete);
  }
  
  TIMEVAL _delay = _sock_disconnect_timeout;
  _delay.tv_usec = (int)round(_timewaitms*1000);

  int err = select(0, &setr, &setw, &sete, &_delay);
  printf("errsock %d\n", err);
  if(err > 0)
    return true;
  
  return false;
}

void SocketListener_Async::_CheckDataMessage(unsigned short msgcode, char* msgdata, int msglength, bool isSending){
  bool dofreedata = true;
  switch(msgcode){
    // do nothing
    // break; case PINGMESSAGE

    // handled outside this function
    // break; case EOBULKMESSAGE

    break; case SENDMESSAGE:
      if(!isSending)
        CallbackOnMessage(msgdata, msglength, &dofreedata);
      break;
    
    break; case CLSOCKMESSAGE:
      Disconnect();

    break; case CHECKRATE:
      _timewaitms = 1000.f/ *reinterpret_cast<int*>(msgdata);
  }

  if(dofreedata && msgdata != NULL)
    free(msgdata);
}

void SocketListener_Async::_ClearDatas(){
  {std::lock_guard<std::mutex> _lg(_datamutex);
    while(!_msgdataQueues.empty()){
      _msgcodeQueues.pop();
      auto _datapair = _msgdataQueues.front();
      free(_datapair.first);

      _msgdataQueues.pop();
    }
  }
}

void SocketListener_Async::_onConnected(){
  ChangeCheckRate(DEFAULT_SOCKETCHECKRATE);
  CallbackOnConnected();
}

void SocketListener_Async::_onDisconnected(){
  CallbackOnDisconnected();
}

void SocketListener_Async::_StartListening(){
  SOCKET listener = socket(
    _listeningAddress->ai_family,
    _listeningAddress->ai_socktype,
    _listeningAddress->ai_protocol
  );

  bind(listener, _listeningAddress->ai_addr, (int)_listeningAddress->ai_addrlen);
  listen(listener, 1);

  while(_stillListening){
    // polling the socket
    while(_stillListening){
      FD_SET set;
      FD_ZERO(&set);

      FD_SET(listener, &set);
      int err = select(0, &set, 0, 0, &_sock_timeout);
      if(FD_ISSET(listener, &set))
        break;
      
      if(err == SOCKET_ERROR){
        // TODO maybe a message here
        _stillListening = false;
        break;
      }
    }
    
    if(!_stillListening)
      break;
    
    SOCKET conn = accept(listener, NULL, NULL);
    if(conn == INVALID_SOCKET){
      // maybe a message here?
      continue;
    }

    printf("connected\n");

    _onConnected();
    _keepConnection = true;
    while(_keepConnection){
      // reading data from the sender
      while(true){
        if(!_CheckForConnection(conn, WS_SELECTREAD)){
          Disconnect();
          break;
        }

        unsigned short msgcode;
        int msglength = 0;
        char *msgdata = NULL;

        printf("recv\n");
        int err = recv(conn, reinterpret_cast<char*>(&msgcode), sizeof(unsigned short), 0);

        if(err == SOCKET_ERROR){
          printf("Error when receiving datas from remote host. Error code 0x%X\n", WSAGetLastError());
          Disconnect();
          break;
        }

        recv(conn, reinterpret_cast<char*>(msglength), sizeof(int), 0);
        if(msglength > 0){
          msgdata = (char*)malloc(msglength);
          recv(conn, msgdata, msglength, 0);
        }

        _CheckDataMessage(msgcode, msgdata, msglength, false);

        if(msgcode == EOBULKMSG || msgcode == PINGMESSAGE)
          break;
      }

      // sending data to the sender
      bool msgexist = false;
      for(int i = 0; _msgcodeQueues.size() > 0 || i < 1; i++){
        if(!_CheckForConnection(conn, WS_SELECTWRITE)){
          Disconnect();
          break;
        }

        unsigned short msgcode = PINGMESSAGE;
        int msglen = 0;
        char *msgdata = NULL;

        if(_msgcodeQueues.size() > 0){
          msgcode = _msgcodeQueues.front();
          msgdata = (char*)_msgdataQueues.front().first;
          msglen = _msgdataQueues.front().second;

          _msgcodeQueues.pop();
          _msgdataQueues.pop();
          msgexist = true;
        }

        printf("send\n");
        int err = send(conn, reinterpret_cast<char*>(&msgcode), sizeof(unsigned short), 0);
        
        if(err == SOCKET_ERROR){
          printf("Error on sending data to remote host. Error code: 0x%X\n", WSAGetLastError());
          Disconnect();
          break;
        }

        send(conn, reinterpret_cast<char*>(&msglen), sizeof(int), 0);
        if(msglen > 0)
          send(conn, msgdata, msglen, 0);
        
        _CheckDataMessage(msgcode, msgdata, msglen, true);

        if(_msgcodeQueues.size() == 0 && msgexist){
          printf("sending eobulkmsg\n");
          send(conn, reinterpret_cast<const char*>(&EOBULKMSG), sizeof(unsigned short), 0);

          int dummy = 0;
          send(conn, reinterpret_cast<char*>(&dummy), sizeof(int), 0);
        }
      }
    }

    shutdown(conn, SD_BOTH);
    closesocket(conn);
    _onDisconnected();
  }

  closesocket(listener);
}

void SocketListener_Async::_AppendData(unsigned short msgcode, std::pair<void*, int> msgdata){
  if(_keepConnection){
    std::lock_guard<std::mutex> _lg(_datamutex);
    _msgcodeQueues.push(msgcode);
    _msgdataQueues.push(msgdata);
  }
}

void SocketListener_Async::StartListening(unsigned short port, const char *ip, int checkratehz){
  if(!_stillListening){
    _keepConnection = true;
    _stillListening = true;

    struct addrinfo hints;
    ZeroMemory(&hints, sizeof(addrinfo));

    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;

    char *portstr = (char*)calloc(6, sizeof(char));
    sprintf(portstr, "%d", port);

    getaddrinfo(NULL, portstr, &hints, &_listeningAddress);

    _socketthread = std::thread(__StartListening, this);
  }
}

void SocketListener_Async::StopListening(bool doJoin){
  if(_stillListening){
    _stillListening = false;

    Disconnect();
    if(doJoin && _socketthread.joinable())
      _socketthread.join();
  }
}

void SocketListener_Async::Disconnect(){
  _AppendData(CLSOCKMESSAGE, {0, NULL});
  _keepConnection = false;
}

void SocketListener_Async::ChangeCheckRate(int checkratehz){
  int *_data = (int*)malloc(sizeof(int));
  *_data = checkratehz;
  _AppendData(CHECKRATE, {_data, sizeof(int)});
}

void SocketListener_Async::AppendData(void *data, int datalength){
  _AppendData(SENDMESSAGE, {data, datalength});
}



/* SocketHandler_Sync class */
SocketHandler_Sync::SocketHandler_Sync(unsigned short port, const char *ip){
  currSock = socket(AF_INET, SOCK_STREAM, 0);

  hostAddress = new sockaddr_in();
  hostAddress->sin_addr.S_un.S_addr = inet_addr(ip);
  hostAddress->sin_family = AF_INET;
  hostAddress->sin_port = htons(port);

  lastErrcode = connect(currSock, (sockaddr*)hostAddress, sizeof(*hostAddress));
}

SocketHandler_Sync::~SocketHandler_Sync(){
  SafeDelete();
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
    lastErrcode = send(currSock, reinterpret_cast<const char*>(&PINGMESSAGE), sizeof(unsigned short), 0);
    if(lastErrcode != SOCKET_ERROR){
      bool keepRecv = true;
      while(keepRecv){
        unsigned short msgcode = 0;
        lastErrcode = recv(currSock, reinterpret_cast<char*>(&msgcode), sizeof(unsigned short), 0);
        if(lastErrcode != SOCKET_ERROR){
          switch(msgcode){
            break; case PINGMESSAGE:
              keepRecv = false;
            
            break; case SENDMESSAGE:{
              int length = 0;
              char *msgdat;

              lastErrcode = recv(currSock, reinterpret_cast<char*>(&length), sizeof(length), 0);

              msgdat = new char[length];
              lastErrcode = recv(currSock, msgdat, length, 0);
              
              messageQueues.push(std::pair<char*, int>(msgdat, length));
            }

            break; case CLSOCKMESSAGE:{
              shutdown(currSock, SD_BOTH);
              lastErrcode = SOCKET_ERROR;
              keepRecv = false;
            }

            break; case EOBULKMSG:{
              keepRecv = false;
            }
          }
        }
      }
    }
  }
}

bool SocketHandler_Sync::IsConnected(){
  return lastErrcode != SOCKET_ERROR;
}

const auto _reconnectTimeDelay = std::chrono::milliseconds(100);
const int _reconnectTimes = 5;
void SocketHandler_Sync::ReconnectSocket(){
  if(stopSending || lastErrcode == SOCKET_ERROR);
    return;

  std::lock_guard<std::mutex> _lock(sMutex);
  for(int i = 0; i < _reconnectTimes && (lastErrcode = connect(currSock, (sockaddr*)hostAddress, sizeof(*hostAddress))) == SOCKET_ERROR; i++)
    std::this_thread::sleep_for(_reconnectTimeDelay);
}

void SocketHandler_Sync::SendData(const void *data, int datalength){
  if(stopSending || lastErrcode == SOCKET_ERROR)
    return;

  lastErrcode = send(currSock, reinterpret_cast<const char*>(&SENDMESSAGE), sizeof(SENDMESSAGE), 0);
  lastErrcode = send(currSock, reinterpret_cast<char*>(&datalength), sizeof(int), 0);
  lastErrcode = send(currSock, reinterpret_cast<const char*>(data), datalength, 0);
}

void SocketHandler_Sync::SafeDelete(){
  if(stopSending)
    return;

  std::lock_guard<std::mutex> _lock(sMutex);
  send(currSock, reinterpret_cast<const char*>(&CLSOCKMESSAGE), sizeof(unsigned short), 0);
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  shutdown(currSock, SD_BOTH);
  closesocket(currSock);
  stopSending = true;
  lastErrcode = SOCKET_ERROR;
  delete hostAddress;
}




/* SocketHandlerUDP_Async class */
void SocketHandlerUDP_Async::sleepThread(std::chrono::duration<double> pd){
  std::this_thread::sleep_for(sleepTime-pd);
}

void SocketHandlerUDP_Async::_QueueMessage(unsigned short code, void *msg, unsigned short msglen){
  if(_actuallyConnected){
    std::lock_guard<std::mutex> _lg(m_messageQueues);
    messageQueues.push(_message{
      .code = code,
      .msg = msg,
      .msglen = msglen
    });
  }
}

void SocketHandlerUDP_Async::on_udpdisconnect(){
  _actuallyConnected = false;

  while(messageQueues.size() > 0){
    delete messageQueues.front().msg;
    messageQueues.pop();
  }

  cbOnDisconnected();
  cv_disconnect.notify_all();
}

void SocketHandlerUDP_Async::on_udppollrate(unsigned char rateHz){
  sleepTime = std::chrono::milliseconds(1000/rateHz);
}

void SocketHandlerUDP_Async::on_udpmaxsize(unsigned short maxmsgsize){
  _maxmsgsize = maxmsgsize;
}

// at least buffer have length of 4
void getheadermsg(char* buffer, unsigned short code, void* msglen, int msglensize){
  memcpy(buffer, &code, sizeof(code));
  memcpy(buffer+sizeof(code), msglen, msglensize);
}

void SocketHandlerUDP_Async::udpthreadfunc(){
  printf("start udp\n");
  int _timesNotRespond = 0;
  while(_actuallyConnected){
    auto _stime = std::chrono::high_resolution_clock::now();

    // sending the message
    if(messageQueues.size() > 0){
      while(messageQueues.size() > 0){
        _message m;
        {std::lock_guard<std::mutex> _lg(m_messageQueues);
          m = messageQueues.front();
          if(m.msglen <= _maxmsgsize || _maxmsgsize == -1){

            // the header format should change based on the code
            char msghead[SIZEOFMSGHEADER];
            bool _useMsg = false;

            ByteIterator _bimsg{m.msg, (size_t)m.msglen};
            ByteIteratorR _bimsgheader{msghead, SIZEOFMSGHEADER};

            // this is just for what to do for certain msgcode
            switch(m.code){
              break; case UDPS_SETPOLLRATE:{
                unsigned char pollrate = 0; _bimsg.getVar(pollrate);
                on_udppollrate(pollrate);
                _useMsg = true;
              }

              break; case UDPS_MAXMSGSIZE:
                unsigned short msgsize = 0; _bimsg.getVar(msgsize);
                on_udpmaxsize(msgsize);
                _useMsg = true;
            }

            _bimsgheader.setVar(m.code);
            if(_useMsg)
              _bimsgheader.setVar(m.msg, m.msglen);
            else
              _bimsgheader.setVar(m.msglen);

            sendto(currSock, msghead, SIZEOFMSGHEADER, 0, (sockaddr*)&hostAddress, sizeof(hostAddress));

            if(!_useMsg && m.msg != NULL){
              int msgsent = 0;
              while(msgsent < m.msglen){
                int packetlen = std::min(m.msglen-msgsent, MAX_SOCKETPACKETSIZE);
                sendto(currSock, reinterpret_cast<char*>(m.msg)+msgsent, packetlen, 0, (sockaddr*)&hostAddress, sizeof(hostAddress));

                msgsent += packetlen;
              }
            }
          }

          if(m.msg != NULL)
            delete m.msg;
          messageQueues.pop();

          if(m.code == UDPS_DISCONNECTMSG){
            on_udpdisconnect();
            return;
          }
        }
      }
      
      sendto(currSock, reinterpret_cast<const char*>(&EOBULKMSG), sizeof(EOBULKMSG), 0, (sockaddr*)&hostAddress, sizeof(sockaddr_in)); 
    }
    else{
      sendto(currSock, reinterpret_cast<const char*>(&PINGMESSAGE), sizeof(PINGMESSAGE), 0, (sockaddr*)&hostAddress, sizeof(sockaddr_in));
    }

    char headercode[SIZEOFMSGHEADER];
    sockaddr_in curraddr;
    int sockaddr_inlen = sizeof(sockaddr_in);

    unsigned short msgcode = 0;

    // receiving the message
    bool keepRecv = true;
    while(keepRecv){
      int packetLength = 0;

      do{
        fd_set sockset;
        FD_ZERO(&sockset);
        FD_SET(currSock, &sockset);
        select(0, &sockset, NULL, NULL, &_sock_timeout);
        if(FD_ISSET(currSock, &sockset))
          packetLength = recvfrom(currSock, headercode, SIZEOFMSGHEADER, 0, (sockaddr*)&curraddr, &sockaddr_inlen);
        else if(++_timesNotRespond > MAXTIMESRESPUDP){
          on_udpdisconnect();
          return;
        }
      }
      while(packetLength <= 0 || curraddr != hostAddress);

      ByteIterator _bimsgheader{headercode, packetLength};

      _timesNotRespond = 0;
      _bimsgheader.getVar(msgcode);
      switch(msgcode){
        break; case UDPS_DISCONNECTMSG:
          on_udpdisconnect();
          return;

        break; case UDPS_SETPOLLRATE:{
          unsigned short pollrate = 0;
          _bimsgheader.getVar(pollrate);
          on_udppollrate(pollrate);
        }

        break; case UDPS_MAXMSGSIZE:{
          unsigned short msgsize = 0;
          _bimsgheader.getVar(msgsize);
          on_udpmaxsize(msgsize);
        }

        break; case SENDMESSAGE:{
          unsigned short msglen = 0;
          _bimsgheader.getVar(msglen);

          char *msgbuffer = new char[msglen];
          
          int msgread = 0;
          sockaddr_in curraddr;
          int curraddrlen = sizeof(sockaddr_in);

          while(msgread < msglen){
            fd_set sockset;
            FD_ZERO(&sockset);
            FD_SET(currSock, &sockset);

            select(0, &sockset, NULL, NULL, &_sock_timeout);

            packetLength = 0;
            if(FD_ISSET(currSock, &sockset)){
              packetLength = recvfrom(currSock, msgbuffer+msgread, MAX_SOCKETPACKETSIZE, 0, (sockaddr*)&curraddr, &curraddrlen);

              if(curraddr == hostAddress){
                _timesNotRespond = 0;
                msgread += packetLength;
              }
            }
            else if(++_timesNotRespond > MAXTIMESRESPUDP){
              on_udpdisconnect();
              delete msgbuffer;
              return;
            }

          }

          callback(msgbuffer, msglen);
          delete msgbuffer;
        }

        break;
        case PINGMESSAGE:
        case EOBULKMSG:
          keepRecv = false;
      }
    }

    auto __ftime = std::chrono::high_resolution_clock::now();
    sleepThread(__ftime - _stime);
  }
}


SocketHandlerUDP_Async::SocketHandlerUDP_Async(){
  sleepTime = std::chrono::milliseconds(1000/UDPS_DEFAULTPOLLRATE);

  hostAddress = sockaddr_in();
  callback = dumpfunc1;
  cbOnDisconnected = dumpfunc2;
}

SocketHandlerUDP_Async::~SocketHandlerUDP_Async(){
  Disconnect();
}

bool SocketHandlerUDP_Async::StartConnecting(const char *ipaddress, unsigned short port){
  if(!_actuallyConnected){
    printf("Trying to connect....\n");
    hostAddress.sin_addr.S_un.S_addr = inet_addr(ipaddress);
    hostAddress.sin_family = AF_INET;
    hostAddress.sin_port = htons(port);
    currSock = socket(AF_INET, SOCK_DGRAM, 0);

    bool connected = false;
    for(int i = 0; i < 5; i++){
      printf("tries: %d\n", i);
      auto _stime = std::chrono::high_resolution_clock::now();

      int sockaddrlen = sizeof(sockaddr_in);
      sendto(currSock, reinterpret_cast<const char*>(&UDPS_CONNECTMSG), sizeof(unsigned short), 0, (sockaddr*)&hostAddress, sockaddrlen);

      unsigned short message = 0;
      sockaddr_in curraddr;
      int curraddrlen = sizeof(sockaddr_in);
      recvfrom(currSock, reinterpret_cast<char*>(&message), sizeof(unsigned short), 0, (sockaddr*)&curraddr, &curraddrlen);

      if(curraddr != hostAddress && message != UDPS_CONNECTMSG){
        auto _fitime = std::chrono::high_resolution_clock::now();
        sleepThread(_fitime - _stime);
      }
      else{
        _actuallyConnected = true;
        connected = true;
        break;
      }
    }

    if(connected)
      udpthread = std::thread(_udpthreadfunc, this);

    return connected;
  }

  return false;
}

void SocketHandlerUDP_Async::QueueMessage(char* msg, int len){
  if(_actuallyConnected){
    _QueueMessage(SENDMESSAGE, msg, len);
  }
}

// the poll rate sets after it the message has been sent
void SocketHandlerUDP_Async::ChangePollrate(unsigned char rateHz){
  if(_actuallyConnected){
    unsigned char *ppr = new unsigned char(rateHz);
    _QueueMessage(UDPS_SETPOLLRATE, ppr, sizeof(unsigned char));
  }
}

void SocketHandlerUDP_Async::SetCallback(SocketCallback cb){
  callback = cb;
}

void SocketHandlerUDP_Async::SetCallback_disconnect(SocketCallbackvoid cb){
  cbOnDisconnected = cb;
}

void SocketHandlerUDP_Async::WaitUntilDisconnect(){
  if(_actuallyConnected){
    std::unique_lock<std::mutex> lkDc{m_onDisconnect};
    cv_disconnect.wait(lkDc);
    printf("done waiting.\n");
  }
}

// the reason why it needs to queue, not full force, is to send all the data first, then disconnect
void SocketHandlerUDP_Async::Disconnect(){
  _QueueMessage(UDPS_DISCONNECTMSG);
}