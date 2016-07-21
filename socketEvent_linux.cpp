#ifndef WIN32
#include "socketEvent.h"
#include <stdio.h>
#include <sys/epoll.h>
#include <iostream>
#include "configData.h"

const int  TCP_BUFFER_LEN = 4096;
const int  EPOLL_MAX_EVENT = 128;
int eventManagerInit(event_manager_t* eventManager, socket_t serverId, socketEventCallback callback)
{
	int iRet = 0;
	do
	{
		eventManager->epfd = epoll_create1(0);
		if (eventManager->epfd == -1)
		{
			printf("epoll_create failed;\n");
			iRet = -1;
			break;
		}
		eventManager->serverSocket = serverId;
		eventManager->callback = callback;
		epoll_event ev;
		ev.data.fd = serverId;
		ev.events = EPOLLIN | EPOLLET ;
		iRet = epoll_ctl(eventManager->epfd, EPOLL_CTL_ADD, serverId, &ev);
		if (iRet == -1)
		{
			printf("epoll_ctl failed;\n");
			break;
		}
	} while (0);
	if (iRet == 0)
	{
		printf("eventManagerInit success\n");
	}
	return iRet;
}

int eventDispatch(event_manager_t* eventManager)
{
	//printf("begin dispatch");
	struct sockaddr serveraddr, clientaddr;
	int connfd, sockfd;
	int i, n, clilen, serlen, iRet;
	struct epoll_event ev;
	struct epoll_event events[EPOLL_MAX_EVENT];
	char buffer[TCP_BUFFER_LEN] = "Hello,Client!";
	while (1)
	{
		int nfds;

		nfds = epoll_wait(eventManager->epfd, events, EPOLL_MAX_EVENT, -1);

		for (i = 0; i < nfds; i++) {
			if ((events[i].events&EPOLLERR) ||
				(events[i].events&EPOLLHUP) ||
				(!(events[i].events&EPOLLIN)))
			{
				printf("epoll error ;\n");
				socket_event_t	data;
				data.clientId = events[i].data.fd;
				data.dataLen = 0;
				data.dataPtr = nullptr;
				data.sock_closed = true;
				//memcpy(&data.target_addr, &clientAddr, sizeof data.target_addr);
				if (eventManager->callback != 0)
				{
					//printf("callback:data %s\n", buffer);
					eventManager->callback(&data);
				}
				close(events[i].data.fd);
				events[i].data.fd = -1;
				continue;
			}
			else if (eventManager->serverSocket == events[i].data.fd)
			{
				while (1)
				{
					//accept
					sockaddr inAddr;
					socklen_t addrLen;
					int infd;
					char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];
					addrLen = sizeof inAddr;
					infd = accept(eventManager->serverSocket, &inAddr, &addrLen);
					if (infd == -1)
					{
						if ((errno == EAGAIN) ||
							(errno == EWOULDBLOCK))
						{
							//printf("connect is incoming;\n");
							break;
						}
						else
						{
							//printf("accept failed;\n");
							break;
						}
					}

					iRet = getnameinfo(&inAddr, addrLen,
						hbuf, sizeof hbuf,
						sbuf, sizeof sbuf,
						NI_NUMERICHOST | NI_NUMERICSERV);
					if (iRet == 0)
					{
						/*printf("accepted connect on descriptor %d (host=%s  ,port=%s)\n",
							infd,hbuf,sbuf);*/
						/*LOG_INFO(configData::getInstance().get_loggerId(), "accepted connect on descriptor" << infd <<
							"host=" << hbuf << ",port=" << sbuf);*/
					}
					iRet = socketSetNoBlocking(infd);
					if (iRet != 0)
					{
						//printf("set socket %d noblock failed,wo close it");
						LOG_INFO(configData::getInstance().get_loggerId(), "set socket " << sockfd << " no block failed,wo close it");
						socket_event_t	data;
						data.clientId = infd;
						data.dataLen = 0;
						data.dataPtr = nullptr;
						data.sock_closed = true;
						if (eventManager->callback != 0)
						{
							eventManager->callback(&data);
						}
						close(infd);
						break;
					}
					socket_event_t	data;
					data.clientId = infd;
					data.dataLen = 0;
					data.dataPtr = nullptr;
					data.sock_closed = false;
					if (eventManager->callback != 0)
					{
						//LOG_INFO(configData::getInstance().get_loggerId(), "callback for connected " << infd);
						eventManager->callback(&data);
					}
					//add
					ev.data.fd = infd;
					ev.events = EPOLLET | EPOLLIN;
					iRet = epoll_ctl(eventManager->epfd, EPOLL_CTL_ADD, infd, &ev);
					break;
				}
				continue;
			}
			else if (events[i].events & EPOLLIN)
			{
				sockfd = events[i].data.fd;
				if (sockfd < 0)
					continue;

				sockaddr clientAddr;
				int len = sizeof(sockaddr);
				iRet = recvfrom(sockfd, buffer, sizeof(buffer), 0, &clientAddr, (socklen_t*)&len);
				if (iRet > 0)
				{
					socket_event_t	data;
					data.clientId = sockfd;
					data.dataLen = iRet;
					data.dataPtr = buffer;
					data.sock_closed = false;
					memcpy(&data.target_addr, &clientAddr, sizeof data.target_addr);
					if (eventManager->callback != 0)
					{
						//printf("callback:data %s\n", buffer);
						eventManager->callback(&data);
					}
				}
				else if (n == 0)
				{
					socket_event_t	data;
					data.clientId = sockfd;
					data.dataLen = iRet;
					data.dataPtr = buffer;
					data.sock_closed = true;
					memcpy(&data.target_addr, &clientAddr, sizeof data.target_addr);
					if (eventManager->callback != 0)
					{
						//printf("callback:data %s\n", buffer);
						LOG_INFO(configData::getInstance().get_loggerId(), "recv o data,close " << sockfd);
						eventManager->callback(&data);
					}
					close(sockfd);
					events[i].data.fd = -1;
				}
			}
		}

	}
	return 0;
}

int eventManagerDestory(event_manager_t* eventManager)
{
	close(eventManager->epfd);
	close(eventManager->serverSocket);
	return 0;
}

#endif

