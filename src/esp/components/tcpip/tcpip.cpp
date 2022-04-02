#include <stdio.h>
#include <string.h>
#include <unordered_map>

#include "trs-io.h"
#include "tcpip.h"

#ifdef ESP_PLATFORM

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_log.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"

#else

#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>

#endif


/****

Commands are Request/Response

See docs/TCP_IP

For TCP: (each <item> represents a single byte)

<TCPIP><SOCKET><FAMILY><TYPE><VERSION>
<TCPIP><CONNECT><SOCKFD><HOST FORMAT><IP4><IP3><IP2><IP1><PORT2><PORT1>
<TCPIP><SEND><SOCKFD><L4><L3><L2><L1><DATA...>
<TCPIP><RECV><SOCKFD><L4><L3><L2><L1>
<TCPIP><CLOSE><SOCKFD>

****/

using namespace std;


typedef struct {
  int fd;
  int family;
} SocketInfo;

#define TCPIP_MODULE_ID 1

class TCPIPModule : public TrsIO {
private:
  uint8_t clientVersionMajor;
  uint8_t clientVersionMinor;
  
  uint8_t nextSocketFd = 0;
  unordered_map<uint8_t, SocketInfo> socketMap;
  
public:
  TCPIPModule(int id) : TrsIO(id) {
    addCommand(static_cast<cmd_t>(&TCPIPModule::doVersion), "BB");
    addCommand(static_cast<cmd_t>(&TCPIPModule::doSocket), "BB");
    addCommand(static_cast<cmd_t>(&TCPIPModule::doConnectIP), "BBBBBI");
    addCommand(static_cast<cmd_t>(&TCPIPModule::doConnectHost), "BSI");
    addCommand(static_cast<cmd_t>(&TCPIPModule::doSend), "BX");
    addCommand(static_cast<cmd_t>(&TCPIPModule::doSendTo), "");
    addCommand(static_cast<cmd_t>(&TCPIPModule::doRecv), "BBL");
    addCommand(static_cast<cmd_t>(&TCPIPModule::doRecvFrom), "");
    addCommand(static_cast<cmd_t>(&TCPIPModule::doClose), "B");
  }

  void doVersion() {
    clientVersionMajor = B(0);
    clientVersionMinor = B(1);
    addByte(IP_VERSION_MAJOR);
    addByte(IP_VERSION_MINOR);
  }

  int getSocketFamily(uint8_t t) {
    switch (t) {
    case IP_SOCKET_FAMILY_AF_INET:
      return AF_INET;
    case IP_SOCKET_FAMILY_AF_INET6:
      return AF_INET6;
    }
    return -1;
  }

  int getSocketType(uint8_t t) {
    switch(t) {
    case IP_SOCKET_TYPE_SOCK_STREAM:
      return SOCK_STREAM;
    case IP_SOCKET_TYPE_SOCK_DGRAM:
      return SOCK_DGRAM;
    }
    return -1;
  }

  void doSocket() {
    int ipSocketFamily = getSocketFamily(B(0)); 
    int ipSocketType = getSocketType(B(1));
    int socketFd = socket(ipSocketFamily, ipSocketType, 0);

    if (socketFd < 0) {
      addByte(IP_COMMAND_ERROR);
      addByte(errno);
    } else {
      uint8_t fd = nextSocketFd++;
      socketMap[fd].fd = socketFd;
      socketMap[fd].family = ipSocketFamily;
      addByte(IP_COMMAND_SUCCESS);
      addByte(fd);
    }
  }

  void doConnectHost() {
    int socketFd = socketMap[B(0)].fd;
    int ipSocketFamily = socketMap[B(0)].family;
    const char* hostname = S(0);
    short port = I(0);
    
    struct hostent* server = gethostbyname(hostname);
  
    if (server == NULL) {
      addByte(IP_COMMAND_ERROR);
      addByte(IP_ERROR_UNKNOWN_HOST);
      return;
    }
    
    struct sockaddr_in serv_addr;
    bzero((char*) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = ipSocketFamily;
    bcopy((char*) server->h_addr,
          (char*) &serv_addr.sin_addr.s_addr,
          server->h_length);
    serv_addr.sin_port = htons(port);
    if (connect(socketFd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
      addByte(IP_COMMAND_ERROR);
      addByte(errno);
    } else {
      addByte(IP_COMMAND_SUCCESS);
    }
  }

  void doConnectIP() {
    int socketFd = socketMap[B(0)].fd;
    int ipSocketFamily = socketMap[B(0)].family;
    short port = I(0);

    char ipAddress[16];
    sprintf(ipAddress, "%d.%d.%d.%d", B(1), B(2), B(3), B(4));
    
    struct sockaddr_in tcpServerAddr;
    tcpServerAddr.sin_addr.s_addr = inet_addr(ipAddress);
    tcpServerAddr.sin_family = ipSocketFamily;
    tcpServerAddr.sin_port = htons(port);
      
    if (connect(socketFd, (struct sockaddr *) &tcpServerAddr, sizeof(tcpServerAddr)) < 0) {
      addByte(IP_COMMAND_ERROR);
      addByte(errno);
    } else {
      addByte(IP_COMMAND_SUCCESS);
    }
  }
  
  void doSend() {
    int socketFd = socketMap[B(0)].fd;
    uint32_t dataLength = XL(0);
    uint8_t* buffer = X(0);

    uint32_t left = dataLength;
    
    while(left > 0) {
      int wrote = write(socketFd, buffer, left);
      if (wrote < 0) {
        addByte(IP_COMMAND_ERROR);
        addByte(errno);
        return;
      }
      left -= wrote;
      buffer += wrote;
    }
    addByte(IP_COMMAND_SUCCESS);
    addLong(dataLength);
  }

  void doSendTo() {
    assert(0);
  }
  
  void doRecv() {
    int socketFd = socketMap[B(0)].fd;
    int recvOption = B(1);
    uint32_t length = L(0);
  
    int option = 0;
    switch(recvOption) {
    case IP_RECV_BLOCKING:
      option = MSG_WAITALL;
      break;
    case IP_RECV_NONBLOCKING:
      option = MSG_DONTWAIT;
      break;
    default:
      addByte(IP_COMMAND_ERROR);
      addByte(IP_ERROR_UNSUPPORTED_COMMAND_OPTION);
      return;
    }

    addByte(IP_COMMAND_SUCCESS);
    uint8_t* buf = startBlob32();
    if (length > getSendBufferFreeSize()) {
      length = getSendBufferFreeSize();
    }
    int bytesWritten = recv(socketFd, buf, length, option);
    if (bytesWritten == -1 && errno != EAGAIN && errno != EWOULDBLOCK) {
      rewind();
      addByte(IP_COMMAND_ERROR);
      addByte(errno);
      return;
    }
    if (bytesWritten == -1) {
      bytesWritten = 0;
    }
    skip(bytesWritten);
    endBlob32();
  }

  void doRecvFrom() {
    assert(0);
  }
  
  void doClose() {
    int socketFd = socketMap[B(0)].fd;
    int c = close(socketFd);
    socketMap.erase(B(0));
    if (c == -1) {
      addByte(IP_COMMAND_ERROR);
      addByte(errno);
      return;
    }
    addByte(IP_COMMAND_SUCCESS);
  }
};

TCPIPModule theTCPIPModule(TCPIP_MODULE_ID);
