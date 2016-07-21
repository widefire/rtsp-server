#include <stdio.h>
#include "socket_type.h"
#include <thread>

int socketInit()
{
#ifdef WIN32
	WORD wVersion;
	WSADATA wsaData;
	wVersion=MAKEWORD(2,2);
	return WSAStartup(wVersion,&wsaData);
#else
	return 0;
#endif
}

int socketDestory()
{
#ifdef WIN32
	return WSACleanup();
#else
	return errno;
#endif
}

int socketError()
{
#ifdef WIN32
	return WSAGetLastError();
#else
	return errno;
#endif
}

int socketSetNoBlocking(socket_t socketId)
{
	int iRet=0;
#ifdef WIN32
	int opt=1;
	iRet=setsockopt(socketId,SOL_SOCKET,SO_REUSEADDR,(const char*)&opt,sizeof opt);
	if (iRet!=0)
	{
		printf("set socket %d reuseaddr failed;\n",(int)socketId);
		return iRet;
	}
	u_long argp=1;
	iRet=ioctlsocket(socketId,FIONBIO,&argp);
	if (iRet!=0)
	{
		printf("set socket %d noblock failed;\n",(int)socketId);
		return	iRet;
	}
#else
	int opts;
	int opt = 1;
	iRet = setsockopt(socketId, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof opt);

	opts = fcntl(socketId, F_GETFL,0);
	if (opts<0)
	{
		printf("set socket %d F_GETFL failed;\n",socketId);
		return opts;
	}

	opts |= O_NONBLOCK;
	opts = fcntl(socketId, F_SETFL,opts);
	if (opts<0)
	{
		printf("set socket %d F_SETFL failed;\n",socketId);
		return opts;
	}
#endif
	return iRet;
}

socket_t socketCreateTcpClient(const char* hostname,int port)
{
	socket_t clientSocket;
	do 
	{
		sockaddr_in svrAddr;
		memset(&svrAddr,0,sizeof svrAddr);
		svrAddr.sin_family=AF_INET;
		svrAddr.sin_addr.s_addr=htons(INADDR_ANY);
		svrAddr.sin_port=htons(0);

		clientSocket=socket(AF_INET,SOCK_STREAM,0);
		int socketErr;
		if (clientSocket<0)
		{
			socketErr=socketError();
			printf("Create socket failed ,errcode :%d\n",socketErr);
			break;
		}
		int iRet=0;
		
		iRet=bind(clientSocket,(const sockaddr*)&svrAddr,sizeof svrAddr);
		if (iRet!=0)
		{
			break;
		}
		
		/*iRet=socketSetNoBlocking(clientSocket);
		if (iRet!=0)
		{
			printf("Set socket %d no blocking failed;");
			break;
		}*/
		
		//set recv buf size
		int bufSize;
#ifdef WIN32
		int sizeSize=sizeof bufSize;
#else
		unsigned int sizeSize=sizeof bufSize;
#endif
		iRet=getsockopt(clientSocket,SOL_SOCKET,SO_RCVBUF,(char*)&bufSize,&sizeSize);
		if (iRet<0)
		{
			printf("get buf size failed;\n");
			break;
		}
		int reqSize=SNDBUF_SIZE;
		while (bufSize<SNDBUF_SIZE)
		{
			if (setsockopt(clientSocket,SOL_SOCKET,SO_RCVBUF,(const char*)&reqSize,sizeSize)>=0)
			{
				printf("set send buf size:%d  success;\n",reqSize);
				break;
			}
			reqSize=((reqSize+bufSize)>>1);
		}
		
		svrAddr.sin_port=htons(port);
		svrAddr.sin_addr.s_addr=inet_addr(hostname);
		iRet=connect(clientSocket,(const sockaddr*)&svrAddr,sizeof svrAddr);
		if (iRet!=0)
		{
			socketErr=socketError();
			if (socketErr==EWOULDBLOCK||socketErr==EINPROGRESS)
			{
				break;
			}
			printf("socket %d connect failed:%d ;\n",clientSocket,socketErr);
		}else
		{
			printf("connect success;\n");
		}
	} while (0);

	return clientSocket;
}

socket_t socketCreateUdpClient()
{
	return socket(AF_INET, SOCK_DGRAM, 0);
}

socket_t socketCreateServer(const char* hostname,int port)
{
	socket_t svrSocket;
	do 
	{
		sockaddr_in svrAddr;
		memset(&svrAddr,0,sizeof svrAddr);
		svrAddr.sin_family=AF_INET;
		svrAddr.sin_addr.s_addr=htons(INADDR_ANY);
		svrAddr.sin_port=htons(port);

		svrSocket=socket(AF_INET,SOCK_STREAM,0);
		int socketErr;
		if (svrSocket<0)
		{
			socketErr=socketError();
			printf("Create socket failed ,errcode :%d\n",socketErr);
			break;
		}
		int iRet;
		iRet=socketSetNoBlocking(svrSocket);
		if (iRet!=0)
		{
			printf("Set socket %d no blocking failed;");
			break;
		}

		iRet=bind(svrSocket,(const sockaddr*)&svrAddr,sizeof svrAddr);
		if (iRet!=0)
		{
			printf("bind svr socket failed;%d\n",socketError());

			break;
		}
		//set send buf size
		int bufSize;
#ifdef WIN32
		int sizeSize=sizeof bufSize;
#else
		unsigned int sizeSize=sizeof bufSize;
#endif
		iRet=getsockopt(svrSocket,SOL_SOCKET,SO_SNDBUF,(char*)&bufSize,&sizeSize);
		if (iRet<0)
		{
			printf("get buf size failed;\n");
			break;
		}
		int reqSize=SNDBUF_SIZE;
		while (bufSize<SNDBUF_SIZE)
		{
			if (setsockopt(svrSocket,SOL_SOCKET,SO_SNDBUF,(const char*)&reqSize,sizeSize)>=0)
			{
				printf("set send buf size:%d  success;\n",reqSize);
				break;
			}
			reqSize=((reqSize+bufSize)>>1);
		}
	} while (0);
	if (svrSocket==(~0))
	{
		svrSocket = 0;
	}
	return svrSocket;
}

int socketServerListen(socket_t socketId)
{
	int iRet=0;
	iRet=listen(socketId,SOMAXCONN);
	if (iRet!=0)
	{
		int err=socketError();
		printf("socket %d listen failed,errcode :%d ;\n",socketId,err);
		return iRet;
	}
	return iRet;
}

int socketSend(socket_t s, const char * buf, int len)
{
	if (len == 0)
	{
		return 0;
	}
	int iRet;
#ifdef WIN32
	//return send(s,buf,len,0);
	WSABUF wsa_buf;
	wsa_buf.buf = const_cast<char*>(buf);
	wsa_buf.len = len;
	DWORD num_bytes_send = 0;
	iRet = WSASend(s, &wsa_buf, 1, &num_bytes_send, 0, 0, 0);
	if (iRet == 0)
	{
		return len;
	}
	else if (iRet = SOCKET_ERROR)
	{
		iRet = socketError();
		if (iRet == 111 || iRet == 0 ||
			iRet == EWOULDBLOCK || iRet == 113)
		{
			return	len;
		}
		else
		{
			return iRet;
		}
	}
#else
	int curPos = 0;
	int againTimes = 0;
	while (curPos<len)
	{
		iRet = write(s, buf + curPos, len - curPos);
		if (iRet==len-curPos)
		{
			return len;
		}
		if (iRet<=0)
		{
			int error = socketError();
			if (error==EINTR||error==EAGAIN)
			{
				againTimes++;
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
				if (againTimes>2000)
				{
					return iRet;
				}
				continue;
			}
			return iRet;
		}
		else
		{
			curPos += iRet;
		}
	}
	return len;
#endif
}

int socketRecv(socket_t s,const char*	buf,  int len)
{

#ifdef WIN32
	return recv(s,const_cast<char*>(buf),len,0);
#else
	//return read(s,const_cast<char*>(buf),len);
	int err = 4;
	int ret = 0;
	while (err == 4)
	{
		ret = recv(s, const_cast<char*>(buf), len, 0);
		if (ret <= 0)
		{
			err = socketError();
			if (err== EAGAIN)
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
			}
			else if (err== EINTR)
			{
				err = 4;
			}
		}
		else
		{
			break;
		}
	}
	return ret;
#endif
}

void setSocketTimeout(socket_t s, timeval time)
{
#ifdef WIN32
	int out = time.tv_sec * 1000 + time.tv_usec / 1000;
	setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (const char *)(&out), sizeof(out));
#else
	setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (const char*)&time, sizeof(time));
#endif // WIN32
}

int socketSendto(socket_t s,
	const char *buf,
	int len,
	int flags,
	const struct sockaddr  *to,
	int tolen)
{
	return sendto(s, buf, len, flags, to, tolen);
}

int socketRecvfrom(socket_t s,
	char  *buf,
	int len,
	int flags,
	struct sockaddr  *from,
	int fromlen)
{
#ifdef WIN32
	return recvfrom(s, buf, len, flags, from, &fromlen);
#else
	return recvfrom(s, buf, len, flags, from, (socklen_t*)&fromlen);
#endif // WIN32
}

socket_t socketCreateUdpServer(int port)
{
	socket_t	udpSock=-1;
	int	socketErr=-1;
	do 
	{
		udpSock=socket(AF_INET,SOCK_DGRAM,0);
		if (udpSock<0)
		{
			socketErr=socketError();
			printf("create socket failed,error code:%d\n",socketErr);
			break;
		}
		int reuseFlag=1;
		if (setsockopt(udpSock,SOL_SOCKET,SO_REUSEADDR,
			(const char*)&reuseFlag,sizeof reuseFlag)<0)
		{
			printf("setsockopt reuseaddr failed;\n");
			closeSocket(udpSock);
			udpSock=-1;
			break;
		}
#ifdef WIN32
#else
#ifdef SO_REUSEPORT
		if (setsockopt(udpSock, SOL_SOCKET, SO_REUSEPORT,
			(const char*)&reuseFlag, sizeof reuseFlag) < 0) {
				printf("setsockopt(SO_REUSEPORT) error: ");
				closeSocket(udpSock);
				udpSock=-1;
				break;
		}
#endif

#ifdef IP_MULTICAST_LOOP
		const u_int8_t loop = 1;
		if (setsockopt(udpSock, IPPROTO_IP, IP_MULTICAST_LOOP,
			(const char*)&loop, sizeof loop) < 0) {
				printf("setsockopt(IP_MULTICAST_LOOP) error: ");
				closeSocket(udpSock);
				udpSock=-1;
				break;
		}
#endif

#endif

#ifdef WIN32
#else
		//if (port!=0)
		{
#endif
			sockaddr_in	svrAddr;
			svrAddr.sin_family=AF_INET;
			svrAddr.sin_addr.s_addr=0;
			svrAddr.sin_port=htons(port);
			if (bind(udpSock,(const sockaddr*)&svrAddr,sizeof svrAddr)!=0)
			{
				printf("bind() error port:%d\n",port);
				closeSocket(udpSock);
				udpSock=-1;
				break;
			}
#ifdef WIN32
#else
	}
#endif
	} while (0);
	return	udpSock;
}

const char *get_server_ip()
{
	char hostName[32];
	gethostname(hostName,sizeof hostName);
	hostent *hostInfo=gethostbyname(hostName);
	char *pIp=inet_ntoa(*((in_addr*)(*hostInfo->h_addr_list)));
	return pIp;
}

int get_source_port(socket_t sock_id, unsigned short &port)
{
	sockaddr_in	tempAddr;
	tempAddr.sin_port = 0;
	SOCKLEN_T len = sizeof tempAddr;
	if (getsockname(sock_id, (sockaddr*)&tempAddr, (socklen_t*)&len)<0)
	{
		sockaddr_in  svrAddr;
		svrAddr.sin_family = AF_INET;
		svrAddr.sin_addr.s_addr = 0;
		svrAddr.sin_port = htons(port);
		bind(sock_id, (const sockaddr*)&svrAddr, sizeof svrAddr);
		if (getsockname(sock_id, (sockaddr*)&tempAddr, (socklen_t*)&len)<0)
		{
			return	-1;
		}
		else
		{
			port = ntohs(tempAddr.sin_port);
			return	0;
		}
	}
	else
	{
		port = ntohs(tempAddr.sin_port);
		return	0;
	}
	return	-1;
}