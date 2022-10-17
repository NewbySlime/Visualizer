#ifndef SOCKET_UDP
#define SOCKET_UDP

#include "vector"
#include "WiFiUdp.h"

#define MAX_UDPPACKETLEN 512
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

    int _isrtimer_num = -1;
    unsigned short _maxmsgsize = MAX_UDPMSGLEN;
    int _intervalms;
    int _maxTimesRespond = 0;
    int _timesNotRespond = 0;

    bool _alreadyconnected = false;
    bool _doCloseSocket = false;
    char *buffer;
    char *msgheader;

    // functions for setting all the parameters
    void _set_pollingrate(unsigned char rateHz);
    void _set_maxmsgsize(unsigned short maxmsgsize);


    static void _onTimer(void *pobj);
    void _onTimer();
    void _onUdpConnect();
    void _onUdpDisconnect();
    void _onUdpClose();
    void _queueMessage(unsigned short msgcode, void* message, unsigned short msglen);
    bool _checkIfSameRemoteHost();
    int _parsePacketsUntilCorrect();

  public:
    SocketUDP();
    ~SocketUDP();
    void startlistening(unsigned short port);

    // send disconnect message and makes it available to connect to another address
    void disconnect();

    // aka stop listening
    void close_socket();

    bool isconnected();

    void set_pollingrate(unsigned char rateHz);
    void set_msgmaxsize(unsigned short size);
    // The callback function will be called when a packet is received
    // free the data after use
    void set_callback(SocketUDP_Callback callback);

    // use malloc or the quite instead of new
    // don't free data, since it uses pointer of the buffer, not the content of the buffer
    void queue_data(char* data, unsigned short datalength);
};


extern SocketUDP udpSock;

#endif