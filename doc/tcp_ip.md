# trsnic
## TCP/IP Socket API

### A lightweight TCP/IP networking API

This API provides a subset of the standard TCP/IP Berkeley Sockets API with just enough functionality to create functioning TCP/IP client applications for the TRS-80. We attempt to adhere to the conventions of the Unix standard sockets library found in <sys/types.h> and <sys/socket.h>.  All API calls are sent to the Z80 I/O port 31 as a sequence of bytes where the socket function is represented by a single byte code.  Other parameters are defined as bytes per the specification below.  Responses for each function are read from the same port 31.

For a thorough course on socket programming, read the bible of TCP/IP Sockets Programming [Unix Network Programming by W. Richard Stevens](https://www.amazon.com/Unix-Network-Programming-Sockets-Networking/dp/0131411551/ref=pd_lpo_sbs_14_t_0?_encoding=UTF8&psc=1&refRID=9ZM2JR92TQCZ6E4PDQ0J)

For a quicker tutorial on using sockets, see <https://www.tutorialspoint.com/unix_sockets/socket_core_functions.htm>

Only TCP streaming sockets are supported at this time.  UDP socket support will be in an upcoming release.  TLS sockets will also be implemented at some time in the future.

### API Details

#### SOCKET (0x01)

Open a socket. 

**Unix:** int socket (int family, int type, int protocol);

Family: AF\_INET, AF\_INET6 (not currently supported)

Types: SOCK\_STREAM, SOCK\_DGRAM (not currently supported)

Protocols: Defaults to TCP for SOCK\_STREAM and UDP for SOCK\_DGRAM (not currently supported)

This call consists of 4 bytes sent in sequence to the trsnic 
port:

`<TCPIP><SOCKET><FAMILY><TYPE>`

TCPIP = 0x1

SOCKET = 0x01

AF\_INET = 0x01 or AF\_INET6 = 0x02 (*AF\INET6 not yet supported*)

SOCK\_STREAM = 0x1 or SOCK\_DGRAM = 0x02 (*SOCK\_DGRAM not yet supported*)


**Example:** To open a TCP streaming socket, you would send 4 bytes to port 31

<0x01><0x01><0x01><0x01>

**Response:** 2 bytes are returned. The first byte is status (1 for error, 0 for success). On success, the second byte is the socket file descriptor, aka sockfd.  This sockfd should be saved as it will be used by the other socket API functions below.  The second byte is set to errno only when there is an error.

**Example**
Success returns <0x00><0x02> where byte 1 = success and byte 2 = sockfd (in this case the integer 2)
Failure returns <0x01><0x01> where byte 1 = error and byte 2 = errno (in this case 1)



#### CONNECT (IP-ADDRESS) (0x02)

Connect to a server socket using an IP address.  Only used for streaming sockets, ie. TCP SOCK_STREAM

**Unix**: int connect(int sockfd, struct sockaddr *serv_addr, int addrlen);

CONNECT with IP address consists of 10 bytes (for an IPv4 address) to send to the trsnic port.  Use 1 byte for the protocol, 1 byte for the command code, 1 byte for the sockfd, 1 byte for the host format (0x00 for IP address), 4 bytes for IPv4 addresses (or 6 bytes for IPv6 addresses, not currently supported) and a 2 byte port sent in network order.

`<TCPIP><CONNECT><SOCKFD><0x00><IP4><IP3><IP2><IP1><PORT LSB><PORT MSB>`

where 

TCPIP = 0x01

CONNECT = 0x02

SOCKFD = the socket descriptor returned from SOCKET

HOST FORMAT = 0x00 (0 indicates IP address)

IP4 = 0x00 to 0xFF

IP3 = 0x00 to 0xFF

IP2 = 0x00 to 0xFF

IP1 = 0x00 to 0xFF

PORT1 = 0x00 to 0xFF  LSB of 16bit port number 

PORT2 = 0x00 to 0xFF  MSB of 16bit port number 

**Example:** To connect to port 23 on a server listening on IP address 192.168.1.100

`<0x01><0x02><SOCKFD><0xC0><0xA8><0x01><0x64><0x17><0x00>`

#### CONNECT (HOSTNAME) (0x03)

CONNECT with a hostname consists of a series of bytes to send to the trsnic port.  Use 1 byte for the protocol, 1 byte for the command code, 1 byte for the sockfd, 1 byte for the host format(0x01 for hostname), the hostname in ASCII bytes followed by a NULL (0x00) (to indicate the end of the hostname) and ends with a 2 byte port sent in network order.

`<TCPIP><CONNECT><SOCKFD><0x01><HOSTNAME><NULL><PORT LSB><PORT MSB>`

where 

TCPIP = 0x01

CONNECT = 0x03

SOCKFD = the socket descriptor returned from SOCKET

HOSTNAME = a sequence of ASCII bytes representing any valid hostname resolvable by your network DNS (e.g. www.google.com)

NULL = 0x00 (marks the end of the hostname string)

PORT1 = 0x00 to 0xFF  LSB of 16bit port number 

PORT2 = 0x00 to 0xFF  MSB of 16bit port number 

**Example:** To connect to port 23 on a server with hostname pski.net

`<0x01><0x03><SOCKFD><0x70><0x73><0x6B><0x69><0x2E><0x6E><0x65><0x74><0x00><0x17><0x00>`

**Response:** 1 or 2 bytes are returned. The first byte is 1 for error, 0 for success.  The second byte is set to errno only when there is an error.

**Example:** Success returns `<0x00>` where byte 1 = success
Failure returns `<0x01><0x01>` where byte 1 = error and byte 2 = errno



#### SEND (0x04)

Sends data over the socket.

**Unix:** int send(int sockfd, const void *msg, int len, int flags);

SEND consists of a 7 byte header and L bytes of data.

`<TCPIP><SEND><SOCKFD><L1><L2><L3><L4><DATA...>`

SOCKFD = the socket descriptor returned from SOCKET

L1-L4 = the length of the data specified as 4 bytes.  This is in little endian order where L1 is the LSB and L4 is the MSB.  The theoretical maximum length of data in one send is 2,147,483,647 bytes.

DATA = a sequence of bytes of length len

**Example:** 

`<0x01><0x04><0x01><0x05><0xFF><0x00><0x00><0xFF><0xFF>....<0xFF>` 
where byte 1 = TCPIP, byte 2 = SEND, byte 3 = SOCKFD, bytes 4 to 7 = data length of 260 and bytes 8 to (data length+8) are the data bytes.


**Response:** a 1 byte success flag (1 for error, 0 for success), followed by 4 byte response with the length of data actually sent on success or single byte errno on error.

**Example.** `<0x00><0x05><0xFF><0x00><0x00>` where byte 1 = success and bytes 2 to 5 = 32 bit data length of 260.
or
`<0x01><0x06>` where byte 1 = error and byte 2 = errno 


#### SENDTO (0x05) (*Not yet supported*)

Sends a UDP datagram over the socket.

**Unix:** int send(int sockfd, const void *msg, int len, int flags);


#### RECV (0x06)

Receive data from the socket.  Use only for connection oriented sockets ie. TCP.

**Unix:** int recv(int s, void *buf, size_t len, int flags);

`<TCPIP><RECV><SOCKFD><OPTION><L1><L2><L3><L4>`

TCPIP = 0x01

RECV = 0x06

SOCKFD = the socket descriptor returned from SOCKET

OPTION (0 - blocking, 1 - nonblocking)

L1-L4 = the maximum length of the data to read specified as 4 bytes.  The maximum length of data in one receive is 64K.

**Example:**
`<0x01><0x06><0x01><0x01><0x05><0xFF><0x00><0x00>` where byte 1 = TCPIP, 2 = RECV, 3 = OPTION, byte 4 = SOCKFD, bytes 5 to 8 = max data length to received with this request in LE order.

**Response:** a 1 byte success flag (1 for error, 0 for success), followed by 4 byte response with the 32 bit length of data followed by the data bytes.
On error only 2 bytes are returned. The first byte is 1 for error, 0 for success.  The second byte is set to errno when there is an error.

**Example:** `<0x00><L1><L2><L3><L4><DATA...>`
or
`<0x01><0x6>` where byte 1 = error and byte 2 = errno (in this case 6)


#### RECVFROM (0x07) (*Not yet supported*)

Receive a data gram from the socket.  Use only for connection-less sockets (ie. UDP);

**Unix:** int recvfrom(int s, void *&ast;buf*, size\_t *len*, int *flags*, struct sockaddr *&ast;from*, socklen\_t *&ast;fromlen*); 

`<RECVFROM><SOCKFD><IP4><IP3><IP2><IP1><PORT LSB><PORT MSB>`

Returns a 1 byte success flag (1 for error, 0 for success), followed by 4 byte response with the length of data
On error only 2 bytes are returned. The first byte is 1 for error, 0 for success.  The second byte is set to errno when there is an error.

<0x00><L1><L2><L3><L4><DATA...>
or
<0x01><errno>


#### CLOSE (0x08)

Close a socket

**Unix:** int close(int sockfd);

Send 3 bytes to the trsnic port.

`<TCPIP><CLOSE><SOCKFD>`

TCPIP = 0x01

CLOSE = 0x08

SOCKFD = the socket descriptor returned from SOCKET

**Response:** 1 or 2 bytes are returned. The first byte is 1 for error, 0 for success.  The second byte is set to errno only when there is an error.

**Example:**
`<0x00>`
or
`<0x01><0x01>`

-----
Have questions? [@pski](https://github.com)


