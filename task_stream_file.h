#ifndef _TASK_STREAM_FILE_H_
#define _TASK_STREAM_FILE_H_
#include "task_schedule_obj.h"
#include "RTSP.h"

class taskStreamFile:public taskScheduleObj
{
public:
	taskStreamFile(LP_RTSP_buffer rtsp_buffer,LP_RTSP_session rtsp_session,float startTime=0.0f,float endTime=0.0f);
	~taskStreamFile();
	virtual task_schedule_type getType();
	const LP_RTSP_buffer get_RTSP_buffer();
	const LP_RTSP_session get_RTSP_session();
	float getStartTime();
	float genEndTime();
private:
	taskStreamFile(taskStreamFile&);
	LP_RTSP_buffer	m_rtsp_buffer;
	LP_RTSP_session m_rtsp_session;
	float m_startTime;
	float m_endTime;
};
#endif
