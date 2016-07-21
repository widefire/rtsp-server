#include "RTSP_server.h"
#include "taskSchedule.h"
#include "taskRtspLivingExist.h"

int RTSP_DESCRIBE(LP_RTSP_buffer rtsp)
{
	int	iRet=0;
	int valid_url=0;
	char	object[255],server[255],trash[255];
	char	*p=0;
	unsigned short port;
	char	url[255];
	char	descr[RTSP_DESC_SIZE];
	int		sdp_fmt=-1;
	do 
	{
		//读取URL
		if (sscanf(rtsp->in_buffer,"%*s %254s ",url)!=1)
		{
			RTSP_send_err_reply(400,0,rtsp);
			iRet=-1;
			break;
		}
		//解析URL
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
		//判断URL是否有效
		//文件流、实时流;
		if (strstr(object,"../"))
		{
			RTSP_send_err_reply(403,0,rtsp);
			iRet=-1;
			break;
		}
		if (strstr(object,"./"))
		{
			RTSP_send_err_reply(403,0,rtsp);
			iRet=-1;
			break;
		}
		p=strrchr(object,'.');
		iRet=0;
		if (p==0)
		{
			LOG_INFO(configData::getInstance().get_loggerId(), "this is descr a realtime stream");
			/*printf("no file suffix,maybe a realtime stream;\n");
			RTSP_send_err_reply(415,0,rtsp);
			iRet=-1;
			break;*/
		}else
		{
			iRet=is_supported_file_stuffix(p);
			if (iRet!=0)
			{
				RTSP_send_err_reply(415,0,rtsp);
				break;
			}
		}
		if (strstr(rtsp->in_buffer,HDR_REQUIRE))
		{
			RTSP_send_err_reply(551,0,rtsp);
			iRet=-1;
			break;
		}
		//SDP need?
		if (strstr(rtsp->in_buffer,HDR_ACCEPT)!=0)
		{
			if (strstr(rtsp->in_buffer,"application/sdp")!=0)
			{
				sdp_fmt=0;
			}else
			{
				RTSP_send_err_reply(551,0,rtsp);
				iRet=-1;
				break;
			}
		}
		//cseq;
		if ((p=strstr(rtsp->in_buffer,HDR_CSEQ))==0)
		{
			RTSP_send_err_reply(400,0,rtsp);
			iRet=-1;
			break;
		}else
		{
			if (sscanf(p,"%254s %d",trash,&rtsp->rtsp_seq)!=2)
			{
				RTSP_send_err_reply(400,0,rtsp);
				iRet=-1;
				break;
			}
		}
		//判断该文件或实时流是否存在;
		//实时流
		if (strlen(object)>strlen(RTSP_LIVE)&&strncmp(object,RTSP_LIVE,strlen(RTSP_LIVE))==0)
		{
			char *streamName = nullptr;
			streamName = new char[strlen(object) - strlen(RTSP_LIVE) + 1];
			//提取流名称
			sscanf(object, RTSP_LIVE "/%s",streamName);
			taskRtspLivingExist *livingExist = nullptr;
			taskRtspLivingSdp	*livingSdp = nullptr;
			do
			{
				//判断流是否存在
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

				//生成SDP信息
				livingSdp = new taskRtspLivingSdp(streamName);
				taskSchedule::getInstance().executeTaskObj(livingSdp);
				std::string sdp;
				//写入out buffer，等待发送;
				if (livingSdp->getSdp(sdp))
				{
					iRet = RTSP_DESCRIBE_reply(rtsp, object, const_cast<char*>(sdp.c_str()));
					if (iRet != 0)
					{
						break;
					}
				}
				else
				{
					LOG_WARN(configData::getInstance().get_loggerId(), "sdp not found:" << streamName);
					RTSP_send_err_reply(404, 0, rtsp);
					iRet = -1;
					break;
				}
			} while (0);
			if (livingExist)
			{
				delete livingExist;
			}
			if (livingSdp)
			{
				delete livingSdp;
			}
			delete[]streamName;
		}
		else
		{
			//文件流
			std::string fullName = configData::getInstance().getFlvSaveDir() + object;
			int exisit = access(fullName.c_str(), 0);
			if (exisit != 0)
			{
				LOG_WARN(configData::getInstance().get_loggerId(), "file " << fullName << " not found");
				RTSP_send_err_reply(404, 0, rtsp);
				iRet = -1;
				break;
			}

			//生成SDP信息
			iRet = generate_SDP_description((char*)fullName.c_str(), descr);
			if (iRet != 0)
			{
				LOG_WARN(configData::getInstance().get_loggerId(), "file " << fullName << " sdp failed");
				RTSP_send_err_reply(404, 0, rtsp);
				break;
			}
			//写入out buffer，等待发送;
			iRet = RTSP_DESCRIBE_reply(rtsp, object, descr);
			if (iRet != 0)
			{
				break;
			}
		}
		
	} while (0);
	return iRet;
}

int RTSP_DESCRIBE_reply(LP_RTSP_buffer rtsp,char *object,char *descr)
{
	int iRet=0;
	do 
	{
		char reply[RTSP_DESC_SIZE];
		sprintf(reply,"%s %d %s" RTSP_EL"CSeq: %d" RTSP_EL,
			RTSP_VER,200,get_stat(200),rtsp->rtsp_seq);
		add_RTSP_timestamp(reply);
		strcat(reply,"Content-Type: application/sdp" RTSP_EL);
		char hostName[32];
		gethostname(hostName,sizeof hostName);
		char url[255];
		if (sscanf(rtsp->in_buffer,"%*s %254s ",url)!=1)
		{
			RTSP_send_err_reply(400,0,rtsp);
			iRet=-1;
			break;
		}

		sprintf(reply+strlen(reply),"Content-Base: rtsp://%s/%s/" RTSP_EL, hostName, object);
		sprintf(reply + strlen(reply), "Content-Length: %d" RTSP_EL, strlen(descr)+strlen(RTSP_EL));
		strcat(reply, RTSP_EL);
		strcat(reply, descr);
		strcat(reply, RTSP_EL);
		iRet=RTSP_add_out_buffer(reply,strlen(reply),rtsp);
		if (iRet!=0)
		{
			break;
		}
	} while (0);
	return iRet;
}

void add_RTSP_timestamp(char *p)
{
	tm *t;
	time_t now;
	now = time(0);
	t=gmtime(&now);
	strftime(p+strlen(p),38,"Date: %a, %d %b %Y %H:%M:%S GMT" RTSP_EL, t);

}