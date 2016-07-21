#include "taskRtspLivingExist.h"

taskRtspLivingSdp::taskRtspLivingSdp(std::string streamName):m_streamName(streamName),m_sdpSeted(false)
{
}

std::string taskRtspLivingSdp::getStreamName()
{
	return m_streamName;
}

task_schedule_type taskRtspLivingSdp::getType()
{
	return schedule_rtsp_living_sdp;
}

void taskRtspLivingSdp::setSdp(std::string strSdp)
{
	m_sdpSeted = true;
	m_sdp = strSdp;
}

bool taskRtspLivingSdp::getSdp(std::string &strSdp)
{
	strSdp = m_sdp;
	return m_sdpSeted;
}

taskRtspLivingSdp::~taskRtspLivingSdp()
{
}
