
#include "RTSP_server.h"
#include "RTSP.h"
#include "socket_type.h"
#include "taskSchedule.h"
#include "task_stream_stop.h"
#include "RTCP_Process.h"
#include "task_RTCP_message.h"
#include "tcpCacher.h"
#include "TCP_ConnectingManager.h"

//一个socket对应一个buffer
std::map<int,LP_RTSP_buffer>	*g_RTSP_buffer_map()
{
	static std::map<int,LP_RTSP_buffer> rtsp_buffer_map;
	return &rtsp_buffer_map;
}

tcpCacherManager		&g_RTSP_buffer_cacher()
{
	static tcpCacherManager rtcp_buffer_cacher;
	return	rtcp_buffer_cacher;
}

TcpConnectingManager &g_RTSP_connectManager()
{
	static TcpConnectingManager tcpConnectManager;
	return tcpConnectManager;
}


int create_RTSP_server(char *hostName, short port, socketEventCallback callback)
{
	int iRet = 0;
	do
	{
		socket_t server_fd = socketCreateServer(hostName, port);
		if (server_fd <= 0)
		{
			iRet = -1;
			break;
		}
		iRet = socketServerListen(server_fd);
		if (iRet != 0)
		{
			iRet = -1;
			break;
		}
		EVENT_MANAGER	event_manager;
		iRet = eventManagerInit(&event_manager, server_fd, callback);
		if (iRet != 0)
		{
			iRet = -1;
			break;
		}

		eventDispatch(&event_manager);

		eventManagerDestory(&event_manager);
	} while (0);
	return iRet;
}

void createTaskJsonServer(short port, socketEventCallback callback)
{
	int iRet = 0;
	do
	{
		socket_t server_fd = socketCreateServer(nullptr, port);
		if (server_fd <= 0)
		{
			break;
		}
		iRet = socketServerListen(server_fd);
		if (iRet != 0)
		{
			iRet = -1;
			break;
		}
		EVENT_MANAGER	event_manager;
		iRet = eventManagerInit(&event_manager, server_fd, callback);
		if (iRet != 0)
		{
			iRet = -1;
			break;
		}

		eventDispatch(&event_manager);

		eventManagerDestory(&event_manager);
	} while (0);
}

void RTSP_Callback(socket_event_t* data)
{
	//套接字关闭，移除RTSP buffer;
	if (data->clientId<0)
	{
		LOG_FATAL(configData::getInstance().get_loggerId(), "invalid socket id geted");
	}
	if (data->sock_closed)
	{
		RTSP_Close_Client(data->clientId);
		g_RTSP_connectManager().deleteSocket(data->clientId);
		return;
	}

	//移除超时的连接
	timeval timeNow;
	gettimeofday(&timeNow, nullptr);
	g_RTSP_connectManager().updateTimeInfo(data->clientId, timeNow.tv_sec);
	auto timeouts=g_RTSP_connectManager().getTimeoutSockets();
	if (timeouts.size())
	{
		for (auto i : timeouts)
		{
			//RTSP_Close_Client(data->clientId);
			//LOG_INFO(configData::getInstance().get_loggerId(), "time out:" << data->clientId);
			RTSP_Close_Client(i);
			LOG_INFO(configData::getInstance().get_loggerId(), "time out:" << i);
		}
	}
	//!移除超时的连接
	if (data->dataLen<=0)
	{
		return;
	}
	g_RTSP_buffer_cacher().appendBuffer(data->clientId, (char*)data->dataPtr, data->dataLen);
	std::list<DataPacket> dataList;
	g_RTSP_buffer_cacher().getValidBuffer(data->clientId, dataList, parseRtpRtcpRtspData);
	std::list<DataPacket>::iterator it;
	for (it=dataList.begin();it!=dataList.end();it++)
	{

		LP_RTSP_buffer cur_RTSP_buffer = Get_RTSP_buffer(data->clientId);
		//LOG_INFO(configData::getInstance().get_loggerId(), cur_RTSP_buffer->in_buffer);
		if (!cur_RTSP_buffer)
		{
			LOG_INFO(configData::getInstance().get_loggerId(), "no RTSP_buffer geted\n");
			return;
		}
		cur_RTSP_buffer->in_size = 0;
		if (cur_RTSP_buffer->in_size + (*it).size > RTSP_BUFFER_SIZE)
		{
			LOG_INFO(configData::getInstance().get_loggerId(), "invalid RTSP buffer too big;\n");
			cur_RTSP_buffer->in_size = 0;
			return;
		}
		//将接收到的数据添加到in_buffer;
		memcpy(&(cur_RTSP_buffer->in_buffer[cur_RTSP_buffer->in_size]), (*it).data, (*it).size);
		memcpy(&cur_RTSP_buffer->sock_addr, &data->target_addr.sin_addr, sizeof(in_addr));
		cur_RTSP_buffer->in_size += data->dataLen;
		//处理RTSP buffer;
		if (RTSP_handler(cur_RTSP_buffer) != 0)
		{
			LOG_INFO(configData::getInstance().get_loggerId(), "invalid input message\n");
		}
		if ((*it).data)
		{
			delete[](*it).data;
			(*it).data = 0;
		}
	}
}

LP_RTSP_buffer Get_RTSP_buffer(socket_t const	sock_id)
{
	std::map<int, LP_RTSP_buffer>::iterator it;
	it = g_RTSP_buffer_map()->find((int)sock_id);
	if (it!=g_RTSP_buffer_map()->end())
	{
		return (*it).second;
	}
	//not found,create a new bufer;
	LP_RTSP_buffer tmp_buffer = new RTSP_buffer();
	//create a default session;
	LP_RTSP_session tmpSession = new RTSP_session();
	//memset(tmpSession, 0,sizeof(RTSP_session) );
	//memset(tmp_buffer, 0, sizeof(RTSP_buffer) );
	tmp_buffer->session_list.push_back(tmpSession);
	tmp_buffer->sock_id = sock_id;
	g_RTSP_buffer_map()->insert(std::pair<int,LP_RTSP_buffer>((int)sock_id,tmp_buffer));
	return tmp_buffer;
}

int RTSP_handler(LP_RTSP_buffer rtsp)
{
	int iRet = 0;
	do
	{
		iRet = is_RTCP_data(rtsp);
		if (rtsp->in_buffer[0] != '$')
		{
			iRet = 0;
			if (RTSP_end(rtsp) != 0)
			{
				iRet = 0;
				LOG_INFO(configData::getInstance().get_loggerId(),"not full message;");
				break;
			}
			RTSP_CMD_STATE method = RTSP_validate_method(rtsp);
			if (method < 0)
			{
				iRet = is_RTCP_data(rtsp);
				if (iRet < 0)
				{
					RTSP_send_err_reply(400, 0, rtsp);
					iRet = -1;
					LOG_INFO(configData::getInstance().get_loggerId(), "invalid method\n");
					break;
				}
			}
			iRet = RTSP_state_machine(rtsp, method);
			if (iRet != 0)
			{
				break;
			}
			//clear in buffer
			//send out buffer
			iRet = socketSend(rtsp->sock_id, rtsp->out_buffer.c_str(), rtsp->out_size);
			if (iRet != rtsp->out_size)
			{
				LOG_INFO(configData::getInstance().get_loggerId(),
					"rtsp send error,sock id:" << rtsp->sock_id << "\n" << "error code:"<< socketError());
				iRet = -1;
				break;
			}
			else
				iRet = 0;
			//clear out buffer
			rtsp->out_size = 0;
		}
		else if (rtsp->in_buffer[0] == '$')
		{
			RTCP_process rtcp_process;

			DataPacket dataPacket;
			dataPacket.size = rtsp->in_size;
			//dataPacket.data = const_cast<char*>(rtsp->in_buffer.c_str());
			dataPacket.data = rtsp->in_buffer;
			int iRet = rtcp_process.parse_RTCP_buf(dataPacket);
			if (iRet != 0)
			{
				LOG_INFO(configData::getInstance().get_loggerId(), "bad rtcp message");
				break;
			}
			for (int i = 0; i < rtcp_process.report_block_count(); i++)
			{
				//封装报告，添加到队列;
				taskScheduleObj *taskObj = 0;
				taskObj = new taskRtcpMessage(rtcp_process);
				taskSchedule::getInstance().addTaskObj(taskObj);
			}
		}
		if (iRet != 0)
		{
			break;
		}
		if (iRet == 0)
		{
			rtsp->in_size = 0;
			break;
		}

	} while (0);

	return iRet;
}


int TCP_data_parse_valid_RTSP_RTP_RTCP_data(LP_RTSP_buffer rtsp, std::list<DataPacket> &data_list)
{
	if (data_list.size())
	{
		std::list<DataPacket>::iterator it;
		for (it=data_list.begin();it!=data_list.end(); it++)
		{
			delete[](*it).data;
			(*it).data = 0;
		}
		data_list.clear();
	}
	int cur = 0;
	while (cur < rtsp->in_size)
	{
		if (rtsp->in_buffer[cur] == '$')
		{
			if (rtsp->in_size - cur < 4)
			{
				break;
			}
			char size1, size2;
			size1 = rtsp->in_buffer[cur + 2];
			size2 = rtsp->in_buffer[cur + 3];
			unsigned short pkg_size = ((size1 << 8) | size2);
			if (rtsp->in_size - cur < pkg_size + 4)
			{
				break;
			}
			DataPacket tmp_packet;
			tmp_packet.size = pkg_size + 4;
			tmp_packet.data = new char[tmp_packet.size];
			memcpy(tmp_packet.data, &rtsp->in_buffer[cur], tmp_packet.size);
			data_list.push_back(tmp_packet);
			cur += tmp_packet.size;

		}
		else
		{
			int start_pos = cur;
			while (true)
			{
				if (cur >= rtsp->in_size)
				{
					break;
				}
				if (cur < start_pos + 4)
				{
					cur += 4;
				}
				if (rtsp->in_buffer[cur - 3] == '\r'&&
					rtsp->in_buffer[cur - 2] == '\n'&&
					rtsp->in_buffer[cur - 1] == '\r'&&
					rtsp->in_buffer[cur - 0] == '\n')
				{
					//get rtsp message;
					DataPacket tmp_packet;
					tmp_packet.size = cur - start_pos + 1;
					tmp_packet.data = new char[tmp_packet.size];
					memcpy(tmp_packet.data, rtsp->in_buffer + start_pos, tmp_packet.size);
					data_list.push_back(tmp_packet);
					cur++;
					break;
				}
				cur++;
			}
		}

	}
	if (data_list.size() == 0)
	{
		return 0;
	}
	return 0;
}

int RTSP_end(LP_RTSP_buffer rtsp)
{
	if (rtsp->in_size < 4)
	{
		return -1;
	}
	if (rtsp->in_buffer[rtsp->in_size - 4] == '\r'&&
		rtsp->in_buffer[rtsp->in_size - 3] == '\n'&&
		rtsp->in_buffer[rtsp->in_size - 2] == '\r'&&
		rtsp->in_buffer[rtsp->in_size - 1] == '\n')
	{
		return 0;
	}
	return -1;
}

int RTSP_Close_Client(socket_t sock_id)
{
	int iRet = 0;
	do
	{
		//__try
		{
			LOG_INFO(configData::getInstance().get_loggerId(), "RTSP Client begin;" << sock_id);
			LP_RTSP_buffer rtsp_buffer = Get_RTSP_buffer(sock_id);


			free_RTSP_buffer(rtsp_buffer);
			std::map<int, LP_RTSP_buffer>::iterator it;
			it = g_RTSP_buffer_map()->find((int)sock_id);
			if (it!=g_RTSP_buffer_map()->end())
			{
				//delete (*it).second;
				//(*it).second = 0;
				g_RTSP_buffer_map()->erase((int)sock_id);
			}

			g_RTSP_buffer_cacher().removeCacher(sock_id);
			//通知事件调度者，该任务结束;
			taskStreamStop *stopTask = new taskStreamStop(sock_id);
			//taskSchedule::getInstance().addTaskObj(stopTask);
			taskSchedule::getInstance().executeTaskObj(stopTask);
			LOG_INFO(configData::getInstance().get_loggerId(), "RTSP Client closed;" << sock_id);
		}/*__except (EXCEPTION_EXECUTE_HANDLER)
		{
			int a = 32;
		}*/
	} while (0);
	return iRet;
}

RTSP_CMD_STATE RTSP_validate_method(LP_RTSP_buffer rtsp)
{
	int iRet = 0;
	RTSP_CMD_STATE	 mid = RTSP_ID_BAD_REQ;
	do
	{
		char method[32];
		char object[256];
		char version[32];
		unsigned int seq;
		int	 pcnt;
		*method = *object = '\0';
		seq = 0;

		if ((pcnt = sscanf(rtsp->in_buffer, "%31s %255s %31s", method, object, version)) != 3)
		{
			iRet = -1;
			break;
		}
		char *hdr = strstr(rtsp->in_buffer, HDR_CSEQ);
		if (!hdr)
		{
			iRet = -1;
			break;
		}
		if (!strstr(hdr, HDR_CSEQ))
		{
			iRet = -1;
			break;
		}
		if (sscanf(hdr, HDR_CSEQ": %u", &seq) != 1)
		{
			iRet = -1;
			break;
		}
		if (strcmp(method, RTSP_METHOD_DESCRIBE) == 0)
		{
			mid = RTSP_ID_DESCRIBE;
		}
		else if (strcmp(method, RTSP_METHOD_ANNOUNCE) == 0)
		{
			mid = RTSP_ID_ANNOUNCE;
		}
		else if (strcmp(method, RTSP_METHOD_GET_PARAMETERS) == 0)
		{
			mid = RTSP_ID_GET_PARAMETERS;
		}
		else if (strcmp(method, RTSP_METHOD_OPTIONS) == 0)
		{
			mid = RTSP_ID_OPTIONS;
		}
		else if (strcmp(method, RTSP_METHOD_PAUSE) == 0)
		{
			mid = RTSP_ID_PAUSE;
		}
		else if (strcmp(method, RTSP_METHOD_PLAY) == 0)
		{
			mid = RTSP_ID_PLAY;
		}
		else if (strcmp(method, RTSP_METHOD_RECORD) == 0)
		{
			mid = RTSP_ID_RECORD;
		}
		else if (strcmp(method, RTSP_METHOD_REDIRECT) == 0)
		{
			mid = RTSP_ID_REDIRECT;
		}
		else if (strcmp(method, RTSP_METHOD_SETUP) == 0)
		{
			mid = RTSP_ID_SETUP;
		}
		else if (strcmp(method, RTSP_METHOD_SET_PARAMETER) == 0)
		{
			mid = RTSP_ID_SET_PARAMETER;
		}
		else if (strcmp(method, RTSP_METHOD_TEARDOWN) == 0)
		{
			mid = RTSP_ID_TEARDOWN;
		}
		iRet = mid;
		rtsp->rtsp_seq = seq;
	} while (0);
	return mid;
}

int RTSP_send_err_reply(int errCode, char *addon, LP_RTSP_buffer rtsp)
{
	int iRet = 0;
	char *reply = 0;
	do
	{
		const int reply_base_size = 256;
		if (addon)
		{
			reply = new char[reply_base_size + strlen(addon)];
		}
		else
		{
			reply = new char[reply_base_size];
		}

		sprintf(reply, "%s %d %s" RTSP_EL"CSeq: %d", RTSP_VER, errCode, get_stat(errCode), rtsp->rtsp_seq);
		strcat(reply, RTSP_EL RTSP_EL);
		socketSend(rtsp->sock_id, reply, strlen(reply));
	} while (0);
	if (reply != 0)
	{
		delete[]reply;
		reply = 0;
	}
	return iRet;
}

int is_RTCP_data(LP_RTSP_buffer rtsp)
{
	int iRet = 0;
	if (rtsp->in_buffer[0] != '$')
	{
		return -1;
	}
	return iRet;
}
