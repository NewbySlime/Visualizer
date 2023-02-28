#include "server.hpp"
#include "polling.h"
#include "ByteIterator.hpp"
#include "vector"

const unsigned short
  PINGMESSAGE = 0x1,
  SENDMESSAGE = 0x2,
  CLSOCKMESSAGE = 0x3
;


void __s_dmp1(const char*, int){}
void __s_dmp2(sock_connstate){}

S_Cb_onconn cb_connstate = __s_dmp2;
S_Cb_ondata cb_data = __s_dmp1;

char _buf[HEADER_DATALEN+MAX_DATALEN];
WiFiServer *wserv = NULL;
WiFiClient wclient;

std::vector<std::pair<uint16_t,std::pair<char*,int>>> datas;


bool serv_initiated = false;
bool _updateSock = false;

void _socket_update();
void _onsocket_connected();
void _onsocket_disconnect();
void _server_update(void*);


void server_init(uint16_t port){
  if(!serv_initiated){
    serv_initiated = true;
    datas.reserve(MAX_DATAQUEUE);
    wserv = new WiFiServer(port);
    wserv->begin();

    polling_addfunc(_server_update, NULL);
  }
}

void server_update(){
  if(serv_initiated){
    if(!_updateSock && wserv->hasClient()){
      Serial.printf("test\n");
      wclient = wserv->available();
      wclient.setNoDelay(true);
      _onsocket_connected();
    }

    if(_updateSock)
      _socket_update();
  }
}

void _server_update(void*){server_update();}

void _socket_queuedata(uint16_t code, char *data, int datalen);
void socket_queuedata(char *data, int datalen){
  if(wclient.connected())
    _socket_queuedata(SENDMESSAGE, data, datalen);
  else if(data)
    free(data);
}

void socket_disconnect(){
  if(wclient.connected())
    _socket_queuedata(CLSOCKMESSAGE, NULL, 0);
}

void set_socketcallback(S_Cb_ondata cb){
  cb_data = cb;
}

void set_socketconnstateCb(S_Cb_onconn cb){
  cb_connstate = cb;
}



void _socket_update(){
  if(wclient.connected()){
    int packetLen;
    while((packetLen = wclient.available()) > 0){
      // receiving first
      uint16_t scode;
      wclient.read(reinterpret_cast<char*>(&scode), sizeof(uint16_t));
      bool _senddata = false;
      switch(scode){
        break; case PINGMESSAGE:{
          _senddata = true;
        }

        break; case SENDMESSAGE:{
          // getting data size
          int length;
          wclient.read(reinterpret_cast<char*>(&length), sizeof(int));

          // getting data
          wclient.read(_buf, length);

          cb_data(_buf, length);
        }

        break; case CLSOCKMESSAGE:{
          _onsocket_disconnect();
        }
      }


      if(_senddata && datas.size() > 0){
        while(datas.size() > 0){
          auto _currdata = datas.front();
          switch(_currdata.first){
            break; case SENDMESSAGE:{
              int buflen =
                HEADER_DATALEN +
                _currdata.second.second
              ;

              ByteIteratorR _bir{_buf, buflen};
              _bir.setVar(SENDMESSAGE);
              _bir.setVar(_currdata.second.second);
              _bir.setVarStr(_currdata.second.first, _currdata.second.second);

              wclient.write(_buf, buflen);
            }

            break; case CLSOCKMESSAGE:{
              wclient.write(reinterpret_cast<const char*>(&CLSOCKMESSAGE), sizeof(uint16_t));
              _onsocket_disconnect();
            }
          }

          if(_updateSock){
            if(datas.begin()->second.first)
              free(datas.begin()->second.first);
              
            datas.erase(datas.begin());
          }
        }
      }
      else
        wclient.write(reinterpret_cast<const char*>(&PINGMESSAGE), sizeof(uint16_t));
    }
  }
  else
    _onsocket_disconnect();
}



void _onsocket_connected(){
  _updateSock = true;
  cb_connstate(connected);
}

void _onsocket_disconnect(){
  _updateSock = false;
  for(auto &data: datas)
    if(data.second.first)
      free(data.second.first);
  datas.resize(0);

  wclient.stop();
  cb_connstate(disconnected);
}

void _socket_queuedata(uint16_t code, char *data, int datalen){
  datas.push_back({code, {data, datalen}});
}