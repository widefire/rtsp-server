#pragma once
#include "task_schedule_obj.h"

#include "RTSP.h"

class taskStreamRtspLiving :
	public taskScheduleObj
{
public:
	taskStreamRtspLiving(LP_RTSP_buffer rtsp_buffer, LP_RTSP_session rtsp_session);
	virtual ~taskStreamRtspLiving();
	virtual task_schedule_type getType();
	const LP_RTSP_buffer get_RTSP_buffer();
	const LP_RTSP_session get_RTSP_session();
private:
	LP_RTSP_buffer	m_rtsp_buffer;
	LP_RTSP_session m_rtsp_session;
};

