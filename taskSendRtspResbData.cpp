#include "taskSendRtspResbData.h"



taskSendRtspResbData::taskSendRtspResbData(std::string dataSend, std::string sessionId):m_dataSend(dataSend),
m_sessionId(sessionId)
{
}


taskSendRtspResbData::~taskSendRtspResbData()
{
}

std::string taskSendRtspResbData::getDataSend()
{
	return m_dataSend;
}

std::string taskSendRtspResbData::getSessionId()
{
	return m_sessionId;
}

task_schedule_type taskSendRtspResbData::getType()
{
	return schedule_send_rtsp_data;
}
