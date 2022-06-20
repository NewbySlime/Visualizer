#include "SPI.h"
#include "Wire.h"
#include <Arduino.h>
#include "ESP8266WiFi.h"
#include "WiFiUdp.h"

#include "timer.hpp"

#include "Adafruit_GFX.h"
#include "Adafruit_SSD1306.h"
#include "FastLED.h"
#include "serialEEPROM.h"

#include "string"
#include "cmath"

#include "socket_udp.hpp"
#include "ByteIterator.hpp"

using namespace std;

#define USEPORT 3020
#define MAX_23BIT 0x7FFFFF


// Note: i don't know how to get packet size, so each packet has header packet (size 4 bytes)
// as the size of data packet

string ssid = ".o.";
string password = "QbllolymLtoRiwqieCRIrzwCr";

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
        char *strbuf = new char[sizeof(teststr)];
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
        char *buffer = new char[resstr.length()+1];
        buffer[resstr.length()] = '\0';
        memcpy(buffer, resstr.c_str(), resstr.length());
        udpSock.queue_data(buffer, resstr.length());
      }
    }
  }
}

void setup() {
  Serial.begin(9600);
  Serial.println("test");

  IPAddress locip = WiFi.localIP();
  IPAddress nulip{0};

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
}

void loop() {
  timer_update();
}