#pragma once
#include "realtimeSinker.h"
#include "flvWriter.h"

//文件槽，通过任务结束
//第一次写入关键帧后，发出存档通知
//如果有文件，析构时发出存档结束通知，和通知源槽没有了
class flvFileSinker :
	public streamSinker
{
public:
	flvFileSinker(taskJsonStruct &stTask,int jsonSocket);
	virtual ~flvFileSinker();
	virtual	taskJsonStruct& getTaskJson();
	virtual void addDataPacket(DataPacket &dataPacket);
	virtual void addTimedDataPacket(timedPacket &dataPacket);
	virtual void sourceRemoved();//被通知源已经结束
	virtual int handTask(taskScheduleObj *task_obj);
	bool inited();
private:
	void initFlvSinker();
	void notifyStartSave();
	void notifyStopSave();
	flvWriter *m_flvWriter;
	taskJsonStruct	m_taskJson;
	std::string m_fullFileName;
	bool m_inited;
	int m_jsonSocket;
	unsigned int m_firstPktTime;
};

