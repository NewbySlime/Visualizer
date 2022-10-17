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
const unsigned short
  UDPS_MSG = 0x7000,
  UDPS_CONNECTMSG = UDPS_MSG + 0x1,
  UDPS_DISCONNECTMSG = UDPS_MSG + 0x2,
  UDPS_PINGMESSAGEDATA = UDPS_MSG + 0x10,
  UDPS_PACKETRECEIVED = UDPS_MSG + 0x11,
  UDPS_RESENDPACKET = UDPS_MSG + 0x12,
  UDPS_SENDPACKETS = UDPS_MSG + 0x13
;

// acknowledgement for each sending
const unsigned char
  UDPS_ACK_OK = 0x00,
  UDPS_ACK_RESENDPCK = 0x01,
  UDPS_ACK_RESENDHEAD = 0x02
;


const IPAddress NulIP{0};

void clearmem(char* mem, int len){
  for(int i = 0; i < len; i++)
    mem[i] = 0;
}

// if it's above the max msg size, it will try not to set to above that number
// if it's below, then it will be allowed

void SocketUDP::_onUdpConnect(){
  remoteIP = wifiudp.remoteIP();
  remotePort = wifiudp.remotePort();

  _responded = true;
  _alreadyconnected = true;

  OnConnected();
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

  OnDisconnected();
}

void SocketUDP::_onUdpClose(){
  _keeplistening = false;
  wifiudp.stop();
}

void SocketUDP::_onNotResponded(){
  if(_responded){
    _responded = false;
    _lasttimeresp = millis();
  }

  unsigned long _deltatime = millis() >= _lasttimeresp?
    millis() - _lasttimeresp:
    0xFFFFFFFFUL - _lasttimeresp + millis();

  if(_deltatime >= MAXWAITTIMEUDPMS)
    _onUdpDisconnect();
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
  buffer_send = new char[MAX_UDPMSGLEN];
  buffer_recv = new char[MAX_UDPMSGRECVLEN];
  resendpackets_send = new char[_UDPMSGPACKETS/8+((_UDPMSGPACKETS%8) > 0)+1];
  resendpackets_recv = new char[_UDPMSGPACKETS/8+((_UDPMSGPACKETS%8) > 0)+1];
  appendedMessages.reserve(MAX_DATAQUEUE);
}

SocketUDP::~SocketUDP(){
  close_socket();

  // waiting until done
  while(_keeplistening)
    update_socket();

  delete[] buffer_send;
  delete[] buffer_recv;
  delete[] resendpackets_send;
  delete[] resendpackets_recv;
}

void SocketUDP::startlistening(unsigned short port){
  wifiudp.begin(port);
  _keeplistening = true;
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

void SocketUDP::set_callback(SocketUDP_Callback cb){
  callbackOnPacket = cb;
}

void SocketUDP::queue_data(char *data, unsigned short datalength){
  _queueMessage(SENDMESSAGE, data, datalength);
}

void SocketUDP::update_socket(){
  if(_keeplistening){
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
      _responded = true;

      if(!_recvMsg && !_sendMsg){
        uint16_t pcode = 0;
        wifiudp.read(reinterpret_cast<char*>(&pcode), sizeof(uint16_t));
        _recvMsg = pcode == UDPS_PINGMESSAGEDATA;

        if(!_recvMsg && pcode != PINGMESSAGE)
          return;

        _sendMsg = appendedMessages.size() > 0;
        uint16_t pingmsg = _sendMsg? UDPS_PINGMESSAGEDATA: PINGMESSAGE;
        wifiudp.beginPacket(remoteIP, remotePort);
        wifiudp.write(reinterpret_cast<char*>(&pingmsg), sizeof(uint16_t));
        wifiudp.endPacket();
      }

      
      if(_recvMsg){
        switch(_rstate){
          break; case _waiting_packets:{

          }

          break; case _recv_state::_done:{

          }
        }
      }

      if(_sendMsg){
        switch(_sstate){
          break; case _sending_packets:{

          }

          break; case _send_state::_done:{

          }
        }
      }
    }
    else if(_alreadyconnected)
      _onNotResponded();

    if(!_alreadyconnected && _doCloseSocket)
      _onUdpClose();
  }
}


SocketUDP udpSock;