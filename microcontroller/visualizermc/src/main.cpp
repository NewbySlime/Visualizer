#include "SPI.h"
#include "Wire.h"
#include "uart.h"
#include <Arduino.h>
#include "ESP8266WiFi.h"

#include "timer.hpp"
#include "polling.h"

#include "Adafruit_GFX.h"
#include "Adafruit_SSD1306.h"
#include "display/display.hpp"
#include "images/drawables.hpp"

#include "FastLED.h"

#include "string"
#include "cmath"

#include "ByteIterator.hpp"
#include "sensor4.hpp"
#include "server.hpp"
#include "password.hpp"
#include "leddriver-master.hpp"
#include "preset.hpp"
#include "visualizerled.hpp"
#include "EEPROM.hpp"

#include "misc.hpp"

using namespace std;

#define USEPORT 3020

#define VIS_DEFRATE 27
#define TWI_DEFAULTCLK 400000

#define LED_NUM 2

#define _iaddr_txt1size 1
#define _iaddr_txt2size 1
#define WIFI_UPDATEMS 5000
#define MAX_WIFIWAITCONNECT 30000
#define SSD1306_X 128
#define SSD1306_Y 64
#define IDLING_TIME 120000

#define PRESETTIMEOUT 2000 * MAX_PRESET_NUM

#ifdef ESP8266
#define INFO_MCUNAME "NodeMCU V2"
#endif

#define PRESETERROR_IMTEXT "Preset can't be receive fully"
#define PRESETOK_IMTEXT "Preset data received all"


// program codes
const unsigned short
  SOUNDDATA_DATA = 0x0010,
  SOUNDDATA_STOPSENDING = 0x0011,
  SOUNDDATA_STARTSENDING = 0x0012,
  FORWARD = 0x0020,
  SET_CONFIG_SG = 0x0021,
  GET_CONFIG_SG = 0x0022
;

// codes for soundget configuration
const unsigned short
  CFG_SG_INPUTDEVICEDATA = 0x0003,
  CFG_SG_SENDRATE = 0x0005,
  CFG_SG_MCUBAND = 0x0021
;

// codes for forward codes
const unsigned short
  MCU_ASKALLPRESET = 0x01,
  MCU_SETPRESET = 0x02,
  MCU_GETPRESET = 0x03,
  MCU_GETPRESETLEN = 0x04,
  MCU_UPDATEPRESETS = 0x05,
  MCU_USEPRESET = 0x06,
  MCU_GETMCUINFO = 0x10
;


// global variables
enum WifiState{
  Connected,
  CantConnect,
  Disconnected,
  Waiting
};

struct adev_data{
  uint16_t idx;
  int namelen;
  char *namestr;
  uint16_t nchannel;
};

string ssid = getPassword();
string password = getSSID();

char _txt_inetaddress[32] = {0};
const char *_txt_iaddr_disconnect = "Disconnected";
const char *_txt_iaddr_connect = "Connected";
draw_text mc_inetaddress{};

const IPAddress _nulip{0};
const char *passp = NULL, *ssidp = NULL;
typedef void (*WifiStateCallback)(WifiState state);
void _wifidmpcb(WifiState state){}
IPAddress wifi_currentWifi = _nulip;
unsigned short wifi_currentPort = 0;
WifiState wifi_currentState = WifiState::Disconnected;
WifiStateCallback _wifi_cb = _wifidmpcb;

bool _autoConnect = false;
unsigned long _wifi_firstConnect = 0;

Adafruit_SSD1306 display(128, 64, &Wire);
displayHandler disphand{};
bitmap_battery bbattbar{};
draw_time dtime{};
draw_acceptbar daccbar{};

bool _idling = true;
int _timer_id_idle = -1;

RTC_DS1307 rtc;
unsigned long __numled = 0;
uint8_t battbar = 0;

DateTime currtime;

uint8_t _vis_uprate = VIS_DEFRATE;
int _vis_timerid = -1;
bool _vis_doUpdate = false;
visualizer *vis;

String _ipaddrstr;
bool _wifi_connect = false;

vector<bool> _ispresetset;
uint16_t _last_presetnum;
bool _set_preset = false;
uint16_t _presetcount;
int _presettimerid;
uint16_t _presetuseidx;



void _init_list(){
  mc_inetaddress.useColor(SSD1306_WHITE, SSD1306_WHITE);
  mc_inetaddress.useSize(_iaddr_txt1size, _iaddr_txt2size);
  mc_inetaddress.useText(_txt_iaddr_disconnect, NULL);
  mc_inetaddress.changeTextType(draw_text::Center);
}



//      misc stuff
void importantText(const char *txt){
  disphand.importanttext(txt);
}



//      config processing
void _sendPresetLenData(){
  size_t bufsize =
    sizeof(uint16_t) +
    sizeof(uint16_t) +
    sizeof(uint16_t)
  ;

  char *buf = (char*)malloc(bufsize);
  ByteIteratorR _bir{buf, bufsize};
  _bir.setVar(FORWARD);
  _bir.setVar(MCU_GETPRESETLEN);
  _bir.setVar((uint16_t)PresetData.manyPreset());

  socket_queuedata(buf, bufsize);
}

void _sendUsePresetIdx(){
  size_t bufsize =
    sizeof(uint16_t) +
    sizeof(uint16_t) +
    sizeof(uint16_t)
  ;

  char *buf = (char*)malloc(bufsize);
  ByteIteratorR _bir{buf, bufsize};
  _bir.setVar(FORWARD);
  _bir.setVar(MCU_USEPRESET);
  _bir.setVar((uint16_t)PresetData.getLastUsedPresetIdx());

  socket_queuedata(buf, bufsize);
}

void _sendPresetData(int idx){
  if(idx >= PresetData.manyPreset())
    return;

  size_t psize = 
    sizeof(uint16_t) +
    sizeof(uint16_t) +
    sizeof(uint16_t) +
    PresetData.presetSizeInBytes(idx)
  ;

  char *presetmem = (char*)malloc(psize);
  ByteIteratorR _bir{presetmem, psize};
  _bir.setVar(FORWARD);
  _bir.setVar(MCU_SETPRESET);
  _bir.setVar((uint16_t)idx);

  size_t actualSize = PresetData.copyToMemory(_bir, idx);
  if(actualSize == 0){
    free(presetmem);
    return;
  }

  socket_queuedata(presetmem, actualSize);
}




//      preset handling
void _on_presettimeout(void*){
  vis->useErrorPreset();
  disphand.importanttext(PRESETERROR_IMTEXT);
  PresetData.resizePreset(_last_presetnum); 
  _set_preset = false;
}

void _on_presetallrecv(){
  timer_deleteTimer(_presettimerid);
  vis->usePreset(_presetuseidx);
  _set_preset = false;
}

void _on_getpreset(ByteIterator &bi){
  uint16_t idx; bi.getVar(idx);
  if(idx >= PresetData.manyPreset())
    return;
  
  preset *data = new preset{};
  Serial.printf("datamem %X\n", data);
  if(!presetData::setPresetFromMem(bi, *data)){
    Serial.printf("not sufficient\n");
    delete data;
    return;
  }

  PresetData.setPreset(idx, *data);
  if(!_ispresetset[idx]){
    _ispresetset[idx] = true;
    if(++_presetcount >= PresetData.manyPreset())
      _on_presetallrecv();
    
    Serial.printf("presetcount %d %d\n", _presetcount, PresetData.manyPreset());
  }

  delete data;
}



//      socket handling
void _s_onforward(const char *data, int datalen);
void _s_oncfg_indevdat(adev_data *devdata);
void callbacksock(const char *buf, int buflen){
  if(buflen >= sizeof(unsigned short)){
    ByteIterator _bi{buf, buflen};
    unsigned short msgcode = 0; _bi.getVar(msgcode);

    if(msgcode != SOUNDDATA_DATA){
      Serial.printf("buflen %d\n", buflen);
      Serial.printf("msgcode %x\n", msgcode);
    }

    switch(msgcode){
      break; case SOUNDDATA_DATA:{
        if(_bi.available() < sizeof(int)) return;
        int n; _bi.getVar(n);

        if(n == LED_NUM && _bi.available() < (sizeof(float)*n)) return;
        float *data = new float[n]; _bi.getVar(data, sizeof(float)*n);

        vis->updateSound(data);

        delete[] data;
      }

      break; case FORWARD:{
        _s_onforward(&buf[buflen-_bi.available()], _bi.available());
      }

      break; case GET_CONFIG_SG:{
        if(_bi.available() < sizeof(uint16_t)) return;
        uint16_t cfgcode; _bi.getVar(cfgcode);

        switch(cfgcode){
          break; case CFG_SG_INPUTDEVICEDATA:{
            adev_data devdat;
            devdat.namestr = (char*)malloc(0);
            while(true){
              if(_bi.available() < sizeof(uint16_t)) break;
              _bi.getVar(devdat.idx);

              if(_bi.available() < sizeof(int)) break;
              _bi.getVar(devdat.namelen);

              if(_bi.available() < devdat.namelen) break;
              devdat.namestr = (char*)realloc(devdat.namestr, devdat.namelen);
              _bi.getVar(devdat.namestr, devdat.namelen);

              if(_bi.available() < sizeof(uint16_t)) break;
              _bi.getVar(devdat.nchannel);

              _s_oncfg_indevdat(&devdat);
            break;}

            free(devdat.namestr);
          }
        }
      }
    }
  }
}

void _s_onforward(const char *data, int datalen){
  ByteIterator _bi(data, datalen);

  Serial.printf("len %d\n", datalen);

  if(_bi.available() < sizeof(uint16_t))
    return;

  uint16_t code; _bi.getVar(code);
  Serial.printf("forcode %x\n", code);
  switch(code){
    break; case MCU_ASKALLPRESET:{
      _sendPresetLenData();
      _sendUsePresetIdx();

      for(int i = 0; i < PresetData.manyPreset(); i++)
        _sendPresetData(i);
    }

    break; case MCU_SETPRESET:{
      size_t smem = ESP.getFreeHeap();
      uint8_t fragp = ESP.getHeapFragmentation();
      Serial.printf("mem available %d, fragmentation %d/255\n", smem, fragp);
      delay(500);

      _on_getpreset(_bi);
    }

    break; case MCU_GETPRESET:{
      uint16_t idx; _bi.getVar(idx);
      _sendPresetData(idx);
    }

    break; case MCU_GETPRESETLEN:{
      if(_set_preset){
        uint16_t plen; _bi.getVar(plen);
        PresetData.resizePreset(plen);
      }
    }

    break; case MCU_UPDATEPRESETS:{
      if(!_set_preset){
        _set_preset = true;
        _last_presetnum = PresetData.manyPreset();
        _presetcount = 0;
        _presetuseidx = 0;
        _ispresetset.clear();
        _ispresetset.resize(MAX_PRESET_NUM);
        
        vis->useLoadingPreset();

        // timer for timeout when data too long to be received
        _presettimerid = timer_setTimeout(PRESETTIMEOUT, _on_presettimeout, NULL);
      }
    }
    break; case MCU_USEPRESET:{
      if(_set_preset)
        _bi.getVar(_presetuseidx);
    }

    break; case MCU_GETMCUINFO:{
      size_t datalen =
        sizeof(uint16_t) +
        sizeof(uint16_t) +
        // mcu name (string)
        sizeof(uint16_t) +
        strlen(INFO_MCUNAME) +
        // many led
        sizeof(uint16_t)
      ;

      char *data = (char*)malloc(datalen);
      ByteIteratorR _bir(data, datalen);

      _bir.setVar(FORWARD);
      _bir.setVar(MCU_GETMCUINFO);
      _bir.setVar((uint16_t)strlen(INFO_MCUNAME));
      _bir.setVar(INFO_MCUNAME, strlen(INFO_MCUNAME));
      _bir.setVar((uint16_t)LED_NUM);

      socket_queuedata(data, datalen);
    }
  }
}

// TODO
void _s_oncfg_indevdat(adev_data *devdat){

}

void _onconnstatesock(sock_connstate state){
  switch(state){
    break; case sock_connstate::connected:{
      // sending led length
      uint16_t data1len = 
        sizeof(uint16_t) +
        sizeof(uint16_t) +
        sizeof(uint16_t);
      
      char *data1 = (char*)malloc(data1len);
      ByteIteratorR _bir(data1, data1len);

      _bir.setVar(SET_CONFIG_SG);
      _bir.setVar(CFG_SG_MCUBAND);
      _bir.setVar((uint16_t)LED_NUM);

      socket_queuedata(data1, data1len);


      // sending send rate
      uint16_t data2len =
        sizeof(uint16_t) +
        sizeof(uint16_t) +
        sizeof(uint16_t);
      
      char *data2 = (char*)malloc(data2len);
      _bir = ByteIteratorR(data2, data2len);

      _bir.setVar(SET_CONFIG_SG);
      _bir.setVar(CFG_SG_SENDRATE);
      _bir.setVar((uint16_t)VIS_DEFRATE);

      socket_queuedata(data2, data2len);

      // asking for input device data
      uint16_t data3len =
        sizeof(uint16_t) +
        sizeof(uint16_t);
      
      char *data3 = (char*)malloc(data3len);
      _bir = ByteIteratorR(data3, data3len);

      _bir.setVar(GET_CONFIG_SG);
      _bir.setVar(CFG_SG_INPUTDEVICEDATA);

      socket_queuedata(data3, data3len);


      // sending start soundget
      uint16_t data4len =
        sizeof(uint16_t);
      
      char *data4 = (char*)malloc(data4len);
      _bir = ByteIteratorR(data4, data4len);

      _bir.setVar(SOUNDDATA_STARTSENDING);

      socket_queuedata(data4, data4len);
    }

    break; case sock_connstate::disconnected:{
      // TODO
    }
  }
}


//      Wifi handling
void wifi_reconnect();
void wifi_update(void *obj){
  IPAddress currip = WiFi.localIP();
  if(wifi_currentState == Waiting){

    if(currip != _nulip){
      wifi_currentState = Connected;
      _wifi_cb(wifi_currentState);
    }
    else{
      if(passp && ssidp)
        WiFi.begin(ssidp, passp);
      unsigned long currtime = millis();
      if((currtime-_wifi_firstConnect) > MAX_WIFIWAITCONNECT){
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
  WiFi.begin(ssidp, passp);
  _wifi_firstConnect = millis();
  wifi_currentState = Waiting;
  _wifi_cb(wifi_currentState);
}

void wifi_change(const char* ssid, const char *pass){
  if(wifi_currentState == Connected)
    WiFi.disconnect();

  ssidp = ssid;
  passp = pass;
  _autoConnect = true;
}

void wifi_setcallback(WifiStateCallback cb){
  _wifi_cb = cb;
}

void wifi_disconnect(){
  if(wifi_currentState == Connected)
    WiFi.disconnect();

  _wifi_cb(Disconnected);
  _autoConnect = false;
}



//      Main list data
void __dmpfunc1(listDisplay_Interact **datalist){}
void _wifistatelist(listDisplay_Interact **datalist);



//      Sensor handling
void _onidle(void *obj){
  _idling = true;
  disphand.back_tomainlist();
}

void callbackSensor(sensor_actType acttype){
  switch(acttype){
    break; case slide_left:
      disphand.act_left();

    break; case slide_right:
      disphand.act_right();
    
    break; case click:

    break; case double_click:
      disphand.act_back();

    break; case pressed:
      daccbar.onpressed();

    break; case released:
      daccbar.onreleased();
  }

  if(_idling){
    _idling = false;
    _timer_id_idle = timer_setTimeout(IDLING_TIME, _onidle, NULL);
  }
  else{
    timer_deleteTimer(_timer_id_idle);
    _timer_id_idle = timer_setTimeout(IDLING_TIME, _onidle, NULL);
  }
}

void callbacktest1s(void *b){
  currtime = currtime + TimeSpan(1);
  dtime.settime(currtime);

  battbar = (battbar+1)%10;
  bbattbar.setbatterylevel((float)battbar/10);
}



//        listintract datas
listDisplay_Interact _maininteractlist{
  // wifi state
  .listinteract = {
    listDisplay_Interact::_interactData{
      .onClicked = _wifistatelist,
      .useFixedName = false,
      .objdraw = &mc_inetaddress
    },

    // timing
    listDisplay_Interact::_interactData{
      .onClicked = NULL,
      .useFixedName = false,
      .objdraw = &dtime
    },

    // TODO
    // led mode
  },

  .listIndex = 1
};

listDisplay_Interact _wifiinfo_list{
  .listinteract = {
    listDisplay_Interact::_interactData{

    }
  }
};



//        Additional handling
void _onacceptbar_accept(){
  disphand.act_accept();
}

void _onwifistate(WifiState state){
  switch(state){
    break; case Connected:
      disphand.importanttext("Connected");
      _wifi_connect = true;
      _ipaddrstr = WiFi.localIP().toString();
      Serial.printf("IP: %s\n", _ipaddrstr.c_str());
      sprintf(_txt_inetaddress, "IP: %s", _ipaddrstr.c_str());
      mc_inetaddress.changeTextType(draw_text::Top_Bottom);
      mc_inetaddress.useText(_txt_iaddr_connect, _txt_inetaddress);
    
    break; case Disconnected:
      disphand.importanttext("Disconnected");
      _wifi_connect = false;
      mc_inetaddress.changeTextType(draw_text::Center);
      mc_inetaddress.useText(_txt_iaddr_disconnect, NULL);
    
    break; case CantConnect:
      disphand.importanttext("Can't connect");
      _wifi_connect = false;
      mc_inetaddress.changeTextType(draw_text::Center);
      mc_inetaddress.useText(_txt_iaddr_disconnect, NULL);
  }
}


void _wifistatelist(listDisplay_Interact **list){
  *list = &_wifiinfo_list;
}

void _changewifistate(listDisplay_Interact**){
  if(_wifi_connect)
    wifi_disconnect();
  else
    wifi_reconnect();
}

void _vis_update(void*){
  if(_vis_doUpdate){
    vis->update();
    led_sendcols(vis->getData());
  }
}

void _vis_changerate(uint8_t rate){
  timer_changeInterval(_vis_timerid, 1000/rate);
}

void _init_visualizer(){
  _vis_doUpdate = true;
  PresetData.initPreset();
  vis = new visualizer(LED_NUM);
  _vis_timerid = timer_setInterval(1000/VIS_DEFRATE, _vis_update, NULL);
}


#ifdef DO_TEST_SKETCH
#endif
AT24C256 storage(0b1010000);
char test[1000];

void test_sending(void*){
  Serial.begin(9600);

  storage.bufWriteBlock(0x20, "estt", 4);
  storage.bufReadBlock(0x20, test, 4);

  printf("%s\n", test);
}


void setup() {
  #ifdef DO_TEST_SKETCH
  #else

  delay(5000);
  Serial.begin(9600);

  Wire.begin();
  Wire.setClock(TWI_DEFAULTCLK);
  test_sending(NULL);

  // setting up wifi
  // using WIFI_STA too slow after some time?
  WiFi.mode(WIFI_AP);
  wifi_start();
  wifi_change(getSSID(), getPassword());

  server_init(USEPORT);
  set_socketcallback(callbacksock);
  set_socketconnstateCb(_onconnstatesock);

  wifi_setcallback(_onwifistate);

  // setting up list for to be put on the display
  _init_list();

  // setting up touch sensor
  uint8_t sensor4_pindata[] = {D5, D6, D7, D8};
  sensor4_setcallback(callbackSensor);
  sensor4_init(sensor4_pindata);
  
  // setting up display
  if(display.begin(SSD1306_SWITCHCAPVCC, 0x3C)){
    disphand.bind_display(SSD1306_X, SSD1306_Y, SSD1306, &display);
    disphand.add_objectDraw(&bbattbar);
    disphand.add_objectDraw(&dtime);
    disphand.add_objectDraw(&daccbar);
    disphand.use_mainlist(&_maininteractlist);

    daccbar.setcallback(disphand._act_accept, &disphand);
  }

  // setting up rtc
  currtime = DateTime(F(__DATE__), F(__TIME__));
  /*
  if(!rtc.begin()){
    Serial.printf("can't find rtc\n");
    digitalWrite(LED_BUILTIN, HIGH);
    while(1)
      ;
  }

  if(!rtc.isrunning()){
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }
  */

  // setting up led
  Serial1.begin(LEDDRIVER_BAUDRATE);
  led_init(LED_NUM);

  // setting up visualizer
  _init_visualizer();

  timer_setInterval(1000, callbacktest1s, NULL);
  #endif
}

void loop() {
  timer_update();
  polling_update();
}