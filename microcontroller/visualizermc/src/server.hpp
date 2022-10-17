#ifndef SERVER_HEADER
#define SERVER_HEADER

#include "Arduino.h"
#include "WiFiServer.h"
#include "WiFiClient.h"

#define MAX_DATALEN 2048
#define HEADER_DATALEN sizeof(uint16_t) + sizeof(int)
#define MAX_DATAQUEUE 16

enum sock_connstate{
  connected,
  disconnected
};

typedef void (*S_Cb_ondata)(const char *data, int datalen);
typedef void (*S_Cb_onconn)(sock_connstate state);

void server_update();
void server_init(uint16_t port);
void socket_queuedata(char *data, int datalen);
void socket_disconnect();
void set_socketcallback(S_Cb_ondata cb);
void set_socketconnstateCb(S_Cb_onconn cb);


#endif