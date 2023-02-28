#include "socket_udp.hpp"
#include "ESP8266WiFi.h"
#include "WiFiClient.h"
#include "WiFiServer.h"
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


const int MAXWAITTIMEUDPMS = 10000;
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
const unsigned short UDPS_PINGMESSAGEDATA = UDPS_MSG + 0x10;
const unsigned short UDPS_PACKETRECEIVED = UDPS_MSG + 0x11;


const IPAddress NulIP{0};

void clearmem(char* mem, int len){
  for(int i = 0; i < len; i++)
    mem[i] = 0;
}

void SocketUDP::_onTimer(void *pobj){
  ((SocketUDP*)pobj)->_onTimer();
}

void SocketUDP::_set_pollingrate(unsigned char rateHz){
  _intervalms = 1000/rateHz;
  _maxTimesRespond = MAXWAITTIMEUDPMS/_intervalms;
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
    unsigned short msgcode = 0;
    wifiudp.read(reinterpret_cast<char*>(&msgcode), sizeof(uint16_t));
    if(msgcode == UDPS_CONNECTMSG){
      Serial.printf("Connected.\n");
      _onUdpConnect();

      wifiudp.beginPacket(remoteIP, remotePort);
      wifiudp.write(reinterpret_cast<const char*>(&UDPS_CONNECTMSG), sizeof(UDPS_CONNECTMSG));
      wifiudp.endPacket();
    }
    
    return;
  }

  while(packetLength > 0 && !_checkIfSameRemoteHost())
    packetLength = wifiudp.parsePacket();
  
  if(packetLength > 0){
    _timesNotRespond = 0;

    uint16_t pcode = 0;
    wifiudp.read(reinterpret_cast<char*>(&pcode), sizeof(uint16_t));
    //Serial.printf("pcode 0x%X\n", pcode);
    bool _recvMsg = pcode == UDPS_PINGMESSAGEDATA;

    bool _sendMsg = appendedMessages.size() > 0;
    uint16_t pingmsg = _sendMsg? UDPS_PINGMESSAGEDATA: PINGMESSAGE;
    wifiudp.beginPacket(remoteIP, remotePort);
    wifiudp.write(reinterpret_cast<char*>(&pingmsg), sizeof(uint16_t));
    wifiudp.endPacket();

    if(!_recvMsg && pcode != PINGMESSAGE){
      Serial.printf("ec %X\n", pcode);
      return;
    }

    if(_recvMsg){      
      bool keepRecv = true;
      while(keepRecv){
        packetLength = 0;
        while(packetLength <= 0)
          packetLength = _parsePacketsUntilCorrect();

        /*
        if(packetLength <= 0){
          _timesNotRespond++;
          return;
        }
        */

        clearmem(msgheader, SIZEOFMSGHEADER);
        wifiudp.read(msgheader, SIZEOFMSGHEADER);
        ByteIterator _bihead{msgheader, SIZEOFMSGHEADER};
        uint16_t msgcode = 0; _bihead.getVar(msgcode);

        switch(msgcode){
          break; case UDPS_SETPOLLRATE:{
            uint8_t prate = 0; _bihead.getVar(prate);
            _set_pollingrate(prate);
          }

          break; case UDPS_SETMAXMSGSIZE:{
            uint16_t mmsg = 0; _bihead.getVar(mmsg);
            _set_maxmsgsize(mmsg);
          }

          break; case UDPS_DISCONNECTMSG:{
            _onUdpDisconnect();
            return;
          }

          break; case SENDMESSAGE:{
            uint16_t msglen = 0; _bihead.getVar(msglen);
            int msgrecv = 0;
            //Serial.printf("message length: %d\n", msglen);

            if(msglen > MAX_UDPMSGLEN)
              break;

            while(msgrecv < msglen){
              int plen = _parsePacketsUntilCorrect();

              /*
              if(plen <= 0)
                break;
              */
              
              plen = min(plen, msglen-msgrecv);
              wifiudp.read(buffer+msgrecv, plen);
              msgrecv += plen;
            }

            callbackOnPacket(buffer, msgrecv);
          }

          break; case EOBULKMESSAGE:
            Serial.printf("end of messages\n");
            keepRecv = false;
        }
      }
    }

    if(_sendMsg){
      for(size_t i = 0; i < appendedMessages.size(); i++){
        auto msgdat = appendedMessages[i];
        ByteIterator _bihead{(char*)(msgdat.buffer), msgdat.bufferlen};
        
        bool _useMsg = false;
        switch(msgdat.msgcode){
          break; case UDPS_SETPOLLRATE:{
            uint8_t prate = 0; _bihead.getVar(prate); 
            _set_pollingrate(prate);
            _useMsg = true;
          }

          break; case UDPS_SETMAXMSGSIZE:{
            uint16_t mmsg = 0; _bihead.getVar(mmsg);
            _set_maxmsgsize(mmsg);
            _useMsg = true;
          }
        }

        ByteIteratorR _bir{msgheader, SIZEOFMSGHEADER};
        _bir.setVar(msgdat.msgcode);

        if(_useMsg)
          _bir.setVar(msgdat.buffer, msgdat.bufferlen);
        else
          _bir.setVar(msgdat.bufferlen);

        wifiudp.beginPacket(remoteIP, remotePort);
        wifiudp.write(msgheader, SIZEOFMSGHEADER);
        wifiudp.endPacket();


        if(!_useMsg && msgdat.buffer){
          int msgsent = 0;
          while(msgsent < msgdat.bufferlen){
            int plen = min(msgdat.bufferlen-msgsent, MAX_UDPPACKETLEN);
            wifiudp.beginPacket(remoteIP, remotePort);
            wifiudp.write(reinterpret_cast<char*>(msgdat.buffer)+msgsent, plen);
            wifiudp.endPacket();

            msgsent += plen;
          }
        }

        if(msgdat.buffer)
          free(msgdat.buffer);
        
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
  }
  else if(_alreadyconnected && ++_timesNotRespond > _maxTimesRespond){
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
  int packetLength = wifiudp.parsePacket();
  while(packetLength > 0 && !_checkIfSameRemoteHost())
    packetLength = wifiudp.parsePacket();

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