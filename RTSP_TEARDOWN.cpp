#include "RTSP_server.h"

#include "taskSchedule.h"
#include "task_stream_stop.h"

int RTSP_TEARDOWN(LP_RTSP_buffer rtsp,LP_RTSP_session session)
{
	//关闭所有的session
	std::string session_id;
	char *p = 0;
	char url[255];
	char server[255];
	char object[255];
	int ssResult = 0;
	char trash[255];
	unsigned short port = 0;

	int iRet=0;
	do
	{
		//CSeq
		if ((p=strstr(rtsp->in_buffer,HDR_CSEQ))==0)
		{
			RTSP_send_err_reply(400, 0, rtsp);
			iRet = -1;
			break;
		}
		else
		{
			if ((sscanf(p,"%254s %s",trash,&rtsp->rtsp_seq))!=2)
			{
				RTSP_send_err_reply(400, 0, rtsp);
				iRet = -1;
				break;
			}

		}
		//Url
		if (!sscanf(rtsp->in_buffer, " %*s %254s ", url)) {
			RTSP_send_err_reply(400, 0, rtsp);	/* bad request */
			return -1;
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
		if (strlen(object) > strlen(RTSP_LIVE) && strncmp(object, RTSP_LIVE, strlen(RTSP_LIVE)) == 0)
		{
			//实时流是否存在
			LOG_INFO(configData::getInstance().get_loggerId(), "teardown live,not processed,live and file in the same list");
		}
		else
		{
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
			p = strrchr(object, '.');
			/*char prefix[255], stuffix[255];
			ssResult = sscanf(object, "%[^.]%[^/]", prefix, stuffix);
			if (ssResult != 2)
			{
				RTSP_send_err_reply(400, 0, rtsp);
				iRet = -1;
				break;
			}*/
			char file_name[255];
			//ssResult = sscanf(object, "%[^/]", file_name);
			strcpy(file_name, object);
			if (file_name[strlen(file_name)-1]=='/')
			{
				file_name[strlen(file_name) - 1] = '\0';
			}
			/*if (ssResult != 1)
			{
				RTSP_send_err_reply(400, 0, rtsp);
				iRet = -1;
				break;
			}*/
			std::string fullName = configData::getInstance().getFlvSaveDir() + file_name;
			int exisit = access(fullName.c_str(), 0);
			//int exisit = access(file_name, 0);
			if (exisit != 0)
			{
				RTSP_send_err_reply(404, 0, rtsp);
				iRet = -1;
				break;
			}
		}
		
		//session
		if ((p = strstr(rtsp->in_buffer, HDR_SESSION)) != NULL) {
			session_id.resize(64);
			if (sscanf(p, "%254s %s", trash, &session_id[0]) != 2) {
				RTSP_send_err_reply(454, 0, rtsp);
				iRet = -1;
				break;
			}
		}
		else {
			session_id = -1;
		}

		//send reply
		iRet = RTSP_TEARDOWN_reply(rtsp,session);
		if (iRet!=0)
		{
			break;
		}
		//send close task
		taskStreamStop *stopTask = new taskStreamStop(rtsp->sock_id);
		taskSchedule::getInstance().addTaskObj(stopTask);
	} while (0);
	return iRet;
}

int RTSP_TEARDOWN_reply(LP_RTSP_buffer rtsp, LP_RTSP_session session)
{
	int iRet=0;
	do
	{
		char reply[RTSP_BUFFER_SIZE];
		char temp[128];
		sprintf(reply, "%s %d %s" RTSP_EL"CSeq: %ld" RTSP_EL"Server: %s/%s" RTSP_EL, RTSP_VER, 200, get_stat(200), rtsp->rtsp_seq,
			configData::getInstance().get_server_name(), configData::getInstance().get_server_version());
		add_RTSP_timestamp(reply);
		strcat(reply, "Session: ");
		sprintf(temp, "%s", session->session_id.c_str());
		strcat(reply, temp);
		strcat(reply, RTSP_EL RTSP_EL);
		iRet = RTSP_add_out_buffer(reply, strlen(reply), rtsp);
		if (iRet != 0)
		{
			break;
		}
	} while (0);
	return	iRet;
}
