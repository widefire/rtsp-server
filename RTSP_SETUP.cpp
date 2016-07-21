#include "RTSP_server.h"
#include "configData.h"

int RTSP_SETUP(LP_RTSP_buffer rtsp, LP_RTSP_session &session)
{
	int iRet = 0;
	char url[255];
	char server[255];
	char object[255];
	unsigned short port;
	char *p = 0;
	char control_id[255];
	int ssResult = 0;
	char trash[255];
	char ch_tmp[2][255];
	memset(control_id, 0, sizeof(control_id));
	do
	{
		//RTSP buffer有效性判断;
		ssResult = sscanf(rtsp->in_buffer, "%[^\r]", trash);
		if (ssResult != 1)
		{
			RTSP_send_err_reply(400, 0, rtsp);
			iRet = -1;
			break;
		}
		ssResult = sscanf(trash, " %*s %254s %s %s", url, ch_tmp[0], ch_tmp[1]);
		if (ssResult != 3 && ssResult != 2)
		{
			RTSP_send_err_reply(400, 0, rtsp);
			iRet = -1;
			break;
		}
		bool	control_found = false;
		if (ssResult == 3)
		{
			strcpy(control_id, ch_tmp[0]);
			control_found = true;
		}
		int parse_result = parse_url(url, server, sizeof server, port, object, sizeof object);
		if (parse_result == 1)
		{
			RTSP_send_err_reply(400, 0, rtsp);
			iRet = -1;
			break;
		}
		else if (parse_result == -1)
		{
			RTSP_send_err_reply(500, 0, rtsp);
			iRet = -1;
			break;
		}
		//文件流是否存在;
		//上级目录不可
		if (strstr(object, "../"))
		{
			RTSP_send_err_reply(403, 0, rtsp);
			iRet = -1;
			break;
		}
		if (strstr(object, "./"))
		{
			RTSP_send_err_reply(403, 0, rtsp);
			iRet = -1;
			break;
		}
		//包含 . 则是文件，可能有下级目录

		char prefix[255], stuffix[255];

		const char *pStuffix = nullptr;
		//前缀为live 没有.则为实时流
		if (strlen(object) > strlen(RTSP_LIVE) && strncmp(object, RTSP_LIVE, strlen(RTSP_LIVE)) == 0)
		{
			LOG_INFO(configData::getInstance().get_loggerId(), object);
			//提取control id和流名
			char streamName[255];
			memset(streamName, 0, sizeof(streamName));
			char *lastOblique = nullptr;
			for (int i = strlen(object)-1; i >0; i--)
			{
				if (object[i]=='/')
				{
					lastOblique = object + i;
					break;
				}
			}
			if (!lastOblique||strlen(lastOblique)<2)
			{
				RTSP_send_err_reply(404, 0, rtsp);
				iRet = -1;
				break;
			}
			strcpy(control_id, lastOblique + 1);
			strncpy(streamName, object, strlen(object) - strlen(lastOblique));
			
		}
		else
		{
			//去掉最后的control
			p = strrchr(object, '.');
			for (int i = strlen(object); i>1; i--)
			{
				if (object[i] == '.')
				{
					pStuffix = object + i ;
					break;
				}
			}
			if (pStuffix == nullptr)
			{
				RTSP_send_err_reply(404, 0, rtsp);
				iRet = -1;
				break;
			}
			//有空格，不用去 control
			if (control_found == true)
			{
				/*ssResult = sscanf(object, "%[^.]%[^/]", prefix, stuffix);
				if (ssResult != 2)
				{
					RTSP_send_err_reply(400, 0, rtsp);
					iRet = -1;
					break;
				}*/
				strcpy(stuffix, pStuffix);
			}
			else
			{
				//去掉control
				/*ssResult = sscanf(object, "%[^.]%[^/]/%s", prefix, stuffix, control_id);
				if (ssResult != 3)
				{
					RTSP_send_err_reply(400, 0, rtsp);
					iRet = -1;
					break;
				}*/
				ssResult = sscanf(pStuffix, "%[^/]/%s", stuffix, control_id);
				if (ssResult!=2)
				{

					RTSP_send_err_reply(400, 0, rtsp);
					iRet = -1;
					break;
				}
			}



			iRet = 0;
			if (p == 0)
			{
				printf("no file suffix,maybe a realtime stream;\n");
				RTSP_send_err_reply(415, 0, rtsp);
				iRet = -1;
				break;
			}
			else
			{
				iRet = is_supported_file_stuffix(stuffix);
				if (iRet < 0)
				{
					iRet = -1;
					RTSP_send_err_reply(415, 0, rtsp);
					break;
				}
				iRet = 0;
			}
			//control是否存在;

			char file_name[255];
			memset(file_name, 0, sizeof(file_name));
			if (!control_found)
			{
				//找到最后一个/的位置
				char *lastOblique = nullptr;
				for (int  i = strlen(object); i >0 ; i--)
				{
					if (object[i]=='/')
					{
						lastOblique = object + i;
						break;
					}
				}
				if (lastOblique==nullptr||strlen(lastOblique)<1)
				{
					RTSP_send_err_reply(404, 0, rtsp);
					iRet = -1;
					break;
				}
				
				strncpy(file_name, object, strlen(object) - strlen(lastOblique));
				/*ssResult = sscanf(object, "%[^/]/%s", file_name, control_id);
				if (ssResult != 2)
				{
					RTSP_send_err_reply(400, 0, rtsp);
					iRet = -1;
					break;
				}*/
			}
			else
			{
				ssResult = sscanf(object, "%[^/]", file_name);
				if (ssResult != 1)
				{
					RTSP_send_err_reply(400, 0, rtsp);
					iRet = -1;
					break;
				}
			}

			//int exisit = access(file_name, 0);
			std::string fullName = configData::getInstance().getFlvSaveDir() + file_name;
			int exisit = access(fullName.c_str(), 0);
			if (exisit != 0)
			{
				RTSP_send_err_reply(404, 0, rtsp);
				iRet = -1;
				break;
			}
		}
		//当前SESSION的CONTROL
		//如果当前SESSION没有control_id,赋值。如果有且不同：寻找相同的或新建一个;
		bool	session_found = false;
		if (strlen(session->control_id) == 0)
		{
			strcpy(session->control_id, control_id);
			session_found = true;
		}
		else
		{
			std::list<LP_RTSP_session>::iterator it;
			for (it = rtsp->session_list.begin(); it != rtsp->session_list.end(); it++)
			{
				if (strcmp(control_id, (*it)->control_id) == 0)
				{
					session = (*it);
					session_found = true;
				}
			}
		}
		if (!session_found)
		{
			RTSP_session *tmp_session = new RTSP_session();
			memset(tmp_session, 0, sizeof(RTSP_session));
			rtsp->session_list.push_back(tmp_session);
			session = tmp_session;
			strcpy(session->control_id, control_id);
			session_found = true;
		}

		if (strcmp(control_id, RTSP_CONTROL_ID_0) != 0)
		{
			RTSP_send_err_reply(404, 0, rtsp);
			iRet = -1;
			break;
		}
		//解析协议是UDP还是TCP;
		p = strstr(rtsp->in_buffer, HDR_TRANSPORT);
		if (p == 0)
		{
			RTSP_send_err_reply(400, 0, rtsp);
			iRet = -1;
			break;
		}
		char transPort[100], cast_model[100];
		ssResult = sscanf(p, HDR_TRANSPORT": %[^;];%[^;];", transPort, cast_model);
		if (ssResult != 2)
		{
			RTSP_send_err_reply(400, 0, rtsp);
			iRet = -1;
			break;
		}
		//cast model;
		if (strcmp(cast_model, "unicast") == 0)
		{
			session->unicast = true;
		}
		else
		{
			session = false;
		}
		if (strcmp(transPort, RTSP_RTP_AVP) == 0 || strcmp(transPort, RTSP_RTP_AVP_UDP) == 0)
		{
			//udp;
			session->transport_type = RTSP_OVER_UDP;
			//get port
			char *client_port = strstr(p, "client_port=");
			if (client_port == 0)
			{
				RTSP_send_err_reply(400, 0, rtsp);
				iRet = -1;
				break;
			}
			ssResult = sscanf(client_port, "%[^=]=%hu-%hu", trash, &(session->RTP_c_port), &(session->RTCP_c_port));
			if (ssResult != 3)
			{
				iRet = -1;
				break;
			}
			session->RTP_s_port = Get_RTP_Port();
			session->RTCP_s_port = Get_RTCP_Port();
		}
		else if (strcmp(transPort, RTSP_RTP_AVP_TCP) == 0)
		{
			//tcp;
			session->transport_type = RTSP_OVER_TCP;
			//interleaved
			char *interleaved = strstr(p, "interleaved=");
			if (!interleaved)
			{
				RTSP_send_err_reply(400, 0, rtsp);
				iRet = -1;
				break;
			}
			ssResult = sscanf(interleaved, "%[^=]=%d-%d", trash, &session->RTP_channel, &session->RTCP_channel);
			if (ssResult != 3)
			{
				RTSP_send_err_reply(400, 0, rtsp);
				iRet = -1;
				break;
			}
		}
		else
		{
			RTSP_send_err_reply(400, 0, rtsp);
			iRet = -1;
			break;
		}
		//生成session id;
		if ((p = strstr(rtsp->in_buffer, HDR_SESSION)) != 0)
		{
			if (sscanf(p, "%*s %s", &session->session_id) != 1)
			{
				RTSP_send_err_reply(454, 0, rtsp);
				iRet = -1;
				break;
			}
		}
		else
		{
			generateGUID64(session->session_id);
		}
		session->ssrc = generate_random32();
		session->first_seq = rand() % 0xffff;
		timeval time_now;
		gettimeofday(&time_now, 0);
		if (strcmp(control_id, RTSP_CONTROL_ID_0) == 0)
		{
			session->start_rtp_time = convert_to_RTP_timestamp(time_now, RTP_video_freq);
		}
		else
		{
			session->start_rtp_time = convert_to_RTP_timestamp(time_now, RTP_audio_freq);
		}
		//保存对应session;
		//发送setup结果,协议，源目标地址，TCP：RTP和RTCP的channel,UDP server port;
		iRet = RTSP_SETUP_reply(rtsp, session);
		if (iRet != 0)
		{
			iRet = -1;
			break;
		}
	} while (0);
	return iRet;
}

int RTSP_SETUP_reply(LP_RTSP_buffer rtsp, LP_RTSP_session session)
{
	int iRet = 0;
	do
	{
		char reply[RTSP_BUFFER_SIZE];
		sprintf(reply, "%s %d %s" RTSP_EL"CSeq: %d" RTSP_EL"Server: %s%s" RTSP_EL,
			RTSP_VER, 200, get_stat(200), rtsp->rtsp_seq,
			configData::getInstance().get_server_name(), configData::getInstance().get_server_version()
		);
		add_RTSP_timestamp(reply);
		int cur = strlen(reply);
		cur += sprintf(reply + cur, "Session: %s;timeout=%d" RTSP_EL, session->session_id.c_str(), configData::getInstance().RTSP_timeout());
		cur += sprintf(reply + cur, "Transport: ");
		switch (session->transport_type)
		{
		case RTSP_OVER_TCP:
			cur += sprintf(reply + cur, "RTP/AVP/TCP;unicast;interleaved=%d-%d", \
				session->RTP_channel, session->RTCP_channel);
			break;
		case RTSP_OVER_UDP:
			if (session->unicast)
			{
				cur += sprintf(reply + cur, "RTP/AVP;unicast;client_port=%d-%d;source=%s;server_port=", \
					session->RTP_c_port, session->RTCP_c_port, get_server_ip());
			}
			else
			{
				//multicast not realize;
				//cur += sprintf(reply + cur, "RTP/AVP;multicast;ttl=%d;destination=%s;port=", TTL, multicast addr);
			}
			cur += sprintf(reply + cur, "%d-%d", session->RTP_s_port, session->RTCP_s_port);
			break;
		case RTSP_TRANSPORT_NO_TYPE:
			iRet = -1;
			break;
		}
		if (iRet != 0)
		{
			break;
		}
		cur += sprintf(reply + cur, ";ssrc=%u", session->ssrc);
		strcat(reply, RTSP_EL);
		strcat(reply, RTSP_EL);
		iRet = RTSP_add_out_buffer(reply, strlen(reply), rtsp);
		if (iRet != 0)
		{
			break;
		}
	} while (0);
	return	iRet;
}
