#include "task_stream_stop.h"

taskStreamStop::taskStreamStop(int socket_id):m_task_type(schedule_stop_rtsp_by_socket),m_socket_id(socket_id)
{

}

taskStreamStop::taskStreamStop(std::string session_name):m_task_type(schedule_stop_rtsp_by_name),m_socket_id(0),
m_session_id(session_name)
{
}

taskStreamStop::~taskStreamStop()
{
}

task_schedule_type taskStreamStop::getType()
{
	return	m_task_type;
}

int taskStreamStop::getSocketId()
{
	return	m_socket_id;
}

std::string taskStreamStop::getSessionName()
{
	return m_session_id;
}

taskSinkerStop::taskSinkerStop(std::string fileName):m_fileName(fileName)
{
}

taskSinkerStop::~taskSinkerStop()
{
}

task_schedule_type taskSinkerStop::getType()
{
	return task_schedule_type::schedule_stop_rtsp_file_sinker;
}


std::string taskSinkerStop::getFileName()
{
	return m_fileName;
}

taskStreamPause::taskStreamPause(std::string session_name):m_sessionId(session_name)
{
}

taskStreamPause::~taskStreamPause()
{
}

task_schedule_type taskStreamPause::getType()
{
	return task_schedule_type::schedule_pause_rtsp_file;
}

std::string taskStreamPause::getSessionName()
{
	return m_sessionId;
}
