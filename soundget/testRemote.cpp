#include "winsock2.h"
#include "stdio.h"
#include "chrono"
#include "thread"

WSAData *wsadat = NULL;

void closeSocket(){
  delete wsadat;
  WSACleanup();
}

void initSocket(){
  wsadat = new WSADATA();
  int errcode = WSAStartup(MAKEWORD(2,2), wsadat);
  if(errcode != 0){
    printf("Error setting up winsock.\n"); 
    throw;
  }
}

const unsigned short port = 3020;
sockaddr_in *hostAddress = new sockaddr_in();


int main(){
  initSocket();

  hostAddress->sin_addr.S_un.S_addr = inet_addr("192.168.76.227");
  hostAddress->sin_family = AF_INET;
  hostAddress->sin_port = htons(port);

  float pingrate = 0.f;
  float pingmin = 0.f;
  float pingmax = 0.f;

  auto delay = std::chrono::microseconds(1000000/60);
  for(int i = 0; i < 100; i++){
    auto starttime = std::chrono::high_resolution_clock::now();
    SOCKET clientSock = socket(AF_INET, SOCK_DGRAM, 0);
    char str[] = "testing";
    int strsize = strlen(str);
    int errcode = sendto(clientSock, str, strlen(str), 0, (sockaddr*)hostAddress, sizeof(sockaddr_in));
    //printf("packet sent.\n");

    int packetLen = 0;
    int sockaddrinlen = sizeof(sockaddr_in);
    errcode = recvfrom(clientSock, reinterpret_cast<char*>(&packetLen), sizeof(int), 0, (sockaddr*)hostAddress, &sockaddrinlen);

    //printf("packet size: %d\n", packetLen);
    char buf[packetLen+1];
    errcode = recvfrom(clientSock, buf, packetLen, 0, (sockaddr*)hostAddress, &sockaddrinlen);

    buf[packetLen] = '\0';
    //printf("packet data: %s\n", buf);

    auto finishtime = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> time = finishtime - starttime;

    
    double dtime = time.count();

    if(i == 1)
      pingrate = pingmax = pingmin = dtime;
    else{
      if(pingmax < dtime)
        pingmax = dtime;
      
      if(pingmin > dtime)
        pingmin = dtime;
    }
    
    pingrate += dtime;
    pingrate /= 2;
    
    std::this_thread::sleep_for(delay-time);
  }

  printf("Testing ping 100 times.\n");
  printf("Ping rate: %f s\n", pingrate);
  printf("Minimal ping time: %f s\n", pingmin);
  printf("Maximal ping time: %f s\n", pingmax);

  closeSocket();
}