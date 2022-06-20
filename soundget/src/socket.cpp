#include "socket.hpp"
#include "initquit.hpp"

#include "stdio.h"

#include "cmath"
#include "thread"
#include "chrono"



// if the socket isn't connected n many times, the socket will stop updating
const int MAXTIMESRESPUDP = 5;
const int SIZEOFMSGHEADER = 4;


const unsigned short PINGMESSAGE = 0x1;
const unsigned short SENDMESSAGE = 0x2;
const unsigned short CLSOCKMESSAGE = 0x3;
const unsigned short EOBULKMSG = 0x4;


// messages for UDP sockets
const unsigned short UDPS_MSG = 0x7000;
const unsigned short UDPS_CONNECTMSG = UDPS_MSG + 0x1;
const unsigned short UDPS_DISCONNECTMSG = UDPS_MSG + 0x2;
const unsigned short UDPS_SETPOLLRATE = UDPS_MSG + 0x3;
const unsigned short UDPS_MAXMSGSIZE = UDPS_MSG + 0x5;



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

  printf("listening done.\n");
  cv_disconnect.notify_all();
}

void SocketHandlerUDP_Async::on_udppollrate(unsigned char rateHz){
  sleepTime = std::chrono::milliseconds(1000/rateHz);
}

void SocketHandlerUDP_Async::on_udpmaxsize(unsigned short maxmsgsize){
  _maxmsgsize = maxmsgsize;
  printf("max msg size set, %d\n", maxmsgsize);
}

// at least buffer have length of 4
void getheadermsg(char* buffer, unsigned short code, void* msglen, int msglensize){
  memcpy(buffer, &code, sizeof(code));
  memcpy(buffer+sizeof(code), msglen, msglensize);
}

// FIXME the thread won't close, need to fix on what to do when the remote host isn't responding
void SocketHandlerUDP_Async::udpthreadfunc(){
  int _timesNotRespond = 0;
  while(_actuallyConnected){
    auto _stime = std::chrono::high_resolution_clock::now();

    // sending the message
    if(messageQueues.size() > 0){
      printf("sending message.\n");
      while(messageQueues.size() > 0){
        printf("Message size: %d\n", messageQueues.size());
        _message m;
        {std::lock_guard<std::mutex> _lg(m_messageQueues);
          m = messageQueues.front();
          if(m.msglen <= _maxmsgsize || _maxmsgsize == -1){
            printf("msglen: %d\n", m.msglen);
            // the header format should change based on the code
            char msghead[SIZEOFMSGHEADER];
            bool _useMsg = false;

            // this is just for what to do for certain msgcode
            switch(m.code){
              break; case UDPS_SETPOLLRATE:
                on_udppollrate(*reinterpret_cast<unsigned char*>(m.msg));
                _useMsg = true;
              break; case UDPS_MAXMSGSIZE:
                on_udpmaxsize(*reinterpret_cast<unsigned short*>(m.msg));
                _useMsg = true;
            }


            if(_useMsg)
              getheadermsg(msghead, m.code, m.msg, m.msglen);
            else
              getheadermsg(msghead, m.code, &m.msglen, sizeof(m.msglen));

            sendto(currSock, msghead, SIZEOFMSGHEADER, 0, (sockaddr*)&hostAddress, sizeof(hostAddress));

            if(m.msg != NULL){
              int msgsent = 0;
              while(msgsent < m.msglen){
                int packetlen = std::min(m.msglen-msgsent, MAX_SOCKETPACKETSIZE);
                sendto(currSock, reinterpret_cast<char*>(m.msg+msgsent), packetlen, 0, (sockaddr*)&hostAddress, sizeof(hostAddress));

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

      printf("End of bulk message.\n");
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
      int packetLength = recvfrom(currSock, headercode, SIZEOFMSGHEADER, 0, (sockaddr*)&curraddr, &sockaddr_inlen);

      _timesNotRespond = 0;
      while(packetLength <= 0 && curraddr != hostAddress){
        recvfrom(currSock, headercode, SIZEOFMSGHEADER, 0, (sockaddr*)&curraddr, &sockaddr_inlen);

        if(++_timesNotRespond > MAXTIMESRESPUDP){
          on_udpdisconnect();
          return;
        }
      }

      _timesNotRespond = 0;
      msgcode = *reinterpret_cast<unsigned short*>(headercode);
      switch(msgcode){
        break; case UDPS_DISCONNECTMSG:
          on_udpdisconnect();
          return;

        break; case UDPS_SETPOLLRATE:{
          on_udppollrate(*reinterpret_cast<unsigned char*>(headercode+sizeof(unsigned short)));
        }

        break; case UDPS_MAXMSGSIZE:{
          on_udpmaxsize(*reinterpret_cast<unsigned short*>(headercode+sizeof(unsigned short)));
        }

        break; case SENDMESSAGE:{
          unsigned short msglen = *reinterpret_cast<unsigned short*>(headercode+sizeof(unsigned short));
          char *msgbuffer = new char[msglen];
          char buffer[MAX_SOCKETPACKETSIZE];
          
          int msgread = 0;
          sockaddr_in curraddr;
          int curraddrlen = sizeof(sockaddr_in);

          while(msgread < msglen){
            int packetlen = recvfrom(currSock, buffer, MAX_SOCKETPACKETSIZE, 0, (sockaddr*)&curraddr, &curraddrlen);

            if(curraddr != hostAddress){
              if(++_timesNotRespond > MAXTIMESRESPUDP){
                on_udpdisconnect();
                delete msgbuffer;
                return;
              }

              continue;
            }

            _timesNotRespond = 0;
            memcpy(msgbuffer+msgread, buffer, packetlen);
            msgread += packetlen;
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
    printf("queuing message\n");

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