#include "task_json.h"

task_json::task_json(std::string strJson,int sockId):m_taskType(schedule_start_json_task_async),m_strJson(strJson),m_sockId(sockId)
{
}

task_json::task_json(std::string taskId):m_taskType(schedule_stop_json_task),m_strJson(taskId)
{
}

task_schedule_type task_json::getType()
{
	return m_taskType;
}

task_json::~task_json()
{
}

std::string task_json::getJsonString()
{
	return m_strJson;
}

int task_json::getSockId()
{
	return m_sockId;
}


taskJsonSendNotify::taskJsonSendNotify(char * data, int size, int sockId):m_data(data),m_size(size),m_sockId(sockId)
{
}

taskJsonSendNotify::~taskJsonSendNotify()
{
	if (m_data)
	{
		delete[]m_data;
		m_data = nullptr;
	}
}

task_schedule_type taskJsonSendNotify::getType()
{
	return task_schedule_type::schedele_json_send_notify;
}

int taskJsonSendNotify::getSockId()
{
	return m_sockId;
}

char * taskJsonSendNotify::getData()
{
	return m_data;
}

int taskJsonSendNotify::getSize()
{
	return m_size;
}

taskJsonClosed::taskJsonClosed(int sockId):m_sockId(sockId)
{
}

int taskJsonClosed::getSockId()
{
	return m_sockId;
}

task_schedule_type taskJsonClosed::getType()
{
	return task_schedule_type::schedule_json_closed;
}
