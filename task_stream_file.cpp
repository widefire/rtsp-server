#include "task_stream_file.h"

taskStreamFile::taskStreamFile(LP_RTSP_buffer rtsp_buffer,LP_RTSP_session rtsp_session,
	float startTime, float endTime)
	:m_rtsp_buffer(0),m_startTime(startTime),m_endTime(endTime)
{
	m_rtsp_buffer=deepcopy_RTSP_buffer(rtsp_buffer);
	m_rtsp_session = deepcopy_RTSP_session(rtsp_session);
}

taskStreamFile::~taskStreamFile()
{
	free_RTSP_buffer(m_rtsp_buffer);
	delete m_rtsp_session;
}

task_schedule_type taskStreamFile::getType()
{
	return	schedule_start_rtsp_file;
}

const LP_RTSP_buffer taskStreamFile::get_RTSP_buffer()
{
	return	m_rtsp_buffer;
}

const LP_RTSP_session taskStreamFile::get_RTSP_session()
{
	return m_rtsp_session;
}

float taskStreamFile::getStartTime()
{
	return m_startTime;
}

float taskStreamFile::genEndTime()
{
	return m_endTime;
}
