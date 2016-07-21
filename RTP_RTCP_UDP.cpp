#include "RTSP_server.h"
#include "RTCP_Process.h"
#include "taskSchedule.h"
#include "task_RTCP_message.h"

#include <thread>

socket_t g_rtp_svr_socket;
socket_t g_rtcp_svr_socket;
unsigned short g_rtp_port;
unsigned short g_rtcp_port;

std::thread g_rtp_svr_handle;
std::thread g_rtcp_svr_handle;



void rtp_server(socketEventCallback callback);
void rtcp_server(socketEventCallback callback);

int create_RTP_RTCP_server(socketEventCallback RTP_callback,socketEventCallback RTCP_callback)
{
	int iRet=0;
	do
	{
		//系统自动分配RTP端口号;
		g_rtp_svr_socket=socketCreateUdpServer(0);
		if (g_rtp_svr_socket<0)
		{
			iRet=-1;
			break;
		}
		//读取端口号;
		if (0!=get_source_port(g_rtp_svr_socket,g_rtp_port))
		{
			printf("get udp RTP source port failed;\n");
			iRet=-1;
			closeSocket(g_rtp_svr_socket);
			break;
		}
		//printf("rtp port:%d", g_rtp_port);
		//端口号奇偶;
		if ((g_rtp_port&1)!=0)
		{
			closeSocket(g_rtp_svr_socket);
			continue;
		}
		//RTCP
		g_rtcp_svr_socket=socketCreateUdpServer(g_rtp_port+1);
		if (g_rtcp_svr_socket<0)
		{
			closeSocket(g_rtp_svr_socket);
			closeSocket(g_rtcp_svr_socket);
			continue;
		}
		if (0!=get_source_port(g_rtcp_svr_socket,g_rtcp_port))
		{
			printf("get udp RTCP source port failed;\n");
			closeSocket(g_rtp_svr_socket);
			closeSocket(g_rtcp_svr_socket);
			iRet=-1;
			break;
		}
		//printf("rtcp port:%d", g_rtcp_port);
		if (g_rtcp_port!=g_rtp_port+1)
		{
			iRet=-1;
			break;
		}
		LOG_INFO(configData::getInstance().get_loggerId(), "\nrtp port:" << g_rtp_port << "\nrtcp port:" << g_rtcp_port<<"\n");
		//两个线程，监听输入，调用回调;
		std::thread tmp_rtp(rtp_server, RTP_Callback);
		g_rtp_svr_handle = std::move(tmp_rtp);
		std::thread tmp_rtcp(rtcp_server, RTCP_callback);
		g_rtcp_svr_handle = std::move(tmp_rtcp);
		break;
	} while (true);
	return iRet;
}

unsigned short Get_RTP_Port()
{
	return g_rtp_port;
}

unsigned short Get_RTCP_Port()
{
	return g_rtcp_port;
}

void rtp_server(socketEventCallback callback)
{
	char buf[1500];
	int buf_size=sizeof buf;
	sockaddr_in client_addr;
	int len =sizeof client_addr;
	socket_event_t data;
	//in_addr
	while(true)
	{
		int n=recvfrom(g_rtp_svr_socket,buf,buf_size,0,(sockaddr*)&client_addr,(socklen_t*)&len);
		unsigned short port = ntohs(client_addr.sin_port);
		char *ip_addr = inet_ntoa((in_addr)client_addr.sin_addr);
		if (n>0)
		{
			if (callback)
			{
				data.dataLen=n;
				data.dataPtr=buf;
				data.clientId=g_rtp_svr_socket;
				data.sock_closed=false;
				memcpy(&data.target_addr,&client_addr,len);
				callback(&data);
			}
		}
	}
}


void rtcp_server(socketEventCallback callback)
{
	char buf[1500];
	int buf_size=sizeof buf;
	sockaddr_in client_addr;
	int len =sizeof client_addr;
	socket_event_t data;

	while(true)
	{
		int n=recvfrom(g_rtcp_svr_socket,buf,buf_size,0,(sockaddr*)&client_addr,(socklen_t*)&len);
		unsigned short port = ntohs(client_addr.sin_port);
		char *ip_addr = inet_ntoa((in_addr)client_addr.sin_addr);
		if (n>0)
		{
			if (callback)
			{
				data.dataLen=n;
				data.dataPtr=buf;
				data.clientId=g_rtp_svr_socket;
				data.sock_closed=false;
				memcpy(&data.target_addr,&client_addr,len);
				callback(&data);
			}
		}
	}
}

void RTP_Callback(socket_event_t *data)
{
	char *data_recved=(char*)data->dataPtr;
}

void RTCP_Callback(socket_event_t *data)
{
	//解析RTCP信息，判断是否为接收者报告，如果是接收者报告，添加到事件队列，以通知对应的直播源;
	int iRet = 0;
	do
	{
		static RTCP_process rtcp_process;
		DataPacket data_packet;
		data_packet.size = data->dataLen;
		data_packet.data = (char*)data->dataPtr;
		int iRet = rtcp_process.parse_RTCP_buf(data_packet);
		if (iRet != 0)
		{
			LOG_INFO(configData::getInstance().get_loggerId(),"bad rtcp message");
			break;
		}
		for (int i = 0; i < rtcp_process.report_block_count(); i++)
		{
			//封装报告，添加到队列;
			taskScheduleObj *taskObj = 0;
			taskObj = new taskRtcpMessage(rtcp_process);
			taskSchedule::getInstance().addTaskObj(taskObj);
		}
	} while (0);
}

