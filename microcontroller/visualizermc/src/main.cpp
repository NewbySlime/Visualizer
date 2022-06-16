#include <Arduino.h>
#include "SPI.h"
#include "Wire.h"
#include "ESP8266WiFi.h"
#include "ESP8266WebServer.h"
#include "WiFiUdp.h"

#include "Adafruit_GFX.h"
#include "Adafruit_SSD1306.h"
#include "ESP8266TimerInterrupt.h"
#include "ESP8266_ISR_Timer.hpp"
#include "FastLED.h"
#include "serialEEPROM.h"

#include "string"
#include "cmath"

using namespace std;

#define CONNECTION_UDP 0
#define CONNECTION_TCP 1
#define CONNECTION_WEB 2

#define MAX_23BIT 0x7FFFFF

#define CURRENTCONNECTION CONNECTION_UDP


// Note: i don't know how to get packet size, so each packet has header packet (size 4 bytes)
// as the size of data packet


string ssid = ".o.";
string password = "QbllolymLtoRiwqieCRIrzwCr";

void udp_onsetup();
void udp_onupdate();

void tcp_onsetup();
void tcp_onupdate();

void web_onsetup();
void tcp_onupdate();

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

  #if CURRENTCONNECTION == CONNECTION_UDP
    udp_onsetup();
  #elif CURRENTCONNECTION == CONNECTION_TCP
    tcp_onsetup();
  #elif CURRENTCONNECTION == CONNECTION_WEB
    web_onsetup();
  #endif

  timer1_enable(1, 0, true);
  timer1_attachInterrupt(onInterrupt);
  timer1_write(timer_getTimerStartValue(MAX_23BIT, 1, (double)1/60));
  timer1_isr_init();
}

void loop() {
  ESP8266Timer timer;
  ESP8266_ISR_Timer isr_timer;
}


#if CURRENTCONNECTION == CONNECTION_UDP
WiFiUDP wudp;
void udp_onsetup(){
  wudp.begin(3020);
}

void udp_onupdate(){
  int packetSize = wudp.parsePacket();
  if(packetSize > 0){
    IPAddress ipad = wudp.remoteIP();
    unsigned short remport = wudp.remotePort();
    //Serial.println("Packet received.");
    //Serial.printf("Packet Length: %d\n", packetSize);
    char *str = new char[packetSize+1];
    wudp.read(str, packetSize);
    str[packetSize] = '\0';
    //Serial.println(str);

    //Serial.printf("incoming ip address: %s\n", ipad.toString().c_str());
    //Serial.printf("incoming port: %d\n", remport);
    string strs = "hello from esp8266!";

    wudp.beginPacket(ipad, remport);
    int strslen = strs.length();
    wudp.write(reinterpret_cast<char*>(&strslen), sizeof(int));
    wudp.endPacket();

    wudp.beginPacket(ipad, remport);
    wudp.write(strs.c_str(), strs.length());
    wudp.endPacket();

    //Serial.println("Packet sent.");
  }
}


#elif CURRENTCONNECTION == CONNECTION_TCP
WiFiServer server(3020);
WiFiClient currentclient;
void tcp_onsetup(){
  server.begin();
  while(!currentclient){
    Serial.println("Waiting for socket...");
    currentclient = server.available();
    if(currentclient.connected()){
      Serial.println("Connected");
      
    }
    delay(1000);
  }

  Serial.println("Socket connected!");
}

void tcp_onupdate(){
  Serial.println("Waiting for a message...");
  currentclient.read();
}


#elif CURRENTCONNECTION == CONNECTION_WEB
void web_onsetup(){

}

void web_onupdate(){

}
#endif