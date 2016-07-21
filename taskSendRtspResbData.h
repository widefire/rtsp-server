#pragma once
#include "task_schedule_obj.h"
#include "RTSP.h"

class taskSendRtspResbData :
	public taskScheduleObj
{
public:
	taskSendRtspResbData(std::string dataSend,std::string sessionId);
	virtual ~taskSendRtspResbData();
	std::string getDataSend();
	std::string getSessionId();
	virtual task_schedule_type getType();
private:
	std::string m_dataSend;
	std::string m_sessionId;
};

