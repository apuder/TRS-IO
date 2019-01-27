#include <stdio.h>
#include <string.h>    //strlen

#include "tcpip.h"

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

static int errorNumber;
static int socketFd;
static int version;
static int recvOption;

static int ipCommand = 0;
static int ipSocketFamily;
static int ipSocketType;
static int hostFormat;

static const char *TAG="tcp_client";

static unsigned char buffer[65536];
static char ipAddress[16];
static int port;
static int ips[4];
static char hostname[255];

static unsigned int byteCounter;
static unsigned int dataLength;
static int state = IP_STATE_READY;
static unsigned int length;
static int paramCounter;
static unsigned int left;

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

bool doSocket();
bool doConnect();
bool doSend();
bool doRecv();
bool doClose();

void tcp_get_send_buffer(uint8_t** buf, int* size)
{
  if (state != IP_STATE_SEND_TO_Z80) {
    *size = 0;
  } else {
    *buf = buffer;
    *size = dataLength;
  }
  state = IP_STATE_READY;
}

void readCommand(uint8_t value) {
  
  //ESP_LOGI(TAG,"IP_STATE_READ_COMMAND with count %d and value %d \n", byteCounter, value);
  
  ipCommand = value;
  switch(ipCommand) {

    case IP_COMMAND_SOCKET:

      state = IP_STATE_READ_SOCKET_FAMILY;
      break;

    case IP_COMMAND_CONNECT:
    case IP_COMMAND_SEND:
    case IP_COMMAND_SENDTO:
    case IP_COMMAND_RECV:
    case IP_COMMAND_RECVFROM:
    case IP_COMMAND_CLOSE:
    
      state = IP_STATE_READ_SOCKFD;
      break;
    
    default:
    
      state = IP_STATE_SEND_TO_Z80;
      dataLength = 2;
      buffer[0] = IP_COMMAND_ERROR;
      buffer[1] = IP_ERROR_UNKNOWN_COMMAND;
      break;
  }
}

void readSocketFamily(uint8_t value) {

  //ESP_LOGI(TAG,"IP_STATE_READ_SOCKET_FAMILY with count %d and value %d \n", byteCounter, value);

  ipSocketFamily = value;
  state = IP_STATE_READ_SOCKET_TYPE;

}

void readSocketType(uint8_t value) {

  //ESP_LOGI(TAG,"IP_STATE_READ_SOCKET_TYPE with count %d and value %d \n", byteCounter, value);

  ipSocketType = value;
  state = IP_STATE_READ_VERSION;
    
}

void readVersion(uint8_t value) {
  
  //ESP_LOGI(TAG,"IP_STATE_READ_VERSION with count %d and value %d \n", byteCounter, value);
  
  version = value;
  state = IP_STATE_SEND_TO_Z80;
  dataLength = 2;

  if ( doSocket() ) {
    buffer[0] = IP_COMMAND_SUCCESS;
    buffer[1] = socketFd;
  } else {
    buffer[0] = IP_COMMAND_ERROR;
    buffer[1] = errorNumber;
  }
}

void readIpAddr(uint8_t value) {

  //ESP_LOGI(TAG,"IP_STATE_READ_IP_ADDR with count %d and value %d \n", byteCounter, value);

  switch(ipSocketFamily) {

    case IP_SOCKET_FAMILY_AF_INET:
      ips[paramCounter-1] = value;
      if ( paramCounter == 4 ) {
        sprintf(ipAddress, "%d.%d.%d.%d", ips[0], ips[1], ips[2], ips[3]);
        state = IP_STATE_READ_PORT;
        paramCounter = 1;
        break;
      }
      paramCounter++;
      break;

    case IP_SOCKET_FAMILY_AF_INET6:
      ESP_LOGE(TAG, "IPv6 Not Supported!\n");
      state = IP_STATE_SEND_TO_Z80;
      dataLength = 2;
      buffer[0] = IP_COMMAND_ERROR;
      buffer[1] = IP_ERROR_UNSUPPORTED_PROTOCOL;
      break;

  }
}

void readHostname(uint8_t value) {
  
  //ESP_LOGI(TAG,"IP_STATE_READ_HOSTNAME with count %d and value %d \n", byteCounter, value);

  switch(ipSocketFamily) {
    case IP_SOCKET_FAMILY_AF_INET:
      hostname[paramCounter-1] = value;
      if ( value == 0 ) {
        state = IP_STATE_READ_PORT;
        paramCounter = 1;
        break;
      }
      paramCounter++;
      break;

    case IP_SOCKET_FAMILY_AF_INET6:
      ESP_LOGE(TAG, "IPv6 Not Supported!\n");
      state = IP_STATE_SEND_TO_Z80;
      dataLength = 2;
      buffer[0] = IP_COMMAND_ERROR;
      buffer[1] = IP_ERROR_UNSUPPORTED_PROTOCOL;
      break;
  }
}

void readPort(uint8_t value) {
  
  //ESP_LOGI(TAG,"IP_STATE_READ_PORT with count %d and value %d \n", byteCounter, value);

  switch(paramCounter) {
    case 1:
      port = value * 256;
      paramCounter++;
      break;
    case 2:
      port += value;
      ESP_LOGI(TAG,"port is %d \n", port);
      if ( ipCommand == IP_COMMAND_CONNECT ) {
        state = IP_STATE_SEND_TO_Z80;
        if ( doConnect() ) {
          buffer[0] = IP_COMMAND_SUCCESS;
          dataLength = 1;
        } else {
          buffer[0] = IP_COMMAND_ERROR;
          buffer[1] = errorNumber;
          dataLength = 2;
        }
      } else {
        ESP_LOGE(TAG, "ONLY CONNECT SUPPORTED AFTER PORT AT THIS TIME.\n");
        state = IP_STATE_SEND_TO_Z80;
        buffer[0] = IP_COMMAND_ERROR;
        buffer[1] = IP_ERROR_UNSUPPORTED_COMMAND_OPTION;
        dataLength = 2; 
      }

      break;
  }
}

void readSockFd(uint8_t value) {
  
  //ESP_LOGI(TAG,"IP_STATE_READ_SOCKFD with count %d and value %d \n", byteCounter, value);
  
  socketFd = value;
        
  switch(ipCommand) {
    case IP_COMMAND_CONNECT:
      paramCounter = 1;
      state = IP_STATE_READ_HOST_FORMAT;
      break;
    case IP_COMMAND_SEND:
      state = IP_STATE_READ_LENGTH;
      paramCounter = 1;
      break;
    case IP_COMMAND_RECV:
      state = IP_STATE_READ_RECV_OPTION;
      break;
    case IP_COMMAND_CLOSE:
      state = IP_STATE_SEND_TO_Z80;
      if ( doClose() ) {
        buffer[0] = IP_COMMAND_SUCCESS;
        dataLength = 1;
      } else {
        buffer[0] = IP_COMMAND_ERROR;
        buffer[1] = errorNumber;
        dataLength = 2;
      }
    break;
    default:
      ESP_LOGE(TAG, "ONLY CONNECT,SEND,RECV AND CLOSE SUPPORTED AFTER SOCKFD AT THIS TIME.\n");
      state = IP_STATE_SEND_TO_Z80;
      buffer[0] = IP_COMMAND_ERROR;
      buffer[1] = IP_ERROR_UNSUPPORTED_COMMAND_OPTION;
      dataLength = 2; 
      break;
  }
}

void readHostFormat(uint8_t value) {
  
  //ESP_LOGI(TAG,"IP_STATE_READ_HOST_FORMAT with count %d and value %d \n", byteCounter, value);
    
  hostFormat = value;
  if ( hostFormat == IP_HOST_FORMAT_HOSTNAME )
    state = IP_STATE_READ_HOSTNAME;
  else
    state = IP_STATE_READ_IP_ADDR;
  paramCounter = 1;

}

void readRecvOption(uint8_t value) {
  
  //ESP_LOGI(TAG,"IP_STATE_READ_RECV_OPTION with count %d and value %d \n", byteCounter, value);
  
  recvOption = value;
  state = IP_STATE_READ_LENGTH;
  paramCounter = 1;

}

void readLength(uint8_t value) {
  
  //ESP_LOGI(TAG,"IP_STATE_READ_LENGTH with count %d and value %d \n", byteCounter, value); 
  
  switch(paramCounter) {
    case 1:
      length = value * 256 * 256 * 256;
      break;
    case 2:
      length += value * 256 * 256;
      break;
    case 3:
      length += value * 256;
      break;
    case 4:
      length += value;
      //ESP_LOGI(TAG,"packet len is %d \n", length);
      if ( length > sizeof(buffer) - 5) {
        length = sizeof(buffer) - 5;
        ESP_LOGE(TAG, "buffer length was too large.  reduced it to %d\n", length);
      }
    
    switch(ipCommand) {
      case IP_COMMAND_SEND:
        dataLength = length;
        left = length;
        state = IP_STATE_READ_DATA;
        break;
      case IP_COMMAND_RECV:
        state = IP_STATE_SEND_TO_Z80;
        if ( doRecv() ) {
          //do nothing here as doRecv handles the response buffer
        } else {
          buffer[0] = IP_COMMAND_ERROR;
          buffer[1] = errorNumber;
          dataLength = 2;
        }
        break;
      default:
        ESP_LOGE(TAG, "ONLY SEND AND RECV SUPPORTED AFTER PACKET LENGTH AT THIS TIME.\n");
        state = IP_STATE_SEND_TO_Z80;
        buffer[0] = IP_COMMAND_ERROR;
        buffer[1] = IP_ERROR_UNSUPPORTED_COMMAND_OPTION;
        dataLength = 2; 
        break;
    }
    break;
  }
  paramCounter++;
}

void readData(uint8_t value) {
  
  //ESP_LOGI(TAG,"IP_STATE_READ_DATA with count %d and value %d \n", byteCounter, value);
  
  if ( left > 0 ) {
    buffer[dataLength-left] = value;
    left--;
  }  
  if ( left <= 0 ) {
    left = dataLength;
    state = IP_STATE_SEND_TO_Z80;
    if ( doSend() ) {
      buffer[0] = IP_COMMAND_SUCCESS;
      //Add the 4 bytes of data length info
      buffer[1] = (dataLength >> 24) & 0xFF;
      buffer[2] = (dataLength >> 16) & 0xFF;
      buffer[3] = (dataLength >> 8) & 0xFF;
      buffer[4] = dataLength & 0xFF;
      dataLength = 5;
    } else {
      buffer[0] = IP_COMMAND_ERROR;
      buffer[1] = errorNumber;
      dataLength = 2;
    }
  }
}

int tcp_z80_out(uint8_t value) {
  
  byteCounter++;
  
  switch(state) {
  
  case IP_STATE_SEND_TO_Z80: // Discard previous send buffer
  case IP_STATE_READY:
    byteCounter = 0;
  state = IP_STATE_READ_COMMAND;
  
  case IP_STATE_READ_COMMAND:
    readCommand(value);
    break;

  case IP_STATE_READ_SOCKET_FAMILY:
    readSocketFamily(value);
    break;

  case IP_STATE_READ_SOCKET_TYPE:
    readSocketType(value);
    break;
    
  case IP_STATE_READ_VERSION:
    readVersion(value);
    break;  

  case IP_STATE_READ_IP_ADDR:
    readIpAddr(value);
    break;

  case IP_STATE_READ_HOSTNAME:
    readHostname(value);
    break;

  case IP_STATE_READ_PORT:
    readPort(value);
    break;
    
  case IP_STATE_READ_SOCKFD:
    readSockFd(value);
    break;

  case IP_STATE_READ_HOST_FORMAT:
    readHostFormat(value);
    break;

  case IP_STATE_READ_RECV_OPTION:
    readRecvOption(value);
    break;
    
  case IP_STATE_READ_LENGTH:
    readLength(value);
    break;
      
  case IP_STATE_READ_DATA:
    readData(value);
    break;
  
  default:
    break;
  
  }
  return state;
}

int getTrueSocketFamily() {
  switch(ipSocketFamily) {
    case IP_SOCKET_FAMILY_AF_INET:
      return AF_INET;
    case IP_SOCKET_FAMILY_AF_INET6:
      return AF_INET6;
  }
  return -1;
}

int getTrueSocketType() {
  switch(ipSocketType) {
    case IP_SOCKET_TYPE_SOCK_STREAM:
      return SOCK_STREAM;
    case IP_SOCKET_TYPE_SOCK_DGRAM:
      return SOCK_DGRAM;
  }
  return -1;
}

bool doSocket() {
  
  //ESP_LOGI(TAG, "Opening a socket.\n");
  
  socketFd = socket(getTrueSocketFamily(), getTrueSocketType(), 0);
  if (socketFd < 0) {
      // Error opening socket
    ESP_LOGE(TAG, "... Failed to allocate socket, errno=%d.\n",errno);
    errorNumber = errno;
    return false;
  } 
    
  //ESP_LOGI(TAG, "Opened socket fd %d.\n", socketFd);
  
  return true;

}

bool doConnect() {

  if ( hostFormat == IP_HOST_FORMAT_HOSTNAME ) {
  
    struct sockaddr_in serv_addr;
    struct hostent* server;
  
    ESP_LOGI(TAG, "Connecting to %s port %d on socket %d\n", hostname, port, socketFd);
    
    server = gethostbyname(hostname);
  
    if (server == NULL) {
      ESP_LOGE(TAG, "No such host");
      errorNumber = IP_ERROR_UNKNOWN_HOST;
      return false;
    }
    
    bzero((char*) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = getTrueSocketFamily();
    bcopy((char*) server->h_addr,
          (char*) &serv_addr.sin_addr.s_addr,
          server->h_length);
    serv_addr.sin_port = htons(port);
    if (connect(socketFd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
      ESP_LOGE(TAG, "... socket connect failed errno=%d \n", errno);
      errorNumber = errno;
      return false;
    }
    
    return true;


  } else {
    
    ESP_LOGI(TAG, "Connecting to %s port %d on socket %d\n", ipAddress, port, socketFd);
    
    struct sockaddr_in tcpServerAddr;
      tcpServerAddr.sin_addr.s_addr = inet_addr(ipAddress);
      tcpServerAddr.sin_family = getTrueSocketFamily();
      tcpServerAddr.sin_port = htons(port);
      
     if (connect(socketFd, (struct sockaddr *) &tcpServerAddr, sizeof(tcpServerAddr)) < 0) {
        ESP_LOGE(TAG, "... socket connect failed errno=%d \n", errno);
        errorNumber = errno;
        return false;
     }
  
    return true;
    
  }
  
}

bool doSend() {

  ESP_LOGI(TAG, "TCP send started...\n");
  
  dataLength = 0;
  while( left > 0 ) {
    int wrote = write(socketFd , &buffer[dataLength] , left);
    if ( wrote < 0 ) {
      ESP_LOGE(TAG, "TCP write failed! \n");
      errorNumber = errno;
      return false;
    }
    left-=wrote;
    dataLength+=wrote;
  }
  
  left = dataLength;
    
  ESP_LOGI(TAG, "... socket write success.");
  return true;

}

bool doRecv() {
  
  //ESP_LOGI(TAG, "Reading from socket\n");
  
  left = 0;
  
  int option = 0;
  switch( recvOption ) {
    case IP_RECV_BLOCKING:
      option = MSG_WAITALL;
    break;
    case IP_RECV_NONBLOCKING:
      option = MSG_DONTWAIT;
    break;
    default:
      ESP_LOGE(TAG, "Unknown recv option failed! \n");
      return false;
      break;
  }
          
  left = recv(socketFd, &buffer[5], length, option);
  if (left == -1 && errno != EAGAIN && errno != EWOULDBLOCK) {
    ESP_LOGE(TAG, "recv failed with %d! \n", errno);
      errorNumber = errno;
      return false;
  }  
    
  buffer[0] = IP_COMMAND_SUCCESS;
  
  if ( left == -1 ) {
    buffer[1] = 0;
    buffer[2] = 0;
    buffer[3] = 0;
    buffer[4] = 0;    
    dataLength = 5;
  } else {       
      //Add the 4 bytes of data length info
    buffer[1] = (left >> 24) & 0xFF;
    buffer[2] = (left >> 16) & 0xFF;
    buffer[3] = (left >> 8) & 0xFF;
    buffer[4] = left & 0xFF;
    //Add 4 for length prefix and 1 for success code
    dataLength = 5 + left; 
  }
    
    //Delay a bit because otherwise we're too fast for the TRS-80 when no data is returned
    //This can use some tweaking
    vTaskDelay(500 / portTICK_PERIOD_MS);
              
    //ESP_LOGI(TAG, "... done reading from socket.");
          
  return true;
  
}

bool doClose() {
  
  //ESP_LOGI(TAG, "doClose()\n");

  int c = close(socketFd);
  if ( c == -1 ) {
    ESP_LOGE(TAG, "... socket close failed errno=%d \n", errno);
    errorNumber = errno;
    return false;
  }
  
  //ESP_LOGI(TAG, "Socket closed.\n");

  return true;
}
