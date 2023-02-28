#include "wifi.hpp"
#include "ESP8266WiFi.h"
#include "string"
#include "timer.hpp"


#define WIFI_UPDATEMS 5000
#define MAX_WIFIWAITCONNECT 30000


const IPAddress _nulip{0};
std::string passstr, ssidstr;
typedef void (*WifiStateCallback)(WifiState state);
void _wifidmpcb(WifiState state){}
IPAddress wifi_currentWifi = _nulip;
unsigned short wifi_currentPort = 0;
WifiState wifi_currentState = WifiState::Disconnected;
WifiStateCallback _wifi_cb = _wifidmpcb;

bool _autoConnect = false;
unsigned long _wifi_firstConnect = 0;


void wifi_reconnect();
void wifi_update(void *obj){
  IPAddress currip = WiFi.localIP();
  if(wifi_currentState == Waiting){

    if(currip != _nulip){
      wifi_currentState = Connected;
      _autoConnect = true;
      _wifi_cb(wifi_currentState);
    }
    else{
      WiFi.begin(ssidstr.c_str(), passstr.c_str());
      unsigned long _currtime = millis();
      if((_currtime-_wifi_firstConnect) > MAX_WIFIWAITCONNECT){
        wifi_currentState = Disconnected;
        _wifi_cb(CantConnect);
      }
    }
  }
  else if(wifi_currentState == Connected && currip == _nulip){
    wifi_currentState = Disconnected;
    _wifi_cb(wifi_currentState);
  }
  else if(wifi_currentState == Disconnected && _autoConnect){
    wifi_reconnect();
  }
}

void wifi_start(){
  timer_setInterval(WIFI_UPDATEMS, wifi_update, NULL);
}

void wifi_reconnect(){
  WiFi.begin(ssidstr.c_str(), passstr.c_str());
  _wifi_firstConnect = millis();
  wifi_currentState = Waiting;
  _wifi_cb(wifi_currentState);
}

void wifi_change(const char *ssid, const char *pass){
  if(wifi_currentState == Connected)
    WiFi.disconnect();
  
  wifi_currentState = Disconnected;
  _wifi_cb(wifi_currentState);

  ssidstr = ssid;
  passstr = pass;
  _autoConnect = true;

  wifi_reconnect();
}

void wifi_setcallback(WifiStateCallback cb){
  _wifi_cb = cb;
}

void wifi_disconnect(){
  if(wifi_currentState == Connected)
    WiFi.disconnect();

  wifi_currentState = Disconnected;
  _wifi_cb(wifi_currentState);
  _autoConnect = false;
}

WifiState wifi_currentstate(){
  return wifi_currentState;
}