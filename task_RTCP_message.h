#pragma once

#include "task_schedule_obj.h"
#include "RTCP_Process.h"

class taskRtcpMessage:public taskScheduleObj
{
public:
	//传入已经解析好的RTCP SR信息个源;
	taskRtcpMessage(const RTCP_process &RTCP_message);
	RTCP_process *get_RTCP_message();
	virtual ~taskRtcpMessage();
	virtual task_schedule_type getType();
private:
	taskRtcpMessage(taskRtcpMessage&);
	RTCP_process	*m_RTCP_process;
};
