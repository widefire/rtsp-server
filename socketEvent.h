#ifndef _SOCKET_EVENT_H_
#define _SOCKET_EVENT_H_

#include "socket_type.h"
#ifdef WIN32
#include <WinSock2.h>
#include <Windows.h>
#endif

template<class T> inline void deletePointer(T*& p) { if(p!=0){delete p; p = 0; }}
template<class T> inline void deletePointerArray(T*& p) { if(p!=0){delete[] p; p = 0; }}


typedef struct socket_event
{
	socket_t clientId;
	size_t	dataLen;
	void*	dataPtr;
	bool	sock_closed;
	sockaddr_in	target_addr;
}socket_event_t,*LPSOCKET_EVENT;

typedef void(*socketEventCallback)(socket_event_t* data);

typedef struct event_manager_t
{
#ifdef WIN32
	HANDLE	completion_o_port;
	HANDLE  completion_I_port;
#else
	int epfd;
	sockaddr	clientAddr;
#endif
	socket_t	serverSocket;
	socketEventCallback	callback;
}EVENT_MANAGER,*LPEVENT_MANAGER;

//return 0 sucess;
int eventManagerInit(EVENT_MANAGER* eventManager,socket_t	serverId,socketEventCallback callback);
int eventDispatch(EVENT_MANAGER* eventManager);
int eventManagerDestory(EVENT_MANAGER* eventManager);


#endif
