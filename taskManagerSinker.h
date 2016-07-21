#include "task_schedule_obj.h"
#include "task_executor.h"
#include <string>

class taskManagerSinker :public taskScheduleObj
{
public:
	taskManagerSinker(taskExecutor *const sinker, bool add);
	virtual task_schedule_type getType();
	taskExecutor *const getSinker();
	bool bAdd();
	~taskManagerSinker();
private:
	bool m_bAdd;
	taskExecutor *m_sinker;
};