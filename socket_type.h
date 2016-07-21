#ifndef _SOCKET_TYPE_H_
#define _SOCKET_TYPE_H_

#if defined(__WIN32__) || defined(_WIN32) || defined(_WIN32_WCE)
/* Windows */
#if defined(WINNT) || defined(_WINNT) || defined(__BORLANDC__) || defined(__MINGW32__) || defined(_WIN32_WCE) || defined (_MSC_VER)
#define _MSWSOCK_
#include <winsock2.h>
#include <ws2tcpip.h>
#endif
#include <windows.h>
#include <errno.h>
#include <string.h>

#define closeSocket closesocket
#ifdef EWOULDBLOCK
#undef EWOULDBLOCK
#endif
#ifdef EINPROGRESS
#undef EINPROGRESS
#endif
#ifdef EAGAIN
#undef EAGAIN
#endif
#ifdef EINTR
#undef EINTR
#endif
#define EWOULDBLOCK WSAEWOULDBLOCK
#define EINPROGRESS WSAEWOULDBLOCK
#define EAGAIN WSAEWOULDBLOCK
#define EINTR WSAEINTR

#if defined(_WIN32_WCE)
#define NO_STRSTREAM 1
#endif

#include <WinSock2.h>
#include <Windows.h>
typedef SOCKET socket_t;
typedef int socklen_t;
//#define close closesocket
/* Definitions of size-specific types: */
typedef __int64 int64_t;
typedef unsigned __int64 u_int64_t;

typedef int int32_t;
typedef unsigned u_int32_t;

typedef short int16_t;
typedef unsigned short u_int16_t;

typedef unsigned char u_int8_t;

#if !defined(_MSC_STDINT_H_) && !defined(_UINTPTR_T_DEFINED) && !defined(_UINTPTR_T_DECLARED) && !defined(_UINTPTR_T)
typedef unsigned uintptr_t;
#endif
#if !defined(_MSC_STDINT_H_) && !defined(_INTPTR_T_DEFINED) && !defined(_INTPTR_T_DECLARED) && !defined(_INTPTR_T)
typedef int intptr_t;
#endif

#elif defined(VXWORKS)
/* VxWorks */
#include <time.h>
#include <timers.h>
#include <sys/times.h>
#include <sockLib.h>
#include <hostLib.h>
#include <resolvLib.h>
#include <ioLib.h>

typedef unsigned int u_int32_t;
typedef unsigned short u_int16_t;
typedef unsigned char u_int8_t;

#else
/* Unix */
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <strings.h>
#include <ctype.h>
#include <stdint.h>
#if defined(_QNX4)
#include <sys/select.h>
#include <unix.h>
#endif

#define closeSocket close
#define stricmp strcasecmp
#define strnicmp strncasecmp

typedef int                  socket_t;
typedef socklen_t            socklen_t;

#ifdef SOLARIS
#define u_int64_t uint64_t
#define u_int32_t uint32_t
#define u_int16_t uint16_t
#define u_int8_t uint8_t
#endif
#endif

#ifndef SOCKLEN_T
#define SOCKLEN_T int
#endif

#ifdef WIN32
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include<unistd.h>

#endif

#define SNDBUF_SIZE (51200)

#ifndef BYTE_ORDER

#define LITTLE_ENDIAN	1234
#define BIG_ENDIAN	4321

#if defined(sun) || defined(__BIG_ENDIAN) || defined(NET_ENDIAN)
#define BYTE_ORDER	BIG_ENDIAN
#else
#define BYTE_ORDER	LITTLE_ENDIAN
#endif

#endif

//return 0 success.else failed;
int socketInit();
//return 0 success.else failed;
int socketDestory();
//return 0 no Err.
int socketError();
//return 0 success.else failed;
int socketSetNoBlocking(socket_t socketId);
//get ip address;
const char *get_server_ip();
//////////////////////////////////////////////////////////////////////////
//return less than 0 failed;
//create , set noblock ,connect;block now
socket_t socketCreateTcpClient(const char*	hostname,int port);
socket_t socketCreateUdpClient();
socket_t socketCreateServer(const char*	hostname,int port);
socket_t socketCreateUdpServer(int port);
//return 0 success.else failed;
int socketServerListen(socket_t socketId);

int socketSend(socket_t s,const char*	buf, int len);
int socketRecv(socket_t s,const char*	buf,  int len);
void setSocketTimeout(socket_t s, timeval time);

int socketSendto(socket_t s,
	const char *buf,
	int len,
	int flags,
	const struct sockaddr  *to,
	int tolen);
int socketRecvfrom(socket_t s,
	char  *buf,
	int len,
	int flags,
	struct sockaddr  *from,
	int fromlen);
int get_source_port(socket_t sock_id, unsigned short &port);
#endif
