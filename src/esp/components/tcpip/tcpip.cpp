#include <stdio.h>
#include <string.h>    //strlen

#include "trs-io.h"
#include "tcpip.h"

#ifdef ESP_PLATFORM

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
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


#define TCPIP_MODULE_ID 1

class TCPIPModule : public virtual TrsIO {
public:
 TCPIPModule(int id) : TrsIO(id) {
    addCommand(doVersion, "");
    addCommand(doSocket, "BB");
    addCommand(doConnectIP, "BBBBBI");
    addCommand(doConnectHost, "BBSI");
    addCommand(doSend, "BX");
    addCommand(doSendTo, "");
    addCommand(doRecv, "BBL");
    addCommand(doRecvFrom, "");
    addCommand(doClose, "B");
  }

  static void doVersion() {
    addByte(IP_VERSION_MAJOR);
    addByte(IP_VERSION_MINOR);
  }

  static int getSocketFamily(uint8_t t) {
    switch (t) {
    case IP_SOCKET_FAMILY_AF_INET:
      return AF_INET;
    case IP_SOCKET_FAMILY_AF_INET6:
      return AF_INET6;
    }
    return -1;
  }

  static int getSocketType(uint8_t t) {
    switch(t) {
    case IP_SOCKET_TYPE_SOCK_STREAM:
      return SOCK_STREAM;
    case IP_SOCKET_TYPE_SOCK_DGRAM:
      return SOCK_DGRAM;
    }
    return -1;
  }
    
  static void doSocket() {
    int ipSocketFamily = getSocketFamily(B(0)); 
    int ipSocketType = getSocketType(B(1));
    int socketFd = socket(ipSocketFamily, ipSocketType, 0);

    if (socketFd < 0) {
      addByte(IP_COMMAND_ERROR);
      addByte(errno);
    } else {
      addByte(IP_COMMAND_SUCCESS);
      addByte(socketFd);
    }
  }

  static void doConnectHost() {
    int socketFd = B(0);
    int ipSocketFamily = getSocketFamily(B(1)); 
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

  static void doConnectIP() {
    int socketFd = B(0);
    int ipSocketFamily = getSocketFamily(B(1)); 
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
  
  static void doSend() {
    int socketFd = B(0);
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

  static void doSendTo() {
    assert(0);
  }
  
  static void doRecv() {
    int socketFd = B(0);
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

  static void doRecvFrom() {
    assert(0);
  }
  
  static void doClose() {
    int c = close(B(0));
    if (c == -1) {
      addByte(IP_COMMAND_ERROR);
      addByte(errno);
      return;
    }
    addByte(IP_COMMAND_SUCCESS);
  }
};

static TCPIPModule theTCPIPModule(TCPIP_MODULE_ID);
