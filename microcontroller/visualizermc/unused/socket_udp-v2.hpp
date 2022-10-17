#ifndef SOCKET_UDP
#define SOCKET_UDP

#include "vector"
#include "WiFiUdp.h"

#define MAX_UDPPACKETLEN 128
#define MAX_UDPMSGLEN 2048
#define MAX_DATAQUEUE 16
#define UDPS_DEFAULTPOLLRATE 30

typedef void (*SocketUDP_Callback)(const char*, int);

class SocketUDP{
  private:
    struct _message{
      unsigned short msgcode;
      void* buffer;
      unsigned short bufferlen;
    };

    WiFiUDP wifiudp;
    IPAddress remoteIP;
    unsigned short remotePort;
    SocketUDP_Callback callbackOnPacket;
    std::vector<_message> appendedMessages;

    unsigned short _maxmsgsize = MAX_UDPMSGLEN;
    bool _responded = false;
    unsigned long _lasttimeresp = 0;

    bool _keeplistening = false;
    bool _alreadyconnected = false;
    bool _doCloseSocket = false;
    bool _recvMsg = false, _sendMsg = false;
    char *buffer;
    char *msgheader;

    // functions for setting all the parameters
    void _set_maxmsgsize(unsigned short maxmsgsize);

    void _onUdpConnect();
    void _onUdpDisconnect();
    void _onUdpClose();
    void _onNotResponded();
    void _queueMessage(unsigned short msgcode, void* message, unsigned short msglen);
    bool _checkIfSameRemoteHost();
    int _parsePacketsUntilCorrect();

  public:
    void (*OnConnected)();
    void (*OnDisconnected)();

    SocketUDP();
    ~SocketUDP();
    void startlistening(unsigned short port);

    // send disconnect message and makes it available to connect to another address
    void disconnect();

    // aka stop listening
    void close_socket();

    bool isconnected();

    void set_msgmaxsize(unsigned short size);
    // The callback function will be called when a packet is received
    // free the data after use
    void set_callback(SocketUDP_Callback callback);

    // use malloc or the quite instead of new
    // don't free data, since it uses pointer of the buffer, not the content of the buffer
    void queue_data(char* data, unsigned short datalength);

    void update_socket();
};


extern SocketUDP udpSock;

#endif