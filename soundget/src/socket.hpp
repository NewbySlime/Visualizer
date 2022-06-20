#ifndef SOCKETHEADER
#define SOCKETHEADER

#include "winsock2.h"
#include "WS2tcpip.h"
#include "thread"
#include "mutex"
#include "condition_variable"
#include "utility"
#include "queue"


#define LockSocket(pObj) std::lock_guard<std::mutex> __lgs(pObj->GetMutex())
#define LockSocketN(pObj, var_name) std::lock_guard<std::mutex> var_name(pObj->GetMutex)

#define MAX_SOCKETPACKETSIZE 512
#define UDPS_DEFAULTPOLLRATE 30


// this class only handles a socket, which doesn't work on multiple oncoming socket connection
// and handles it synchronously, even if one of the functions is called in another thread
class SocketHandler_Sync{
  private:
    sockaddr_in *hostAddress;
    SOCKET currSock;
    std::mutex sMutex;
    bool stopSending = false;
    int lastErrcode = 0;

    std::queue<std::pair<char*, int>> messageQueues;
    

  public:
    SocketHandler_Sync(unsigned short port, const char *ip = "127.0.0.1");
    ~SocketHandler_Sync();

    // in case if need to lock it from another thread when sending data seperately
    std::mutex &GetMutex();

    // get next message after pinging the socket
    // user have to delete the buffer after using
    std::pair<char*, int> GetNextMessage();

    // check if there's another message in the socket
    bool IsAnotherMessage();

    // basically updating the socket, if there's a message from the listener,
    // the function return a pair of buffer of the msg and the length of it
    // if there's no message from the listener, the function return null buffer
    void PingSocket();

    // checking if connected based on last code when pinging or sending message
    bool IsConnected();

    // reconnecting socket for 5 times
    void ReconnectSocket();

    // only this function that is not thread-safe
    // the reason is in order to send datas in seperate function
    // to make it thread safe, use mutex from GetMutex() function
    void SendData(const void *data, int datalength);
    void SafeDelete();
};


// No function for closing the socket because it isn't a listener
class SocketHandlerUDP_Async{
  private:
    struct _message{
      public:
        unsigned short code;
        void* msg;
        unsigned short msglen;
    };

    sockaddr_in hostAddress;
    SOCKET currSock;

    std::chrono::milliseconds sleepTime;
    std::mutex m_messageQueues, m_onDisconnect;
    std::condition_variable cv_disconnect;
    std::thread udpthread;

    // carefull when the message buffer is actually null
    std::queue<_message> messageQueues;

    bool _actuallyConnected = false;
    int _maxmsgsize = -1;

    static void dumpfunc1(const char*,int){}
    static void dumpfunc2(){}


    // stopping a thread for sleepTime decrement by procduration
    void sleepThread(std::chrono::duration<double> procduration);

    void _QueueMessage(unsigned short code, void* msg = NULL, unsigned short msglen = 0);

    void on_udpdisconnect();
    void on_udppollrate(unsigned char rateHz);
    void on_udpmaxsize(unsigned short maxmsgsize);
    
    void udpthreadfunc();
    static void _udpthreadfunc(SocketHandlerUDP_Async *obj){
      obj->udpthreadfunc();
    }
    
  public:
    typedef void (*SocketCallback)(const char*, int);
    typedef void (*SocketCallbackvoid)();

    SocketHandlerUDP_Async();
    ~SocketHandlerUDP_Async();

    // returns false if can't connect to the socket
    // the program will block for around 1 sec
    bool StartConnecting(const char *ipaddress, unsigned short port);
    void QueueMessage(char* msg, int len);
    // this will also tell to remoteHost to change its pollrate
    void ChangePollrate(unsigned char rateHz);
    // don't delete the buffer when the callback called
    void SetCallback(SocketCallback cb);
    void SetCallback_disconnect(SocketCallbackvoid cb);
    void WaitUntilDisconnect();
    void Disconnect();
  
  
  private:
    SocketCallback callback;
    SocketCallbackvoid cbOnDisconnected;
};

#endif