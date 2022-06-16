#include "socket_udp.hpp"
#include "ESP8266WiFi.h"
#include "WiFiUdp.h"

#include "timer.hpp"

#include "vector"

#define _timer1_MaxValue 0x7FFFFF
#define _S_IN_US 1000000


/** Socket message format
 * Message always starts with message code
 * If msg code is SENDMESSAGE then after it should be the size of the message (not packet)
 * 
 * If message size is above the maximum packet size, the next packet (after the first one),
 * will not define message size in front of the packet
*/


const int MAXTIMESRESPUDP = 5;
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
const unsigned short UDPS_MSGACCEPTED = UDPS_MSG + 0x4;
const unsigned short UDPS_SETMAXMSGSIZE = UDPS_MSG + 0x5;


const IPAddress NulIP{0};


void SocketUDP::_onTimer(void *pobj){
  ((SocketUDP*)pobj)->_onTimer();
}


void SocketUDP::_set_pollingrate(unsigned char rateHz){
  _intervalms = 1000/rateHz;
  if(_isrtimer_num >= 0){
    ISR_Timer.changeInterval(_isrtimer_num, _intervalms);
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

// TODO: change the code based on the sender one
// TODO: should test whether the sender is the right one
// TODO: add a checking for if the remote is going unexpectedly disconnected
void SocketUDP::_onTimer(){
  int packetLength = wifiudp.parsePacket();
  // if it needs a connection first
  if(!_alreadyconnected){
    wifiudp.read(msgheader, SIZEOFMSGHEADER);
    unsigned short msgcode = *reinterpret_cast<unsigned short*>(msgheader);
    if(msgcode == UDPS_CONNECTMSG)
      _onUdpConnect(wifiudp.remoteIP(), wifiudp.remotePort());
    
    // parse another packet or message
    packetLength = wifiudp.parsePacket();
  }

  while(packetLength > 0 && !_checkIfSameRemoteHost()){
    wifiudp.flush();
    packetLength = wifiudp.parsePacket();
  }

  if(packetLength > 0){
    unsigned short msgcode = 0;
    
    // reading the message
    while(msgcode != EOBULKMESSAGE){
      wifiudp.read(msgheader, SIZEOFMSGHEADER);
      msgcode = *reinterpret_cast<unsigned short*>(msgheader);

      // TODO check based on the msgcode
      switch(msgcode){
        break; case UDPS_CONNECTMSG:
          if(!_alreadyconnected)
            _onUdpConnect();
      }
    }
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
  _alreadyconnected = false;

  for(int i = 0; i < appendedMessages.size(); i++)
    if(appendedMessages[i].buffer != NULL)
      delete appendedMessages[i].buffer;

  appendedMessages.resize(0);
}

void SocketUDP::_onUdpClose(){
  ISR_Timer.deleteTimer(_isrtimer_num);
  _isrtimer_num = -1;
  delete buffer;
  delete msgheader;
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
   delete msg;
}

bool SocketUDP::_checkIfSameRemoteHost(){
  return wifiudp.remoteIP == remoteIP && wifiudp.remotePort() == remotePort;
}



SocketUDP::SocketUDP(){
  _set_pollingrate(UDPS_DEFAULTPOLLRATE);
  appendedMessages.reserve(MAX_DATAQUEUE);
}

void SocketUDP::startlistening(unsigned short port){
  wifiudp.begin(port);
  buffer = new char[MAX_UDPPACKETLEN];
  msgheader = new char[SIZEOFMSGHEADER];

  _isrtimer_num = ISR_Timer.setInterval(_intervalms, _onTimer, this);
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
  unsigned char *prhz = new unsigned char(rateHz);
  _queueMessage(UDPS_SETPOLLRATE, prhz, sizeof(rateHz));
}

void SocketUDP::set_callback(SocketUDP_Callback cb){
  callbackOnPacket = cb;
}

void SocketUDP::queue_data(char *data, unsigned short datalength){
  _queueMessage(SENDMESSAGE, data, datalength);
}


SocketUDP udpSock;