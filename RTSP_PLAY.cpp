#include "RTSP_server.h"
#include "configData.h"
#include "taskSchedule.h"
#include "task_stream_file.h"
#include "taskRtspLivingExist.h"
#include "taskStreamRtspLiving.h"

int RTSP_PLAY(LP_RTSP_buffer rtsp,LP_RTSP_session session)
{
	int iRet=0;
	char *p=0,*q=0;;
	char trash[255],url[255],server[255],object[255];
	float start_time=0.0f;
	float end_time=0.0f;
	int ss_result=0;
	char session_id[64]; //long session_id = 0;
	unsigned short port=0;
	do
	{
		//播放范围;
		if (0!=(p=strstr(rtsp->in_buffer,HDR_RANGE)))
		{
			//npt
			do
			{
				q=strstr(p,"npt");
				if (q!=0)
				{
					if ((q=strstr(q,"="))==0)
					{
						RTSP_send_err_reply(400,0,rtsp);
						iRet=-1;
						break;
					}
					ss_result=sscanf(q+1,"%f",&start_time);
					if (ss_result!=1)
					{
						RTSP_send_err_reply(400,0,rtsp);
						iRet=-1;
						break;
					}
					if ((q=strstr(q,"-"))==0)
					{
						RTSP_send_err_reply(400,0,rtsp);
						iRet=-1;
						break;
					}
					ss_result=sscanf(q+1,"%f",&end_time);
					if (ss_result!=1)
					{
						end_time=0;
					}
					break;
				}
				//smpte
				q=strstr(p,"smpte");
				if (q!=0)
				{
					start_time=0.0f;
					end_time=0.0f;
					break;
				}
				//clock
				q=strstr(p,"clock");
				if (q!=0)
				{
					start_time=0.0f;
					end_time=0.0f;
					break;
				}
				//time
				q=strstr(p,"time");
				if (q!=0)
				{
					start_time=0.0f;
					end_time=0.0f;
					break;
				}
				//other
				if (q==0)
				{
					double t;
					q = strstr(p, ":");
					//Hour
					sscanf(q + 1, "%lf", &t);
					start_time = t * 60 * 60;
					//Min
					q = strstr(q + 1, ":");
					sscanf(q + 1, "%lf", &t);
					start_time += (t * 60);
					//Sec
					q = strstr(q + 1, ":");
					sscanf(q + 1, "%lf", &t);
					start_time += t;
				}
			} while (0);
		}else
		{
			start_time=0.0f;
			end_time=0.0f;
		}

		if (iRet!=0)
		{
			break;
		}
		//是否有session;
		if ((p = strstr(rtsp->in_buffer, HDR_SESSION)) != NULL)
		{
			if (sscanf(p, "%254s %s", trash, &session_id) != 2)
			{
				RTSP_send_err_reply(454, 0, rtsp);
				iRet=-1;
				break;
			}
		}
		else
		{
			RTSP_send_err_reply(400,0,rtsp);
			iRet=-1;
			break;
		}
		//url
		ss_result=sscanf(rtsp->in_buffer," %*s %254s ",url);
		if (ss_result!=1)
		{
			RTSP_send_err_reply(400,0,rtsp);
			iRet=-1;
			break;
		}
		int parse_result=parse_url(url,server,sizeof server,port,object,sizeof object);
		if (parse_result==1)
		{
			RTSP_send_err_reply(400,0,rtsp);
			iRet=-1;
			break;
		}else if (parse_result==-1)
		{
			RTSP_send_err_reply(500,0,rtsp);
			iRet=-1;
			break;
		}
		//前缀为live 没有.则为实时流

		LOG_INFO(configData::getInstance().get_loggerId(), "play rtsp:" << object<<"sock id:"<<rtsp->sock_id);

		if (strlen(object) > strlen(RTSP_LIVE) && strncmp(object, RTSP_LIVE, strlen(RTSP_LIVE)) == 0)
		{
			//提取流名
			char streamName[255];
			memset(streamName, 0, sizeof(streamName));
			ss_result = sscanf(object, RTSP_LIVE "/%[^/]", streamName);
			if (ss_result!=1)
			{
				RTSP_send_err_reply(403, 0, rtsp);
				iRet = -1;
				break;
			}

			//判断流是否存在
			taskRtspLivingExist *livingExist = nullptr;
			do
			{

				livingExist = new taskRtspLivingExist(streamName);
				if (!livingExist)
				{
					RTSP_send_err_reply(404, 0, rtsp);
					iRet = -1;
					break;
				}
				taskSchedule::getInstance().executeTaskObj(livingExist);
				if (!livingExist->getLivingExist())
				{
					RTSP_send_err_reply(404, 0, rtsp);
					iRet = -1;
					break;
				}
				//发送play任务
				if (iRet==0)
				{
					//处理文件名中有多个 . 的情况
					//!处理文件名中有多个 . 的情况
					
					//处理流转发类
					taskScheduleObj *taskObj = 0;

					strcpy(rtsp->source_name, streamName);
					taskObj = new taskStreamRtspLiving(rtsp, session);
					taskSchedule::getInstance().addTaskObj(taskObj);
					LOG_INFO(configData::getInstance().get_loggerId(), "add play task:" << rtsp->source_name);
				}
				else
				{

					RTSP_send_err_reply(404, 0, rtsp);
				}
			} while (0);
			if (livingExist)
			{
				delete livingExist;
				livingExist = 0;
			}
			if (iRet!=0)
			{
				break;
			}
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
			char prefix[255], stuffix[255];
			ss_result = sscanf(object, "%[^.]%[^/]", prefix, stuffix);
			if (ss_result != 2)
			{
				RTSP_send_err_reply(400, 0, rtsp);
				iRet = -1;
				break;
			}
			//加入事件队列，开始直播;
			//not done;
			//PLAY 应该对应session,而不是对应rtsp;
			bool file_stream = true;
			taskScheduleObj *taskObj = 0;
			strcpy(rtsp->source_name, object);
			if (file_stream)
			{
				taskObj = new taskStreamFile(rtsp, session,start_time,end_time);
			}
			else
			{

			}
			taskSchedule::getInstance().addTaskObj(taskObj);
			LOG_INFO(configData::getInstance().get_loggerId(), "add play task:" << rtsp->source_name);
		}
		//发送Play响应;
		int obj_len=strlen(object);
		if (object[obj_len-1]=='/')
		{
			object[obj_len-1]='\0';
		}
		iRet=RTSP_PLAY_reply(rtsp,object,session);
	} while (0);

	return iRet;
}

int RTSP_PLAY_reply(LP_RTSP_buffer rtsp,char *object,LP_RTSP_session session)
{
	int iRet=0;

	/*
	RTSP/1.0 200 OK
	CSeq: 2
	Session: 12345678
	Range: smpte=0:10:00-0:20:00
	RTP-Info: url=rtsp://audio.example.com/twister/audio.en;
	seq=876655;rtptime=1032181
	*/
	char reply[RTSP_BUFFER_SIZE];
	char tmp[64];
	do
	{
		sprintf(reply,"%s %d %s" RTSP_EL"CSeq: %d" RTSP_EL"Server: %s%s" RTSP_EL,
			RTSP_VER,200,get_stat(200),rtsp->rtsp_seq,
			configData::getInstance().get_server_name(),configData::getInstance().get_server_version()
			);
		add_RTSP_timestamp(reply);
		strcat(reply,"Session: ");
		sprintf(tmp,"%s\r\n",session->session_id.c_str());
		strcat(reply,tmp);
		bool	need_cmma=false;
		std::list<LP_RTSP_session>::iterator it;
		for(it=rtsp->session_list.begin();it!=rtsp->session_list.end();it++)
		{
			if ((*it)->cur_state==READY_STAT)
			{
				if (need_cmma)
				{
					strcat(reply,",");
				}
				strcat(reply,"RTP-info: ");
				strcat(reply,"url=");
				sprintf(reply+strlen(reply),"rtsp://%s/%s/%s;",get_server_ip(),object,(*it)->control_id);
				sprintf(reply+strlen(reply),"seq=%u;rtptime=%u", (*it)->first_seq, (*it)->start_rtp_time);
				need_cmma=true;
			}
		}
		strcat(reply,RTSP_EL);
		strcat(reply,RTSP_EL);
		iRet=RTSP_add_out_buffer(reply,strlen(reply),rtsp);
		if (iRet!=0)
		{
			break;
		}
	} while (0);

	return iRet;
}
