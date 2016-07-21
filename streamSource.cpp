#include "streamSource.h"
#include "H264VideoFileStreamSource.h"
#include "task_stream_file.h"
#include "taskStreamRtspLiving.h"
#include "taskRtspLivingExist.h"
#include "h264RtspLiveSinkerSource.h"
#include "FlvVideoFileStreamSource.h"
#include "taskSchedule.h"

streamSource::streamSource():taskExecutor()
{

}

streamSource::~streamSource()
{

}

streamSource *streamSource::createNewFileStreamSource(taskScheduleObj *task_obj)
{
	streamSource *outSource=0;
	do 
	{
		//根据后缀名决定创建哪个;
		taskStreamFile *task_real=static_cast<taskStreamFile*>(task_obj);
		if (!task_real)
		{
			break;
		}
		LP_RTSP_buffer rtsp_buffer=task_real->get_RTSP_buffer();
		if (!rtsp_buffer)
		{
			break;
		}
		if (strlen(rtsp_buffer->source_name)<=0)
		{
			break;
		}
		
		char file_name[256];
		/*ss_result=sscanf(rtsp_buffer->source_name,"%[^/]",file_name);
		if (ss_result!=1)
		{
			break;
		}*/
		memset(file_name, 0, sizeof(file_name));
		strcpy(file_name, rtsp_buffer->source_name);
		if (file_name[strlen(file_name)-1]=='/')
		{
			file_name[strlen(file_name) - 1] = '\0';
		}
		const char *pStuffix = nullptr;
		for (int i = strlen(file_name); i>1; i--)
		{
			if (file_name[i] == '.')
			{
				pStuffix = file_name + i + 1;
				break;
			}
		}
		if (pStuffix == nullptr)
		{

			break;
		}
		if (stricmp(pStuffix,"h264")==0||stricmp(pStuffix,"264")==0)
		{
			outSource=new H264VideoFileStreamSource(file_name, task_real->get_RTSP_session());
		}
		else if (stricmp(pStuffix,"flv")==0)
		{
			outSource = new FlvVideoFileStreamSource(file_name, task_real->get_RTSP_session());
		}
	} while (0);
	return outSource;
}

streamSinker * streamSource::createNewRealtimeSource(taskScheduleObj * task_obj)
{
	streamSinker *streamSource=nullptr;
	taskStreamRtspLiving *taskRtsp = nullptr;
	taskRtspLivingMediaType *mediaType = nullptr;
	taskJsonStruct taskJson;
	//memset(&taskJson, 0, sizeof(taskJson));
	do
	{
		switch (task_obj->getType())
		{
		case schedule_start_rtsp_file:
			break;
		case schedule_start_rtsp_living:
			taskRtsp = static_cast<taskStreamRtspLiving*>(task_obj);
			taskRtsp->get_RTSP_buffer()->source_name;
			//取得流类型，目前应该是H264
			mediaType = new taskRtspLivingMediaType(taskRtsp->get_RTSP_buffer()->source_name);

			taskSchedule::getInstance().executeTaskObj(mediaType);
			switch (mediaType->getMediaType())
			{
			case RTP_H264_Payload_type:
				streamSource = new h264RtspLiveSinker(taskRtsp->get_RTSP_buffer()->source_name,
					taskRtsp->get_RTSP_session(),taskJson);
				//创造H264转播源
				break;
			default:
				break;
			}
			break;
		case schedule_stop_rtsp_by_socket:
			break;
		case schedule_stop_rtsp_by_name:
			break;
		case schedule_rtp_data:
			break;
		case schedule_rtcp_data:
			break;
		case schedule_stop_json_task:
			break;
		case schedule_rtsp_living_exist:
			break;
		case schedule_rtsp_living_sdp:
			break;
		case schedule_send_rtsp_data:
			break;
		case schedule_start_json_task_async:
			break;
		default:
			break;
		}
	} while (0);
	if (mediaType)
	{
		delete mediaType;
		mediaType = nullptr;
	}
	return streamSource;
}
