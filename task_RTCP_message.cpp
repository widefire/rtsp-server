#include "task_RTCP_message.h"


taskRtcpMessage::taskRtcpMessage(const RTCP_process & RTCP_message):m_RTCP_process(0)
{
	m_RTCP_process = new	RTCP_process(const_cast<RTCP_process&>(RTCP_message));
}

RTCP_process * taskRtcpMessage::get_RTCP_message()
{
	return m_RTCP_process;
}

taskRtcpMessage::~taskRtcpMessage()
{
	if (m_RTCP_process)
	{
		delete m_RTCP_process;
		m_RTCP_process = 0;
	}
}

task_schedule_type taskRtcpMessage::getType()
{
	return schedule_rtcp_data;
}
