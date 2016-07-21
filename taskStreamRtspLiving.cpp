#include "taskStreamRtspLiving.h"


taskStreamRtspLiving::taskStreamRtspLiving(LP_RTSP_buffer rtsp_buffer, LP_RTSP_session rtsp_session)
{
	m_rtsp_buffer = deepcopy_RTSP_buffer(rtsp_buffer);
	m_rtsp_session = deepcopy_RTSP_session(rtsp_session);
}

taskStreamRtspLiving::~taskStreamRtspLiving()
{
	free_RTSP_buffer(m_rtsp_buffer);
	delete m_rtsp_session;
}

task_schedule_type taskStreamRtspLiving::getType()
{
	return schedule_start_rtsp_living;
}

const LP_RTSP_buffer taskStreamRtspLiving::get_RTSP_buffer()
{
	return m_rtsp_buffer;
}

const LP_RTSP_session taskStreamRtspLiving::get_RTSP_session()
{
	return m_rtsp_session;
}
