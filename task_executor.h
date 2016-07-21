#ifndef _TASK_EXECUTOR_H_
#define _TASK_EXECUTOR_H_

class taskScheduleObj;

class taskExecutor
{
public:
	taskExecutor();
	virtual ~taskExecutor();

	virtual int handTask(taskScheduleObj *task_obj) = 0;
protected:
private:
	taskExecutor(taskExecutor&);
};

#endif
