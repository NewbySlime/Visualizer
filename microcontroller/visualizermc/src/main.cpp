#include "SPI.h"
#include "Wire.h"
#include "uart.h"
#include <Arduino.h>
#include "ESP8266WiFi.h"

#include "ESP8266TimerInterrupt.h"

#include "timer.hpp"
#include "polling.h"

#include "Adafruit_GFX.h"
#include "Adafruit_SSD1306.h"
#include "display/display.hpp"
#include "images/drawables.hpp"

#include "string"
#include "cmath"
#include "time.h"

#include "ByteIterator.hpp"
#include "sensor4.hpp"
#include "server.hpp"
#include "password.hpp"
#include "leddriver-master.hpp"
#include "preset.hpp"
#include "visualizerled.hpp"
#include "EEPROM.hpp"
#include "file_system.hpp"
#include "wifi.hpp"
#include "time_rand.hpp"
#include "data_encryption.hpp"
#include "async.hpp"

#include "misc.hpp"

using namespace std;

#define USEPORT 3020

#define VIS_DEFRATE 27
#define TWI_DEFAULTCLK 400000

#define _iaddr_txt1size 1
#define _iaddr_txt2size 1
#define SSD1306_X 128
#define SSD1306_Y 64
#define IDLING_TIME 120000

#define HANDSHAKETIMEOUT 20000

#define ENCRYPTKEY_FILEID 0x001F

#define PRESETTIMEOUT 2000 * MAX_PRESET_NUM

#define BATTERYCHECK_INTERVAL 10000

#ifdef ESP8266
const char *INFO_MCUNAME = "NodeMCU V2";
#endif

const char *PRESETERROR_IMTEXT = "Preset can't be receive fully";
const char *PRESETOK_IMTEXT = "Preset data received all";

const char *WIFISTATE_DISCONNECT_TEXT = "Disconnect";
const char *WIFISTATE_CONNECT_TEXT = "Connect";
const char *WIFISTATE_CONNECTING_TEXT = "Connecting";

const char *_txt_iaddr_disconnect = "Disconnected";
const char *_txt_iaddr_connect = "Connected";


#define DISP_WIFINAME_MAX 10
#define DISP_WIFINAME_CONSTRAINT 8


// program codes
const unsigned short
  SOUNDDATA_DATA = 0x0010,
  SOUNDDATA_STOPSENDING = 0x0011,
  SOUNDDATA_STARTSENDING = 0x0012,
  FORWARD = 0x0020,
  SET_CONFIG_SG = 0x0021,
  GET_CONFIG_SG = 0x0022,

  // --   Codes that will not be encrypted    --
  // MCU pairing
  // will be send first when connecting to MCU when it's on pairing mode
  MCU_PAIR = 0x0100,
  // Message to connect to MCU
  MCU_CONNECT = 0x101
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
  MCU_UPDATEPRESETS = 0x02,
  MCU_USEPRESET = 0x03,

  MCU_SETPRESET = 0x100,
  MCU_SETPASSWORD = 0x101,
  MCU_SETREMOVEPASSWORD = 0x102,
  MCU_SETTIME = 0x103,

  MCU_GETPRESET = 0x200,
  MCU_GETPRESETLEN = 0x201,
  MCU_GETPASSWORDMAX = 0x202,
  MCU_GETPASSWORD = 0x203,

  MCU_GETMCUINFO = 0x280,
  MCU_GETMCUINFO_BATTERYLVL = 0x281
;


// global variables
struct adev_data{
  uint16_t idx;
  int namelen;
  char *namestr;
  uint16_t nchannel;
};

char _txt_inetaddress[32] = {0};
draw_text mc_inetaddress{};

Adafruit_SSD1306 display(128, 64, &Wire);
displayHandler disphand{};
bitmap_battery bbattbar{};
draw_time dtime{};
draw_acceptbar daccbar{};
draw_text mediaCtrl_draw;
draw_text ledMode_draw;
draw_loadapply pairMode_draw;

bool _idling = true;
int _timer_id_idle = -1;

RTC_DS1307 rtc;
unsigned long __numled = 0;
uint8_t battbar = 0;
bool _rtc_found;

DateTime _currtime;
volatile int _secondsElapsed = 0;

uint8_t _vis_uprate = VIS_DEFRATE;
int _vis_timerid = -1;
bool _vis_doUpdate = false;
bool _vis_ledoff = true;
visualizer *vis;

String _ipaddrstr;
string _conntostr;
int _wifi_authidx = 0;

vector<bool> _ispresetset;
uint16_t _last_presetnum;
bool _set_preset = false;
uint16_t _presetcount;
int _presettimerid;
uint16_t _presetuseidx;

float _batterylvl;

AT24C256 _eeprom(0b1010000);

data_encryption crypt;

bool _alreadypaired = false;
bool _pairmode_b = false;
bool _is_paired = false;

bool _handshaked = false;
int _handshaketimerid;

ESP8266TimerInterrupt HardwareTimer;




//      declaring functions
void _doPairing();
void _cancelPairing();
void _onDonePairing();
void _s_onforward(ByteIterator &_bi);
void _s_oncfg_indevdat(adev_data *devdata);
void _wifistatelist(listDisplay_Interact **datalist);
void _pairmodelist(listDisplay_Interact**, void*);
void _wifistatelist(listDisplay_Interact**, void*);
void _mediacontrollist(listDisplay_Interact**, void*);
void _ledmodelist(listDisplay_Interact**, void*);
void _onhandshaked();
void _updateTime();




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
  ByteIteratorR_Encryption _bir{buf, bufsize, &crypt};
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
  ByteIteratorR_Encryption _bir{buf, bufsize, &crypt};
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

  Serial.printf("psize %d\n", psize);

  char *presetmem = (char*)malloc(psize);
  ByteIteratorR_Encryption _bir{presetmem, psize, &crypt};
  _bir.setVar(FORWARD);
  _bir.setVar(MCU_SETPRESET);
  _bir.setVar((uint16_t)idx);

  Serial.printf("Getting preset idx: %d\n");

  size_t actualSize = PresetData.copyToMemory(_bir, idx);
  for(int i = 0; i < psize; i++){
    if((i % 16) == 0)
      Serial.printf("\nidx %d 0x%X\n\t", i, i);
    
    Serial.printf("0x%X ", presetmem[i]);
  }

  if(actualSize == 0){
    Serial.printf("something wrong when copying preset.\n");
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

  // TODO use a timer to ask the visualizer to sent another data when the timer sets off

  delete data;
}



//      socket handling
void callbacksock(const char *buf, int buflen){
  if(buflen >= sizeof(unsigned short)){
    if(_handshaked){
      ByteIterator_Encryption _bi{buf, buflen, &crypt};
      unsigned short msgcode = 0; _bi.getVar(msgcode);
      if(msgcode != SOUNDDATA_DATA){
        Serial.printf("buflen %d\n", buflen);
        Serial.printf("msgcode %x\n", msgcode);
      }

      switch(msgcode){
        break; case SOUNDDATA_DATA:{
          if(_bi.available() < sizeof(int))
            return;

          int n; _bi.getVar(n);

          if(n == CONST_LEDNUM && _bi.available() < (sizeof(float)*n)) return;
          float *data = new float[n]; _bi.getVarStr(reinterpret_cast<char*>(data), sizeof(float)*n);

          vis->updateSound(data);

          delete[] data;
        }

        break; case FORWARD:{
          _s_onforward(_bi);
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
                _bi.getVarStr(devdat.namestr, devdat.namelen);

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
    else{
      Serial.printf("Not handshaked\n");
      ByteIterator _bi(buf, buflen);
      unsigned short _code; _bi.getVar(_code);

      Serial.printf("Msg Code %d\n", _code);

      switch(_code){
        break; case MCU_PAIR:{
          // this will send once
          // then send the key
          // if soundget wants to connect it should use MCU_CONNECT

          if(_pairmode_b){
            crypt.createAndUseKey();
            size_t _datalen = sizeof(uint16_t) + crypt.getKey()->size();
            char *_data = (char*)malloc(_datalen);

            //Serial.printf("key: %s\n", crypt.getKey()->c_str());

            Serial.printf("_datalen %d\n", _datalen);

            ByteIteratorR _bir(_data, _datalen);
            _bir.setVar(MCU_PAIR);
            _bir.setVarStr(crypt.getKey()->c_str(), crypt.getKey()->size());

            socket_queuedata(_data, _datalen);
            _onDonePairing();
          }
        }

        break; case MCU_CONNECT:{
          // mcu will send part of the key encrypted based on the key itself
          // then soundget will do the same

          if(!_pairmode_b){
            Serial.printf("Connecting\n");
            size_t _enkeylen = _bi.available();
            if(_enkeylen > crypt.getKey()->size()){
              Serial.printf("Handshake key is longer than the key\n");
              break;
            }

            char _enkey[_enkeylen];
            _bi.getVarStr(_enkey, _enkeylen);

            //Serial.printf("encrypted hskey: %s\n", _enkey);

            crypt.encryptOrDecryptData(_enkey, _enkeylen, _enkeylen);
            const char *_ckey = crypt.getKey()->c_str();

            //Serial.printf("key: %s\n", _ckey);
            //Serial.printf("decrypted hskey: %s\n", _enkey);

            size_t i = 0;
            for(; i < _enkeylen; i++)
              if(_ckey[i] != _enkey[i])
                break;
            
            if(i < _enkeylen){
              Serial.printf("key not the same. %d %d\n", i, _enkeylen);
              break;
            }
            
            if(!_alreadypaired){
              _alreadypaired = true;
              size_t _keydatalen = crypt.getKey()->size();
              char *_keydata = (char*)malloc(_keydatalen);
              memcpy(_keydata, crypt.getKey()->c_str(), _keydatalen);

              Serial.printf("key bin dump\n");
              for(int i = 0; i < _keydatalen; i++)
                Serial.printf("%X ", _keydata[i]);

              EEPROM_FS.write_file(ENCRYPTKEY_FILEID, _keydata, _keydatalen);
            }

            Serial.printf("Handshaked\n");
            _onhandshaked();
          }
        }
      }
    }
  }
}

void _s_onforward(ByteIterator &_bi){
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


    break; case MCU_SETPRESET:{
      size_t smem = ESP.getFreeHeap();
      uint8_t fragp = ESP.getHeapFragmentation();
      Serial.printf("mem available %d, fragmentation %d/255\n", smem, fragp);
      delay(500);

      _on_getpreset(_bi);
    }

    // TODO string length checking
    break; case MCU_SETPASSWORD:{
      uint16_t _authlen; _bi.getVar(_authlen);
      
      for(int i = 0; i < _authlen; i++){
        uint16_t _ssidstrlen; _bi.getVar(_ssidstrlen);
        char _ssidstr[_ssidstrlen+1]; _bi.getVarStr(_ssidstr, _ssidstrlen);
        _ssidstr[_ssidstrlen] = '\0';

        uint16_t _passstrlen; _bi.getVar(_passstrlen);   
        char _passstr[_passstrlen]; _bi.getVarStr(_passstr, _passstrlen);
        _passstr[_passstrlen] = '\0';

        if(i <= PWD_alternateCount())
          PWD_addAuth(_ssidstr, _passstr);
        else
          PWD_setAuth(i, _ssidstr, _passstr);
      }
    }

    break; case MCU_SETREMOVEPASSWORD:{
      uint16_t _authidx; _bi.getVar(_authidx);
      PWD_removeAuth(_authidx);
    }

    break; case MCU_SETTIME:{
      tm _time; _bi.getVar(_time);
      _currtime = DateTime(_time.tm_year, _time.tm_mon, _time.tm_mday, _time.tm_hour, _time.tm_min, _time.tm_sec);

      if(_rtc_found)
        rtc.adjust(_currtime);

      _updateTime();
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

    break; case MCU_GETPASSWORDMAX:{
      size_t datalen =
        sizeof(uint16_t) +
        sizeof(uint16_t) +
        sizeof(uint16_t)
      ;

      char *data = (char*)malloc(datalen);
      ByteIteratorR_Encryption _bir(data, datalen, &crypt);

      _bir.setVar(FORWARD);
      _bir.setVar(MCU_GETPASSWORDMAX);
      _bir.setVar(PWD_maxAlternates);

      socket_queuedata(data, datalen);
    }

    break; case MCU_GETPASSWORD:{
      size_t datalen =
        sizeof(uint16_t) +
        sizeof(uint16_t) +
        sizeof(uint16_t)
      ;

      for(int i = 0; i < PWD_alternateCount(); i++){
        auto _p = PWD_getAuth(i);

        datalen +=
          sizeof(uint16_t) +
          strlen(_p.first) +
          sizeof(uint16_t) +
          strlen(_p.second)
        ;
      }

      char *data = (char*)malloc(datalen);
      ByteIteratorR_Encryption _bir(data, datalen, &crypt);

      _bir.setVar(FORWARD);
      _bir.setVar(MCU_SETPASSWORD);
      _bir.setVar((uint16_t)PWD_alternateCount());

      for(int i = 0; i < PWD_alternateCount(); i++){
        auto _p = PWD_getAuth(i);
        uint16_t _strsize_ssid = strlen(_p.first);
        uint16_t _strsize_pass = strlen(_p.second);

        _bir.setVar(_strsize_ssid);
        _bir.setVarStr(_p.first, _strsize_ssid);
        
        _bir.setVar(_strsize_pass);
        _bir.setVarStr(_p.second, _strsize_pass);
      }

      socket_queuedata(data, datalen);
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
      ByteIteratorR_Encryption _bir(data, datalen, &crypt);

      _bir.setVar(FORWARD);
      _bir.setVar(MCU_GETMCUINFO);
      _bir.setVar((uint16_t)strlen(INFO_MCUNAME));
      _bir.setVarStr(INFO_MCUNAME, strlen(INFO_MCUNAME));
      _bir.setVar((uint16_t)CONST_LEDNUM);

      socket_queuedata(data, datalen);
    }

    break; case MCU_GETMCUINFO_BATTERYLVL:{
      size_t datalen =
        sizeof(uint16_t) +
        sizeof(uint16_t) +
        sizeof(float)
      ;

      char *data = (char*)malloc(datalen);
      ByteIteratorR_Encryption _bir(data, datalen, &crypt);

      _bir.setVar(FORWARD);
      _bir.setVar(MCU_GETMCUINFO_BATTERYLVL);
      _bir.setVar(_batterylvl);

      socket_queuedata(data, datalen);
    }
  }
}

// TODO
void _s_oncfg_indevdat(adev_data *devdat){

}

void _cb_onhandshake_timeout(void *){
  socket_disconnect();
  Serial.printf("handshake timeout\n");
}

void _onhandshaked(){
  _handshaked = true;
  timer_deleteTimer(_handshaketimerid);

  // sending led length
  uint16_t data1len = 
    sizeof(uint16_t) +
    sizeof(uint16_t) +
    sizeof(uint16_t);
  
  char *data1 = (char*)malloc(data1len);
  ByteIteratorR_Encryption _bir(data1, data1len, &crypt);

  _bir.setVar(SET_CONFIG_SG);
  _bir.setVar(CFG_SG_MCUBAND);
  _bir.setVar((uint16_t)CONST_LEDNUM);

  socket_queuedata(data1, data1len);


  // sending send rate
  uint16_t data2len =
    sizeof(uint16_t) +
    sizeof(uint16_t) +
    sizeof(uint16_t);
  
  char *data2 = (char*)malloc(data2len);
  _bir = ByteIteratorR_Encryption(data2, data2len, &crypt);

  _bir.setVar(SET_CONFIG_SG);
  _bir.setVar(CFG_SG_SENDRATE);
  _bir.setVar((uint16_t)VIS_DEFRATE);

  socket_queuedata(data2, data2len);

  // asking for input device data
  uint16_t data3len =
    sizeof(uint16_t) +
    sizeof(uint16_t);
  
  char *data3 = (char*)malloc(data3len);
  _bir = ByteIteratorR_Encryption(data3, data3len, &crypt);

  _bir.setVar(GET_CONFIG_SG);
  _bir.setVar(CFG_SG_INPUTDEVICEDATA);

  socket_queuedata(data3, data3len);


  // sending start soundget
  uint16_t data4len =
    sizeof(uint16_t);
  
  char *data4 = (char*)malloc(data4len);
  _bir = ByteIteratorR_Encryption(data4, data4len, &crypt);

  _bir.setVar(SOUNDDATA_STARTSENDING);

  socket_queuedata(data4, data4len);
}

void _onconnstatesock(sock_connstate state){
  switch(state){
    break; case sock_connstate::connected:{
      _handshaketimerid = timer_setTimeout(HANDSHAKETIMEOUT, _cb_onhandshake_timeout, NULL);
      Serial.printf("_handshaketimer %d\n", _handshaketimerid);
      //Serial.printf("pair mode: %s\n", _pairmode_b? "true": "false");
      
      if(_pairmode_b){
        Serial.printf("Pairing mode\n");
        size_t _datalen = sizeof(uint16_t);
        char *_data = (char*)malloc(_datalen);
        
        ByteIteratorR _bir(_data, _datalen);
        _bir.setVar(MCU_PAIR);

        socket_queuedata(_data, _datalen);
      }
      else{
        Serial.printf("Not pairing mode\n");
        // this should send the key to soundget
        size_t _maxlen = crypt.getKey()->size()/4*3;
        size_t _minlen = crypt.getKey()->size()/4;
        Serial.printf("size %d maxlen %d minlen %d\n", crypt.getKey()->size(), _maxlen, _minlen);

        size_t _len = sizeof(uint16_t) + max(TimeRandom.getRandomBuffer().first[0] % _maxlen, _minlen);
        char *_enkey = (char*)malloc(_len);
        
        ByteIteratorR _bir(_enkey, _len);
        _bir.setVar(MCU_CONNECT);

        memcpy(_enkey+_bir.tellidx(), crypt.getKey()->c_str(), _bir.available());
        //Serial.printf("key: %s\nhskey: %s\n", crypt.getKey()->c_str(), _enkey+_bir.tellidx());

        crypt.encryptOrDecryptData(_enkey+_bir.tellidx(),  _bir.available(), _bir.available());

        //Serial.printf("encrypted hskey: %s\n", _enkey+_bir.tellidx());

        socket_queuedata(_enkey, _len);
      }
    }

    break; case sock_connstate::disconnected:{
      _handshaked = false;
    }
  }
}



//      Pairing functions
void _doPairing(){
  socket_disconnect();

  _pairmode_b = true;
  _alreadypaired = false;

  Serial.printf("pair mode: %s\n", _pairmode_b? "true": "false");
  pairMode_draw.onLoading();
}

void _cancelPairing(){
  _pairmode_b = false;

  pairMode_draw.onCancelled();
}

void _onDonePairing(){
  _pairmode_b = false;

  pairMode_draw.onApplied();
}



//      Main list data
void __dmpfunc1(listDisplay_Interact **datalist){}



//      Sensor handling
void _onidle(void *obj){
  _idling = true;
  disphand.back_tomainlist();
}

void callbackSensor(sensor_actType acttype){
  TimeRandom.supplyTime(millis());

  switch(acttype & 0xffff){
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

  disphand.pass_input(acttype);

  if(_idling){
    _idling = false;
    _timer_id_idle = timer_setTimeout(IDLING_TIME, _onidle, NULL);
  }
  else{
    timer_deleteTimer(_timer_id_idle);
    _timer_id_idle = timer_setTimeout(IDLING_TIME, _onidle, NULL);
  }
}

void _batteryUpdate_sw(void *b){
  battbar = (battbar+1)%10;
  _batterylvl = (float)battbar/10;
  bbattbar.setbatterylevel(_batterylvl);
}



//        listintract datas
listDisplay_Interact _maininteractlist{
  .listinteract = {
    // pairing mode
    listDisplay_Interact::_interactData{
      .onClicked = _pairmodelist,
      .useFixedName = false,
      .inputPass = NULL,
      .objdraw = &pairMode_draw
    },
    
    // TODO
    // audio device change

    // media control
    listDisplay_Interact::_interactData{
      .onClicked = _mediacontrollist,
      .useFixedName = false,
      .inputPass = NULL,
      .objdraw = &mediaCtrl_draw
    },

    // wifi state
    listDisplay_Interact::_interactData{
      .onClicked = _wifistatelist,
      .useFixedName = false,
      .inputPass = NULL,
      .objdraw = &mc_inetaddress
    },

    // timing
    listDisplay_Interact::_interactData{
      .onClicked = NULL,
      .useFixedName = false,
      .inputPass = NULL,
      .objdraw = &dtime
    },

    // led mode
    listDisplay_Interact::_interactData{
      .onClicked = _ledmodelist,
      .useFixedName = false,
      .inputPass = NULL,
      .objdraw = &ledMode_draw
    }

    // TODO
    // change preset
  },

  .listIndex = 3
};

listDisplay_Interact _wifiinfo_list;



//        Additional handling
void _onacceptbar_accept(){
  disphand.act_accept();
}

void _pairmodelist(listDisplay_Interact**, void*){
  if(_pairmode_b)
    _cancelPairing();
  else
    _doPairing();
}


void _mediacontrollist(listDisplay_Interact **list, void *obj){

}


void _ledMode_setStr(preset_colorMode mode){
  const char *modestr = NULL;
  switch(mode){
    break; case preset_colorMode::Static_c:
      modestr = "Static";
    
    break; case preset_colorMode::RGB_p:
      modestr = "RGB";
    
    break; case preset_colorMode::Sound:
      modestr = "Sound";
    
    break; case preset_colorMode::_StopVis:
      modestr = "LED Off";
  }

  ledMode_draw.useText2(modestr);
}

void _ledmodelist(listDisplay_Interact **list, void *obj){
  int _mode = (int)vis->getLedMode()+1;
  if(_mode > (int)preset_colorMode::_StopVis)
    _mode = (int)preset_colorMode::Static_c;
    
  _ledMode_setStr((preset_colorMode)_mode);
  vis->setLedMode((preset_colorMode)_mode);
}


void _onwifistate(WifiState state){
  switch(state){
    break; case Connected:{
      disphand.importanttext("Connected");
      _ipaddrstr = WiFi.localIP().toString();
      Serial.printf("IP: %s\n", _ipaddrstr.c_str());
      sprintf(_txt_inetaddress, "IP: %s", _ipaddrstr.c_str());
      mc_inetaddress.changeTextType(draw_text::Top_Bottom);
      mc_inetaddress.useText(_txt_iaddr_connect, _txt_inetaddress);

      if(_wifi_authidx < _wifiinfo_list.listinteract.size())
        ((draw_text*)_wifiinfo_list.listinteract[_wifi_authidx].objdraw)->useText2(WIFISTATE_CONNECT_TEXT);
    }
    
    break; case Disconnected:{
      disphand.importanttext("Disconnected");
      mc_inetaddress.changeTextType(draw_text::Center);
      mc_inetaddress.useText(_txt_iaddr_disconnect, NULL);
      
      if(_wifi_authidx < _wifiinfo_list.listinteract.size())
        ((draw_text*)_wifiinfo_list.listinteract[_wifi_authidx].objdraw)->useText2(WIFISTATE_DISCONNECT_TEXT);
    }
    
    break; case CantConnect:{
      disphand.importanttext("Can't connect");
      mc_inetaddress.changeTextType(draw_text::Center);

      if(_wifi_authidx < _wifiinfo_list.listinteract.size())
        ((draw_text*)_wifiinfo_list.listinteract[_wifi_authidx].objdraw)->useText2(WIFISTATE_DISCONNECT_TEXT);

      _wifi_authidx++;
      if(_wifi_authidx >= PWD_alternateCount())
        _wifi_authidx = 0;

      auto _pauth = PWD_getAuth(_wifi_authidx);

      _conntostr = _pauth.first;
      mc_inetaddress.useText("Connecting to...", _conntostr.c_str());

      wifi_change(_pauth.first, _pauth.second);
    }

    break; case Waiting:{
      if(_wifi_authidx < _wifiinfo_list.listinteract.size())
        ((draw_text*)_wifiinfo_list.listinteract[_wifi_authidx].objdraw)->useText2(WIFISTATE_CONNECTING_TEXT);
    }
  }
}

void _wifistatelist_onaccept(listDisplay_Interact **list, void *obj){
  ByteIterator _bi(reinterpret_cast<const char*>(obj), sizeof(uint16_t)+sizeof(uint64_t));
  uint16_t idx; _bi.getVar(idx);
  uint64_t _straddr; _bi.getVar(_straddr);

  const char *strdata = (const char*)_straddr;
  if(idx == _wifi_authidx){
    switch(wifi_currentstate()){
      // don't use, since when it's in this state, it will reconnect to another wifi
      //break; case CantConnect:

      break; case Disconnected:
        wifi_reconnect();
      
      break; case Connected:
      case Waiting:
        wifi_disconnect();
    }
  }
  else{
    auto _pauth = PWD_getAuth(idx);
    wifi_change(_pauth.first, _pauth.second);
  }
}

void _wifistatelist(listDisplay_Interact **list, void *obj){
  *list = &_wifiinfo_list;
  Serial.printf("wifiinfo list size %d\n", _wifiinfo_list.listinteract.size());
  for(int i = 0; i < _wifiinfo_list.listinteract.size(); i++){
    auto &_list = _wifiinfo_list.listinteract[i];

    ByteIterator _bi(reinterpret_cast<const char*>(_list.additionalObj), sizeof(uint16_t)+sizeof(uint64_t));
    _bi.seek(sizeof(uint16_t));
    uint64_t _straddr; _bi.getVar(_straddr);
    
    free((void*)_straddr);
    free(_list.additionalObj);

    delete (draw_text*)_list.objdraw;
  }

  _wifiinfo_list.listinteract.resize(PWD_alternateCount());
  Serial.printf("setting up\n");
  for(int i = 0; i < PWD_alternateCount(); i++){
    auto &_interact = _wifiinfo_list.listinteract[i];

    draw_text *dtext = new draw_text();
    
    auto _pauth = PWD_getAuth(i);
    size_t _strwifilen = strlen(_pauth.first);
    bool _constrained = false;
    if(_strwifilen >= DISP_WIFINAME_MAX){
      _strwifilen = DISP_WIFINAME_MAX;
      _constrained = true;
    }

    char *_strwifi = (char*)malloc(_strwifilen+1);
    for(int i = 0; i < _strwifilen; i++){
      _strwifi[i] = _pauth.first[i];
      if(_constrained && i >=DISP_WIFINAME_CONSTRAINT)
        _strwifi[i] = '.';
    }
    _strwifi[_strwifilen] = '\0';

    const char *_strwifistate = WIFISTATE_DISCONNECT_TEXT;
    if(i == _wifi_authidx){
      switch(wifi_currentstate()){
        break; case WifiState::CantConnect:
        case WifiState::Disconnected:
          _strwifistate = WIFISTATE_DISCONNECT_TEXT;
        
        break; case WifiState::Connected:
          _strwifistate = WIFISTATE_CONNECT_TEXT;
        
        break; case WifiState::Waiting:
          _strwifistate = WIFISTATE_CONNECTING_TEXT;
      }
    }
    
    dtext->useText(_strwifi, _strwifistate);
    dtext->useColor(WHITE, WHITE);
    dtext->useSize(2, 1);
    dtext->changeTextType(draw_text::TextType::Top_Bottom);

    char *_additionalObj = (char*)malloc(sizeof(uint16_t) + sizeof(uint64_t));
    ByteIteratorR _bir(_additionalObj, sizeof(uint16_t)+sizeof(uint64_t));
    _bir.setVar((uint16_t)i);
    _bir.setVar((uint64_t)_strwifi);

    _interact.onClicked = _wifistatelist_onaccept;
    _interact.additionalObj = _additionalObj;
    _interact.useFixedName = false;
    _interact.inputPass = NULL;
    _interact.objdraw = dtext;
  }

  _wifiinfo_list.listIndex = _wifi_authidx;
}


void _vis_update(void*){
  if(_vis_doUpdate){
    if(vis->getLedMode() != preset_colorMode::_StopVis || !_vis_ledoff){
      if(vis->getLedMode() != preset_colorMode::_StopVis)
        _vis_ledoff = false;

      vis->update();
      led_sendcols(vis->getData());
    }
  }
}

void _vis_changerate(uint8_t rate){
  timer_changeInterval(_vis_timerid, 1000/rate);
}

void _init_visualizer(){
  _vis_doUpdate = true;
  PresetData.initPreset();
  vis = new visualizer(CONST_LEDNUM);
  _vis_timerid = timer_setInterval(1000/VIS_DEFRATE, _vis_update, NULL);
}

void _setupdrawings(){
  // media control draw setup
  mediaCtrl_draw.useText("Media", "Control");
  mediaCtrl_draw.useSize(2, 1);
  mediaCtrl_draw.useColor(WHITE, WHITE);
  mediaCtrl_draw.changeTextType(draw_text::TextType::Top_Bottom);

  ledMode_draw.useText("Led Mode", NULL);
  ledMode_draw.useSize(2, 1);
  ledMode_draw.useColor(WHITE, WHITE);
  ledMode_draw.changeTextType(draw_text::TextType::Top_Bottom);

  pairMode_draw.useDotLoad(false, true);
  pairMode_draw.useText("Pair", "pairing");
  pairMode_draw.useText_applied("done pairing");
  pairMode_draw.useText_cancelled("cancelled");
  pairMode_draw.useSize(2,1);
  pairMode_draw.useColor(WHITE, WHITE);
  pairMode_draw.changeTextType(draw_text::TextType::Top_Bottom);
}



/*      Precision timing      */
#define TIME_POLLID 0x1
void IRAM_ATTR _onTimeInterval(){
  _secondsElapsed++;
}

void _updateTime(){
  dtime.settime(_currtime);
}

void _timePolling(void*){
  if(_secondsElapsed > 0){
    _currtime = _currtime + TimeSpan(_secondsElapsed);
    _secondsElapsed = 0;
    _updateTime();
  }
}

void _setupTime(){
  // setting up rtc
  _currtime = DateTime(F(__DATE__), F(__TIME__));
  /*
  _rtc_found = true;
  if(!rtc.begin()){
    Serial.printf("can't find rtc\n");
    _rtc_found = false;
  }

  if(_rtc_found && !rtc.isrunning())
    rtc.adjust(_currtime);
  */

  _updateTime();

  // precision timer setup
  timer1_attachInterrupt(_onTimeInterval);
  timer1_write(1000000 * 5);                      // 1s * 5 ticks/us
  timer1_enable(TIM_DIV16, TIM_EDGE, TIM_LOOP);

  // polling for time draw
  polling_addfunc(_timePolling, (void*)TIME_POLLID);
}


#ifdef DO_TEST_SKETCH
  #ifdef DO_TEST_TIMER
    int _idint1s = -1;
    int _idint10s = -1;

    void _onTimeout20s(void*){
      Serial.printf("Timeout 20s\ndeleting 10s\n");
      timer_deleteTimer(_idint10s);
    }

    void _onTimeout10s(void*){
      Serial.printf("Timeout 10s\ndeleting 1s\n");
      timer_deleteTimer(_idint1s);

      _idint1s = -1;
    }

    void _onInterval1s(void*){
      Serial.printf("Interval 1s\n");
    }

    void _onInterval7s(void*){
      Serial.printf("Interval 7s\n");
      if(_idint1s < 0){
        Serial.printf("setting another timer\n");
        _idint1s = timer_setInterval(1000, _onInterval1s, NULL);
        _idint10s = timer_setTimeout(10000, _onTimeout10s, NULL);
      }
    }

    void test_timer(){
      timer_setInterval(7000, _onInterval7s, NULL);
      timer_setInterval(20000, _onTimeout20s, NULL);
    }
  #endif

  #ifdef DO_TEST_EEPROM
    #define __TEST_EEPROM_BUFSIZE 64
    char __te_buffer[__TEST_EEPROM_BUFSIZE];
    int __te_n = 0;
    size_t __te_i = 0;
    size_t __te_errorcount = 0;
    size_t __te_eepromsize;

    unsigned long __te_starttime;

    void __te_done(){
      unsigned long __te_stoptime = millis();

      Serial.printf("Done testing EEPROM.\n");
      Serial.printf("Testing time %d.%dms", __te_stoptime/1000, __te_stoptime%1000);
      Serial.printf("\n");
      if(__te_errorcount > 0)
        Serial.printf("While testing, there's %d errors.\n", __te_errorcount);
      else
        Serial.printf("No error occured while testing.\n");
    }

    void __te__wiperead(void*){
      if(__te_i < __te_eepromsize){
        for(size_t i = 0; i < __TEST_EEPROM_BUFSIZE; i++)
          if(__te_buffer[i] != 0xFF){
            Serial.printf("\nCan't wipe contents of address 0x%X\n\n", __te_i+i-__TEST_EEPROM_BUFSIZE);
            __te_errorcount++;
          }
          
        _eeprom.bufReadAsync(__te_i, __te_buffer, __TEST_EEPROM_BUFSIZE, __te__wiperead, NULL);
        __te_i += __TEST_EEPROM_BUFSIZE;
      }
      else
        __te_done();
    }

    void __te_wiperead(){
      Serial.printf("Wiping checking...\n");

      _eeprom.bufReadAsync(0, __te_buffer, __TEST_EEPROM_BUFSIZE, __te__wiperead, NULL);

      __te_i = __TEST_EEPROM_BUFSIZE;
      __te_n = 0;
    }

    void __te__wipewrite(void*){
      if(__te_i < __te_eepromsize){
        _eeprom.bufWriteAsync(__te_i, __te_buffer, __TEST_EEPROM_BUFSIZE, __te__wipewrite, NULL);
        __te_i += __TEST_EEPROM_BUFSIZE;
      }
      else
        __te_wiperead();
    }

    void __te_wipewrite(){
      Serial.printf("Wiping EEPROM contents...\n");

      __te_i = 0;
      __te_n = 0;

      for(size_t i = 0; i < __TEST_EEPROM_BUFSIZE; i++)
        __te_buffer[i] = 0xFF;
      __te__wipewrite(NULL);
    }

    void __te__read(void*){
      if(__te_i < __te_eepromsize){
        for(size_t bi = 0; bi < __TEST_EEPROM_BUFSIZE; bi += sizeof(int)){
          int _num; memcpy(&_num, __te_buffer+bi, sizeof(int));
          if(__te_n != _num){
            Serial.printf("\nSomething wrong at number %d.\nAddress 0x%X\n\n", __te_n, __te_i+bi-__TEST_EEPROM_BUFSIZE);
            __te_errorcount++;
          }

          __te_n++;
        }

        _eeprom.bufReadAsync(__te_i, __te_buffer, __TEST_EEPROM_BUFSIZE, __te__read, NULL);

        __te_i += __TEST_EEPROM_BUFSIZE;
      }
      else
        __te_wipewrite();
    }

    void __te_read(){
      Serial.printf("Reading and checking the contents...\n");

      _eeprom.bufReadAsync(0, __te_buffer, __TEST_EEPROM_BUFSIZE, __te__read, NULL);

      __te_i = __TEST_EEPROM_BUFSIZE;
      __te_n = 0;
    }

    void __te__write(void*){
      if(__te_i < __te_eepromsize){
        for(size_t bi = 0; bi < __TEST_EEPROM_BUFSIZE; bi += sizeof(int)){
          memcpy(__te_buffer+bi, &__te_n, sizeof(int));
          __te_n++;
        }

        _eeprom.bufWriteAsync(__te_i, __te_buffer, __TEST_EEPROM_BUFSIZE, __te__write, NULL);

        __te_i += __TEST_EEPROM_BUFSIZE;
      }
      else
        __te_read();
    }

    void __te_write(){
      Serial.printf("Begin writing on eeprom...\n");

      __te_i = 0;
      __te_n = 0;
      __te__write(NULL);
    }
    
    void test_eeprom(){
      Serial.printf("This will begin full checking on EEPROM memory. All contents will be overwritten.\n");
      Serial.printf("EEPROM size: %d\n", _eeprom.getMemSize());

      __te_starttime = millis();
      __te_eepromsize = _eeprom.getMemSize();
      __te_write();
    }
  #endif

  #ifdef DO_TEST_EEPROM_RANDOM
    char __ter_buffer[DTER_BUFFERSIZE];
    char __ter_buffercheck[DTER_BUFFERSIZE];

    void __ter_done(void*){
      Serial.printf("Original buffer hexdump: ");
      for(size_t i = 0; i < DTER_BUFFERSIZE; i++)
        Serial.printf("%X ", __ter_buffer[i]);
      
      Serial.printf("\n\nRead buffer hexdump: ");
      for(size_t i = 0; i < DTER_BUFFERSIZE; i++)
        Serial.printf("%X ", __ter_buffercheck[i]);
      
      Serial.printf("\n\nChecking data...\n");
      for(size_t i = 0; i < DTER_BUFFERSIZE; i++){
        if(__ter_buffer[i] != __ter_buffercheck[i]){
          Serial.printf("False file content at %d (%X, %X). Addr: 0x%X\n", i, __ter_buffer[i], __ter_buffercheck[i], i+DTER_OFFSETADDR);
        }
      }
    }

    void __ter_read(void*){
      _eeprom.bufReadAsync(DTER_OFFSETADDR, __ter_buffercheck, DTER_BUFFERSIZE, __ter_done, NULL);
    }

    void test_eeprom_random(){
      Serial.printf("This will begin writing random data to EEPROM. Some of EEPROM contents will be overwritten.\n");
      
      srand(DTER_SEED);
      for(size_t i = 0; i < DTER_BUFFERSIZE; i += sizeof(int)){
        int _tmp = rand();
        memcpy(__ter_buffer+i, &_tmp, sizeof(int));
      }

      _eeprom.bufWriteAsync(DTER_OFFSETADDR, __ter_buffer, DTER_BUFFERSIZE, __ter_read, NULL);
    }
  #endif
#endif

void setup() {
  delay(5000);
  Serial.begin(9600);

  #ifdef DO_TEST_SKETCH
    #ifdef DO_TEST_TIMER
      test_timer();
    #endif

    #ifdef DO_TEST_EEPROM
      test_eeprom();
    #endif

    #ifdef DO_TEST_EEPROM_RANDOM
      test_eeprom_random();
    #endif
  #else

  auto _fserror = EEPROM_FS.bind_storage(&_eeprom);

  if(_fserror == fs_error::storage_not_initiated){
    Serial.printf("File system not detected in eeprom.\nInitializing...\n");
    EEPROM_FS.init_storage(&_eeprom);
  }

  Serial.printf("memory: %d\n", ESP.getFreeHeap());
  PWD_init();
  Serial.printf("test\n");

  Wire.begin();
  Wire.setClock(TWI_DEFAULTCLK);


  // setting up wifi
  // using WIFI_STA too slow after some time?
  WiFi.mode(WIFI_AP);
  wifi_start();

  _wifi_authidx = 0;
  auto _pauth = PWD_getAuth(0);
  wifi_change(_pauth.first, _pauth.second);
  wifi_setcallback(_onwifistate);

  server_init(USEPORT);
  set_socketcallback(callbacksock);
  set_socketconnstateCb(_onconnstatesock);

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

    _setupdrawings();
  }

  _setupTime();

  // setting up led
  Serial1.begin(LEDDRIVER_BAUDRATE);
  led_init(CONST_LEDNUM);

  // setting up visualizer
  _init_visualizer();
  _ledMode_setStr(vis->getLedMode());

  TimeRandom.supplyTime(_currtime.unixtime());

  // getting encryption key
  if(EEPROM_FS.file_exist(ENCRYPTKEY_FILEID)){
    size_t _keylen = EEPROM_FS.file_size(ENCRYPTKEY_FILEID);
    char _key[_keylen];
    EEPROM_FS.read_file(ENCRYPTKEY_FILEID, _key, NULL, NULL);
    EEPROM_FS.complete_tasks();

    Serial.printf("key bin dump\n");
    for(int i = 0; i < _keylen; i++)
      Serial.printf("%X ", _key[i]);

    crypt.useKey(_key, _keylen);
  }
  else{
    _is_paired = false;
    _doPairing();
  }

  timer_setInterval(BATTERYCHECK_INTERVAL, _batteryUpdate_sw, NULL);

  #endif
}

void loop() {
  _run_anothertask();
}