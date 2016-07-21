#ifdef WIN32
#include "socketEvent.h"
#include <WS2tcpip.h>
#include <WinSock2.h>
#include <Windows.h>
#include <stdio.h>

const int PER_IO_OPT_DATA_BUF_LEN=2048;

enum IO_TYPE
{
	IO_Read,
	IO_Write,
};


typedef struct _PER_IO_OPERATION_DATA
{
	OVERLAPPED overlapped;
	WSABUF	dataBuf;
	char	buf[PER_IO_OPT_DATA_BUF_LEN];
	int		bufLen;
	IO_TYPE		optType;
	~_PER_IO_OPERATION_DATA()
	{
	}
}PER_IO_OPERATION_DATA,*LPPER_IO_OPERATION_DATA;

typedef struct _PER_HANDLE_DATA
{
	_PER_HANDLE_DATA()
	{
		per_io_data = new	PER_IO_OPERATION_DATA;
	}
	~_PER_HANDLE_DATA()
	{
		delete per_io_data;
	}
	socket_t	socketId;
	sockaddr_in	clientAddr;
	EVENT_MANAGER	*eventManager;
	LPPER_IO_OPERATION_DATA	per_io_data;
}PER_HANDLE_DATA,*LPPER_HANDLE_DATA;

static DWORD	WINAPI	ServerWorkThread(LPVOID	lpParam);


int eventManagerInit(EVENT_MANAGER* eventManager,socket_t serverId,socketEventCallback callback)
{
	HANDLE	completionPort=CreateIoCompletionPort(INVALID_HANDLE_VALUE,NULL,0,0);
	if (0==completionPort)
	{
		return	-1;
	}
	eventManager->completion_o_port=completionPort;

	eventManager->serverSocket=serverId;
	eventManager->callback=callback;

	return 0;
}

int	eventManagerDestory(EVENT_MANAGER* eventManager)
{
	CloseHandle(eventManager->completion_o_port);
	closeSocket(eventManager->serverSocket);
	return	0;
}

int eventDispatch(EVENT_MANAGER* eventManager)
{

	printf("begin dispatch:\n");
	SYSTEM_INFO	sysInfo;
	GetSystemInfo(&sysInfo);
	for (DWORD	i=0;i<sysInfo.dwNumberOfProcessors*2;i++)
	{
		HANDLE	hThread=CreateThread(0,0,(LPTHREAD_START_ROUTINE)ServerWorkThread,eventManager->completion_o_port,0,0);
		if (0==hThread)
		{
			printf("create iocp work thread failed;\n");
			return	-1;
		}
		CloseHandle(hThread);
	}

	while (1)
	{
		LPPER_HANDLE_DATA	perHandleData=0;
		SOCKADDR_IN	sockAddrRemote;
		int	remoteLen;
		socket_t	acceptSocket=-1;
		remoteLen=sizeof sockAddrRemote;
		acceptSocket=accept(eventManager->serverSocket,(sockaddr*)&sockAddrRemote,&remoteLen);
		if (acceptSocket==-1)
		{
			Sleep(10);
			continue;
		}
		if (acceptSocket<=0)
		{
			printf("accept socket error:%d\n",socketError());
			return -1;
		}
		perHandleData=new PER_HANDLE_DATA;
		perHandleData->eventManager=eventManager;
		perHandleData->socketId=acceptSocket;
		memcpy(&perHandleData->clientAddr,&sockAddrRemote,remoteLen);

		CreateIoCompletionPort((HANDLE)(perHandleData->socketId),eventManager->completion_o_port,(ULONG_PTR)perHandleData,0);

		//LPPER_IO_OPERATION_DATA	perIoData=new PER_IO_OPERATION_DATA;
		LPPER_IO_OPERATION_DATA	perIoData=perHandleData->per_io_data;
		memset(&(perIoData->overlapped),0,sizeof OVERLAPPED);
		perIoData->dataBuf.buf=perIoData->buf;
		perIoData->dataBuf.len=PER_IO_OPT_DATA_BUF_LEN;
		perIoData->optType=IO_Read;
		
		socket_event_t	data;
		data.clientId = perHandleData->socketId;
		data.dataLen = 0;
		data.dataPtr = nullptr;
		data.dataLen = 0;
		data.sock_closed = false;
		if (perHandleData->eventManager->callback != 0)
		{
			perHandleData->eventManager->callback(&data);
		}

		DWORD	recvBytes;
		DWORD	flags=0;
		WSARecv(perHandleData->socketId,&(perIoData->dataBuf),1,&recvBytes,&flags,&(perIoData->overlapped),0);
	}
}

static	DWORD	WINAPI ServerWorkThread(LPVOID lpParam)
{
	HANDLE				completionPort=static_cast<HANDLE>(lpParam);
	DWORD				bytesTransferred;
	LPOVERLAPPED		lpOverlapped=0;
	LPPER_HANDLE_DATA	perHandleData=0;
	LPPER_IO_OPERATION_DATA	perIoData=0;
	DWORD				recvBytes=0;
	DWORD				flags=0;
	int					iRet=0;
	while (true)
	{
		iRet=GetQueuedCompletionStatus(completionPort,&bytesTransferred,(PULONG_PTR)&perHandleData,
			(LPOVERLAPPED*)&lpOverlapped,INFINITE);
		if (iRet==0)
		{
			printf("GetQueuedCompletionStatus failed,error:%d\n",socketError());
			closeSocket(perHandleData->socketId);
			socket_event data;
			data.sock_closed=true;
			data.clientId=perHandleData->socketId;
			if (perHandleData->eventManager->callback!=0)
			{
				perHandleData->eventManager->callback(&data);
			}
			deletePointer(perHandleData);
			//deletePointer(perIoData);
			continue;

		}
		perIoData=(LPPER_IO_OPERATION_DATA)CONTAINING_RECORD(lpOverlapped,PER_IO_OPERATION_DATA,overlapped);
		if (0==bytesTransferred)
		{
			closeSocket(perHandleData->socketId);
			socket_event data;
			data.sock_closed=true;
			data.clientId=perHandleData->socketId;
			if (perHandleData->eventManager->callback!=0)
			{
				perHandleData->eventManager->callback(&data);
			}
			deletePointer(perHandleData);
			//deletePointer(perIoData);
			continue;
		}


		socket_event_t	data;
		data.clientId=perHandleData->socketId;
		data.dataLen=perIoData->dataBuf.len;
		data.dataPtr=perIoData->dataBuf.buf;
		data.dataLen=bytesTransferred;
		data.sock_closed=false;
		memcpy(&data.target_addr, &perHandleData->clientAddr, sizeof data.target_addr);
		if (perHandleData->eventManager->callback!=0)
		{
			perHandleData->eventManager->callback(&data);
		}

		//read again;
		memset(&(perIoData->overlapped),0,sizeof OVERLAPPED);
		perIoData->dataBuf.buf=perIoData->buf;
		perIoData->dataBuf.len=PER_IO_OPT_DATA_BUF_LEN;
		perIoData->optType=IO_Read;
		WSARecv(perHandleData->socketId,&(perIoData->dataBuf),1,&recvBytes,&flags,&(perIoData->overlapped),0);
	}
}

#endif