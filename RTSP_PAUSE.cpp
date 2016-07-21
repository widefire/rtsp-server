#include "RTSP_server.h"
#include "configData.h"
#include "taskSchedule.h"
#include "task_stream_file.h"
#include "task_stream_stop.h"

int RTSP_PAUSE(LP_RTSP_buffer rtsp, LP_RTSP_session session)
{
	int iRet = 0;
	char url[255];
	char server[255];
	char object[255];
	unsigned short port;
	char *p = 0;
	int ssResult = 0;
	char trash[255];
	char ch_tmp[2][255];
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
		LOG_INFO(configData::getInstance().get_loggerId(), "pause rtsp:" << object);

		char file_name[255];

		if (strlen(object) > strlen(RTSP_LIVE) && strncmp(object, RTSP_LIVE, strlen(RTSP_LIVE)) == 0)
		{
			//实时流是否存在
			LOG_INFO(configData::getInstance().get_loggerId(), "pause live,not processed,live and file in the same list");
		}
		else
		{

			//文件流是否存在;
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
			/*ssResult = sscanf(object, "%[^/]", file_name);
			if (ssResult != 1)
			{
				RTSP_send_err_reply(400, 0, rtsp);
				iRet = -1;
				break;
			}*/
			strcpy(file_name, object);
			if (file_name[strlen(file_name) - 1] == '/')
			{
				file_name[strlen(file_name) - 1] = '\0';
			}
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
		//是否有session;
		if (session)
		{
			bool file_stream = true;
			taskScheduleObj *taskObj = 0;
			strcpy(rtsp->source_name, object);
			if (file_stream)
			{
				//taskObj = new taskStreamStop(session->session_id);
				taskObj = new taskStreamPause(session->session_id);

				taskSchedule::getInstance().executeTaskObj(taskObj);

				delete taskObj;
				LOG_INFO(configData::getInstance().get_loggerId(), "RTSP pause:" << file_name << " sock id:" << rtsp->sock_id);
			}
			else
			{

			}
			//taskSchedule::getInstance().addTaskObj(taskObj);
		}
		else
		{
			iRet = -1;
			break;
		}
		//rtsp pause reply;
		iRet = RTSP_PAUSE_reply(rtsp, session);
		if (iRet != 0)
		{
			break;
		}
	} while (0);
	return iRet;
}

int RTSP_PAUSE_reply(LP_RTSP_buffer rtsp, LP_RTSP_session session)
{
	int iRet = 0;

	do
	{
		char reply[RTSP_BUFFER_SIZE];
		char temp[255];
		sprintf(reply, "%s %d %s" RTSP_EL"CSeq: %d" RTSP_EL"Server: %s/%s" RTSP_EL,
			RTSP_VER, 200, get_stat(200), rtsp->rtsp_seq,
			configData::getInstance().get_server_name(), configData::getInstance().get_server_version());
		add_RTSP_timestamp(reply);
		int cur = strlen(reply);
		strcat(reply, "Session: ");
		sprintf(temp, "%s", session->session_id.c_str());
		strcat(reply, temp);
		strcat(reply, RTSP_EL);
		strcat(reply, RTSP_EL);
		iRet = RTSP_add_out_buffer(reply, strlen(reply), rtsp);
		if (iRet != 0)
		{
			break;
		}
	} while (0);

	return iRet;
}
