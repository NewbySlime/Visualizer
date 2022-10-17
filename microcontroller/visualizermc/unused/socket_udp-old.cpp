#include "socket_udp.hpp"
#include "ESP8266WiFi.h"
#include "WiFiUdp.h"
#include "ByteIterator.hpp"

#include "timer.hpp"

#define _timer1_MaxValue 0x7FFFFF
#define _S_IN_US 1000000


/** Socket message format
 * Message always starts with message code
 * If msg code is SENDMESSAGE then after it should be the size of the message (not packet)
 * 
 * If message size is above the maximum packet size, the next packet (after the first one),
 * will not define message size in front of the packet
*/


const int MAXTIMESRESPUDP = 32;
const int SIZEOFMSGHEADER = 4;

const unsigned short PINGMESSAGE = 0x1;
const unsigned short SENDMESSAGE = 0x2;
//const unsigned short CLSOCKMESSAGE = 0x3;
// CLSOCKMESSAGE is replaced by UDPS_CONNECTMSG and UDPS_DISCONNECTMSG
const unsigned short EOBULKMESSAGE = 0x4;



// messages for UDP sockets

// template UDP messages
const unsigned short UDPS_MSG = 0x7000;
const unsigned short UDPS_CONNECTMSG = UDPS_MSG + 0x1;
const unsigned short UDPS_DISCONNECTMSG = UDPS_MSG + 0x2;
const unsigned short UDPS_SETPOLLRATE = UDPS_MSG + 0x3;
const unsigned short UDPS_SETMAXMSGSIZE = UDPS_MSG + 0x5;


const IPAddress NulIP{0};



void SocketUDP::_onTimer(void *pobj){
  ((SocketUDP*)pobj)->_onTimer();
}

void SocketUDP::_set_pollingrate(unsigned char rateHz){
  _intervalms = 1000/rateHz;
  if(_isrtimer_num >= 0){
    timer_changeInterval(_isrtimer_num, _intervalms);
  }
}

// if it's above the max msg size, it will try not to set to above that number
// if it's below, then it will be allowed
void SocketUDP::_set_maxmsgsize(unsigned short maxmsgsize){
  if(maxmsgsize > MAX_UDPMSGLEN || maxmsgsize == -1)
    set_msgmaxsize(MAX_UDPMSGLEN);
  else
    _maxmsgsize = maxmsgsize;
}

void SocketUDP::_onTimer(){
  int packetLength = wifiudp.parsePacket();
  // if it needs a connection first
  if(!_alreadyconnected && packetLength > 0){
    wifiudp.read(msgheader, SIZEOFMSGHEADER);
    unsigned short msgcode = *reinterpret_cast<unsigned short*>(msgheader);
    if(msgcode == UDPS_CONNECTMSG){
      Serial.printf("Connected.\n");
      _onUdpConnect();

      wifiudp.beginPacket(remoteIP, remotePort);
      wifiudp.write(reinterpret_cast<const char*>(&UDPS_CONNECTMSG), sizeof(UDPS_CONNECTMSG));
      wifiudp.endPacket();
    }
    
    return;
  }

  //while(packetLength > 0 && !_checkIfSameRemoteHost())
  //  packetLength = wifiudp.parsePacket();

  if(packetLength > 0 && _checkIfSameRemoteHost()){
    _timesNotRespond = 0;
    unsigned short msgcode = 0;
    
    // reading the message
    bool keepRecv = true;
    while(keepRecv && packetLength > 0){
      wifiudp.read(msgheader, SIZEOFMSGHEADER);
      msgcode = *reinterpret_cast<unsigned short*>(msgheader);

      switch(msgcode){
        break; case UDPS_SETPOLLRATE:
          _set_pollingrate(*reinterpret_cast<unsigned char*>(msgheader+sizeof(msgcode)));
        
        break; case UDPS_SETMAXMSGSIZE:
          _set_maxmsgsize(*reinterpret_cast<unsigned short*>(msgheader+sizeof(msgcode)));
        
        break; case UDPS_DISCONNECTMSG:
          _onUdpDisconnect();
          return;

        break; case SENDMESSAGE:{
          int msgread = 0;
          unsigned short msglen = *reinterpret_cast<unsigned short*>(msgheader+sizeof(msgcode));
          //Serial.printf("message length: %d\n", msglen);
          bool dontread = false;
          
          if(msglen > MAX_UDPMSGLEN)
            dontread = true;

          while(msgread < msglen){
            packetLength = _parsePacketsUntilCorrect();

            if(packetLength <= 0)
              break;

            if(!dontread){
              packetLength = min(packetLength, msglen-msgread);
              wifiudp.read(buffer+msgread, packetLength);
            }

            msgread += packetLength;
          }

          if(!dontread)
            callbackOnPacket(buffer, msgread);
        }

        break;
        case EOBULKMESSAGE:
        //Serial.printf("end of messages\n");
        case PINGMESSAGE:
          keepRecv = false;
      }

      packetLength = _parsePacketsUntilCorrect();
    }


    // sending the message
    if(appendedMessages.size() > 0){
      for(size_t i = 0; i < appendedMessages.size(); i++){
        _message msgdat = appendedMessages[i];
        printf("sending msgdat 0x%X\n", msgdat.msgcode);
        
        bool _useMsg = false;
        switch(msgdat.msgcode){
          break; case UDPS_SETPOLLRATE:
            _set_pollingrate(*reinterpret_cast<unsigned char*>(msgdat.buffer));
            _useMsg = true;
          
          break; case UDPS_SETMAXMSGSIZE:
            _set_maxmsgsize(*reinterpret_cast<unsigned short*>(msgdat.buffer));
            _useMsg = true;
        }

        ByteIteratorR _bir{msgheader, SIZEOFMSGHEADER};   
        _bir.setVar(msgdat.msgcode);

        if(_useMsg)
          _bir.setVar(msgdat.buffer, msgdat.bufferlen);
        else // TODO what to sent if msgdat.buffer is NULL
          _bir.setVar(msgdat.bufferlen);
        
        wifiudp.beginPacket(remoteIP, remotePort);
        wifiudp.write(msgheader, SIZEOFMSGHEADER);
        wifiudp.endPacket();

        if(!_useMsg && msgdat.buffer != NULL){
          int msgsent = 0;
          while(msgsent < msgdat.bufferlen){
            int packetlen = min(msgdat.bufferlen-msgsent, MAX_UDPPACKETLEN);
            wifiudp.beginPacket(remoteIP, remotePort);
            wifiudp.write(reinterpret_cast<char*>(msgdat.buffer)+msgsent, packetlen);
            wifiudp.endPacket();

            msgsent += packetlen;
          }
        }

        if(msgdat.buffer != NULL){
          free(msgdat.buffer);
          msgdat.buffer = NULL;
        }

        if(msgdat.msgcode == UDPS_DISCONNECTMSG){
          _onUdpDisconnect();
          return;
        }
      }

      appendedMessages.resize(0);
      
      wifiudp.beginPacket(remoteIP, remotePort);
      wifiudp.write(reinterpret_cast<const char*>(&EOBULKMESSAGE), sizeof(EOBULKMESSAGE));
      wifiudp.endPacket();
    }
    else{
      wifiudp.beginPacket(remoteIP, remotePort);
      wifiudp.write(reinterpret_cast<const char*>(&PINGMESSAGE), sizeof(PINGMESSAGE));
      wifiudp.endPacket();
    }
  }
  else if(_alreadyconnected && ++_timesNotRespond > MAXTIMESRESPUDP){
    _timesNotRespond = 0;
    _onUdpDisconnect();
  }

  if(!_alreadyconnected && _doCloseSocket)
    _onUdpClose();
}

void SocketUDP::_onUdpConnect(){
  remoteIP = wifiudp.remoteIP();
  remotePort = wifiudp.remotePort();

  _alreadyconnected = true;

  set_pollingrate(UDPS_DEFAULTPOLLRATE);
  set_msgmaxsize(MAX_UDPMSGLEN);
}

void SocketUDP::_onUdpDisconnect(){
  remoteIP = NulIP;
  remotePort = 0;
  printf("disconnected.\n");
  _alreadyconnected = false;

  for(size_t i = 0; i < appendedMessages.size(); i++)
    if(appendedMessages[i].buffer != NULL)
      free(appendedMessages[i].buffer);

  appendedMessages.resize(0);
}

void SocketUDP::_onUdpClose(){
  timer_deleteTimer(_isrtimer_num);
  _isrtimer_num = -1;
  delete[] buffer;
  delete[] msgheader;
  wifiudp.stop();
}

void SocketUDP::_queueMessage(unsigned short msgcode, void* msg, unsigned short msglen){
  if(_alreadyconnected)
    appendedMessages.push_back(_message{
      .msgcode = msgcode,
      .buffer = msg,
      .bufferlen = msglen
    });
  else if(msg != NULL)
    free(msg);
}

bool SocketUDP::_checkIfSameRemoteHost(){
  return wifiudp.remoteIP() == remoteIP && wifiudp.remotePort() == remotePort;
}

int SocketUDP::_parsePacketsUntilCorrect(){
  int packetLength = 0;
  do
    packetLength = wifiudp.parsePacket();
  while(packetLength > 0 && !_checkIfSameRemoteHost());

  return packetLength;
}



SocketUDP::SocketUDP(){
  _set_pollingrate(UDPS_DEFAULTPOLLRATE);
  appendedMessages.reserve(MAX_DATAQUEUE);
}

SocketUDP::~SocketUDP(){
  close_socket();
}

void SocketUDP::startlistening(unsigned short port){
  wifiudp.begin(port);
  buffer = new char[MAX_UDPMSGLEN];
  msgheader = new char[SIZEOFMSGHEADER];

  _isrtimer_num = timer_setInterval(_intervalms, _onTimer, this);
}

void SocketUDP::disconnect(){
  appendedMessages.push_back(_message{
    .msgcode = UDPS_DISCONNECTMSG,
    .buffer = NULL,
    .bufferlen = 0
  });
}

void SocketUDP::close_socket(){
  disconnect();
  _doCloseSocket = true;
}


bool SocketUDP::isconnected(){
  return _alreadyconnected;
}

void SocketUDP::set_pollingrate(unsigned char rateHz){
  unsigned char *prhz = (unsigned char*)malloc(sizeof(unsigned char));
  *prhz = rateHz;
  _queueMessage(UDPS_SETPOLLRATE, prhz, sizeof(rateHz));
}

void SocketUDP::set_msgmaxsize(unsigned short msglen){
  if(msglen <= MAX_UDPMSGLEN){
    unsigned short *pml = (unsigned short*)malloc(sizeof(unsigned short));
    *pml = msglen;
    _queueMessage(UDPS_SETMAXMSGSIZE, pml, sizeof(msglen));
  }
}

void SocketUDP::set_callback(SocketUDP_Callback cb){
  callbackOnPacket = cb;
}

void SocketUDP::queue_data(char *data, unsigned short datalength){
  _queueMessage(SENDMESSAGE, data, datalength);
}


SocketUDP udpSock;