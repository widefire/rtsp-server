#pragma once
#include "task_schedule_obj.h"
#include "RTSP.h"

class taskStreamStop:public taskScheduleObj
{
public:
	//通过对应套接字执行停止任务;
	taskStreamStop(int socket_id);
	//通过session ID停止执行任务;关闭某个流的所有转播和源任务
	taskStreamStop(std::string session_name);
	virtual ~taskStreamStop();
	virtual task_schedule_type getType();
	int		getSocketId();
	std::string getSessionName();
protected:
private:
	taskStreamStop(taskStreamStop&)=delete;
	task_schedule_type	m_task_type;
	std::string	m_session_id;
	int	m_socket_id;
};

class taskSinkerStop :public taskScheduleObj
{
public:
	taskSinkerStop(std::string fileName);
	virtual ~taskSinkerStop();
	virtual task_schedule_type getType();
	std::string getFileName();
private:
	taskSinkerStop(const taskSinkerStop&) = delete;
	std::string m_fileName;
};

class taskStreamPause :public taskScheduleObj
{
public:
	taskStreamPause(std::string session_name);
	virtual ~taskStreamPause();
	virtual task_schedule_type getType();
	std::string getSessionName();
private:
	std::string m_sessionId;
};