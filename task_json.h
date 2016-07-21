#pragma once
#include "task_schedule_obj.h"
#include <string>
#include <stdio.h>
#include <stdlib.h>
class task_json :
	public taskScheduleObj
{
public:
	task_json(std::string strJson,int sockId);
	task_json(std::string taskId);
	virtual task_schedule_type getType();
	virtual ~task_json();
	std::string getJsonString();
	int getSockId();
private:
	task_json(const task_json&) = delete;
	task_json operator =(const task_json&) = delete;
	task_schedule_type	m_taskType;
	std::string m_strJson;
	int m_sockId;
};

class taskJsonSendNotify :public taskScheduleObj
{
public:
	taskJsonSendNotify(char *data,int size,int sockId);
	virtual ~taskJsonSendNotify();
	virtual task_schedule_type getType();
	int getSockId();
	char *getData();
	int getSize();
private:
	char *m_data;
	int m_size;
	int m_sockId;
};

class taskJsonClosed :public taskScheduleObj
{
public:
	taskJsonClosed(int sockId);
	int getSockId();
	virtual task_schedule_type getType();
private:
	int m_sockId;
};
