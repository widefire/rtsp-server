#include "taskManagerSinker.h"

taskManagerSinker::taskManagerSinker(taskExecutor * const sinker, bool add):
	m_sinker(sinker),m_bAdd(add)
{
}

task_schedule_type taskManagerSinker::getType()
{
	return task_schedule_type::schedule_manager_sinker;
}

taskExecutor * const taskManagerSinker::getSinker()
{
	return m_sinker;
}

bool taskManagerSinker::bAdd()
{
	return m_bAdd;
}

taskManagerSinker::~taskManagerSinker()
{
}
