#include "taskRtspLivingExist.h"



taskRtspLivingExist::taskRtspLivingExist(std::string streamName):m_streamName(streamName),m_livingExist(false)
{
}

task_schedule_type taskRtspLivingExist::getType()
{
	return schedule_rtsp_living_exist;
}

std::string taskRtspLivingExist::getStreamName()
{
	return m_streamName;
}

void taskRtspLivingExist::setLivingExist(bool exist)
{
	m_livingExist = exist;
}

bool taskRtspLivingExist::getLivingExist()
{
	return m_livingExist;
}


taskRtspLivingExist::~taskRtspLivingExist()
{
}
