#include "SPI.h"
#include "Wire.h"
#include <Arduino.h>
#include "ESP8266WiFi.h"

#include "timer.hpp"

#include "Adafruit_GFX.h"
#include "Adafruit_SSD1306.h"
#include "FastLED.h"

#include "string"
#include "cmath"

#include "socket_udp.hpp"
#include "ByteIterator.hpp"
#include "sensor4.hpp"
#include "password.hpp"

using namespace std;

#define USEPORT 3020
#define MAX_23BIT 0x7FFFFF

#define SSD1306_NO_SPLASH


// Note: i don't know how to get packet size, so each packet has header packet (size 4 bytes)
// as the size of data packet

// TODO add an handler for each drawing update
//  - like a callback for object that handle each draw
//  - maybe for this, don't use display.clear() if possible.
//    since it might be a bit costly to resource
//    use display.drawFillBox() instead.
// TODO try interface EEPROM again
// TODO test interfacing all module
// TODO maybe add an interrupt display (like displaying an important notification)
// TODO find a way to get data from computer (i.e. Serial)
// TODO code for interfacing eeprom
// TODO lastly, try using the led

string ssid = getPassword();
string password = getSSID();


//    Socket testing
const unsigned short DO_TEST = 0x3;
const unsigned short DO_MTPLY = 0x4;
void callbacksock(const char *buf, int buflen){
  if(buflen >= sizeof(unsigned short)){
    ByteIterator _bi{buf, buflen};
    unsigned short msgcode = 0;
    Serial.printf("msgcode: 0x%X\n", msgcode);

    _bi.getVar(msgcode);
    Serial.printf("msgcode msg %d\n", msgcode);
    switch(msgcode){
      break; case DO_TEST:{
        Serial.printf("Sending test message.\n");
        char teststr[] = "Hello from esp8266!";
        char *strbuf = (char*)malloc(sizeof(teststr));
        memcpy(strbuf, teststr, sizeof(teststr));
        udpSock.queue_data(strbuf, sizeof(teststr));
      }

      break; case DO_MTPLY:{
        int i1 = 0;
        int i2 = 0;

        _bi.getVar(i1);
        _bi.getVar(i2);

        int res = i1*i2;
        Serial.printf("result: %d\n", res);
        string resstr = to_string(res);
        char *buffer = (char*)malloc(resstr.length()+1);
        buffer[resstr.length()] = '\0';
        memcpy(buffer, resstr.c_str(), resstr.length());
        udpSock.queue_data(buffer, resstr.length());
      }
    }
  }
}



//      Wifi handling
#define WIFI_UPDATEMS 5000
#define MAX_WIFIWAITCONNECT 30000
enum WifiState{
  Connected,
  CantConnect,
  Disconnected,
  Waiting
};

IPAddress _nulip{0};
typedef void (*WifiStateCallback)(WifiState state);
void _wifidmpcb(WifiState state){}
IPAddress wifi_currentWifi = _nulip;
unsigned short wifi_currentPort = 0;
WifiState wifi_currentState = WifiState::Disconnected;
WifiStateCallback _wifi_cb = _wifidmpcb;

unsigned long _wifi_firstConnect = 0;

void wifi_update(void *obj){
  IPAddress currip = WiFi.localIP();
  if(wifi_currentState == Waiting){
    if(currip != _nulip){
      wifi_currentState = Connected;
      _wifi_cb(wifi_currentState);
    }
    else{
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
}

void wifi_start(){
  timer_setInterval(WIFI_UPDATEMS, wifi_update, NULL);
}

void wifi_change(const char* ssid, const char *pass){
  if(wifi_currentState == Connected)
    WiFi.disconnect();

  WiFi.begin(ssid, pass);
  _wifi_firstConnect = millis();
  wifi_currentState = Waiting;
  _wifi_cb(wifi_currentState);
}

void wifi_setcallback(WifiStateCallback cb){
  _wifi_cb = cb;
}



//      Display testing
#define SSD1306_X 128
#define SSD1306_Y 64
Adafruit_SSD1306 display(128, 64, &Wire);

const char strscroll[] = "Time Elapsed";
const int refreshRate = 30;
const int step = 1;

char timedatastr[64];

uint16_t txtWidth = 0, txtHeight = 0;
int16_t currx = 0, curry = 0;
void callbackDisp(void *obj){
  int16_t _dmp = 0;
  curry = 0;
  currx += step;

  display.clearDisplay();
  
  display.setTextSize(2);
  display.setCursor(currx, curry);
  display.printf(strscroll);

  display.setCursor(currx - (txtWidth*2) - 8, curry);
  display.printf(strscroll);

  curry += txtHeight*2+2;

  sprintf(timedatastr, "%d:%d:%d", millis()/60000, (millis()/1000)%60, millis()%1000);
  uint16_t tdatstrw = 0, tdatstrh = 0;
  display.getTextBounds(timedatastr, 0, 0, &_dmp, &_dmp, &tdatstrw, &tdatstrh);
  display.setTextSize(1);
  display.setCursor((SSD1306_X/2)-(tdatstrw/4), curry);
  display.printf("%s", timedatastr);

  
  display.display();

  if(currx >= SSD1306_X)
    currx = SSD1306_X - txtWidth*2 - 8;
}

void callbackSensor(sensor_actType acttype){

}


char *test = NULL;

void setup() {
  Wire.begin();
  Serial.begin(9600);
  test = (char*)calloc(1, 1024);

  /*
  IPAddress locip = WiFi.localIP();

  while(locip == nulip){
    Serial.println("Connecting...");
    WiFi.begin(ssid.c_str(), password.c_str());
    locip = WiFi.localIP();
    delay(5000);
  }
  
  Serial.println("Connected.");
  Serial.print("Local IP: ");
  Serial.println(locip.toString());

  udpSock.set_callback(callbacksock);
  udpSock.startlistening(USEPORT);


  char t[20] = "Hello";
  for(int i = 0; i < 10; i++){
    Wire.beginTransmission(0x50);
    Wire.write((uint16_t)0x0);
    Wire.write((uint8_t*)t, 6);
    Wire.endTransmission();
    delay(10);
  }
  */

  uint8_t sensor4_pindata[] = {D5, D6, D7, D8};
  sensor4_setcallback(callbackSensor);
  sensor4_init(sensor4_pindata);
  
  if(display.begin(SSD1306_SWITCHCAPVCC, 0x3C)){
    display.display();
    delay(1500);
    display.clearDisplay();

    int16_t _dummy = 0;
    display.getTextBounds(strscroll, 0, 0, &_dummy, &_dummy, &txtWidth, &txtHeight);
    display.setTextWrap(false);
    display.setTextColor(SSD1306_WHITE);
    currx = (SSD1306_X/2)-(txtWidth/2);

    timer_setInterval(1000/refreshRate, callbackDisp, NULL);
  }

  /*
  delay(5000);
  EEPROM_AT24C256 eeprom{0x0};

  char *t = (char*)malloc(30);

  eeprom.writeAsync(0x30, t, 28);
  delay(2000);
  Serial.printf("done delaying\n");
  */
}

bool doCheck = true;
void loop() {
  timer_update();

  for(size_t i = 0; doCheck && i < 1024; i++)
    if(test[i] != 0){
      doCheck = false;
      Serial.printf("the memory is corrupted. in %d\n", i);
      break;
    }
}