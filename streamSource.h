#ifndef _STREAM_SOURCE_H_
#define _STREAM_SOURCE_H_
#include "task_executor.h"
#include "task_schedule_obj.h"
#include "realtimeSinker.h"

#define NAL_TYPE_SINGLE_NAL_MIN	1
#define NAL_TYPE_SINGLE_NAL_MAX 23
#define NAL_TYPE_STAP_A 24
#define NAL_TYPE_STAP_B 25
#define NAL_TYPE_MTAP16	26
#define NAL_TYPE_MTAP24	27
#define NAL_TYPE_FU_A	28
#define NAL_TYPE_FU_B	29

class streamSource:public taskExecutor
{
public:
	static streamSource *createNewFileStreamSource(taskScheduleObj *task_obj);
	static streamSinker *createNewRealtimeSource(taskScheduleObj *task_obj);
	virtual ~streamSource();
	virtual int startStream(taskScheduleObj *task_obj)=0;
	virtual int stopStream(taskScheduleObj *task_obj)=0;
	virtual int handTask(taskScheduleObj *task_obj)=0;
protected:
	streamSource();
	virtual int notify_stoped()=0;
private:
	streamSource(streamSource&);
};

#endif
