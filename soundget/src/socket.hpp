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

#define DEFAULT_SOCKETCHECKRATE 30


// this class only handles a socket, which doesn't work on multiple oncoming socket connection
// and handles it synchronously, even if one of the functions is called in another thread
class SocketHandler_Sync{
  private:
    sockaddr_in *hostAddress = NULL;
    SOCKET currSock;
    std::mutex sMutex;
    bool stopSending = true;
    int lastErrcode = 0;

    bool _checkingDelay = false;
    unsigned long pingDelayMs = 0;
    std::chrono::_V2::system_clock::time_point _starttime;

    std::queue<std::pair<char*, int>> messageQueues;

    bool _responded = true;
    std::chrono::_V2::system_clock::time_point _starttimeout;

    
    bool _checkrecv();
    void _onnotresponding();
    void _onresponding();

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

    void Disconnect();

    // only this function that is not thread-safe
    // the reason is in order to send datas in seperate function
    // to make it thread safe, use mutex from GetMutex() function
    void SendData(const void *data, int datalength);
    void SafeDelete();

    unsigned long LastPing();

    void ChangeAddress(unsigned short port, const char *ip = "127.0.0.1");
};

#endif