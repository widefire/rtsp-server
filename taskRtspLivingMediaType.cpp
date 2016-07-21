#include "taskRtspLivingExist.h"

taskRtspLivingMediaType::taskRtspLivingMediaType(std::string streamName):m_streamName(streamName),m_mediaType(0)
{
}

std::string taskRtspLivingMediaType::getStreamName()
{
	return m_streamName;
}

task_schedule_type taskRtspLivingMediaType::getType()
{
	return schedule_rtsp_living_mediaType;
}

void taskRtspLivingMediaType::setMediaType(int mediaType)
{
	m_mediaType = mediaType;
}

int taskRtspLivingMediaType::getMediaType()
{
	return m_mediaType;
}

taskRtspLivingMediaType::~taskRtspLivingMediaType()
{
}
