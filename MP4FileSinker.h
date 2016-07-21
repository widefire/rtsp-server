#pragma once

#include "realtimeSinker.h"
#include "mp4FileStream.h"

class MP4FileSinker :
	public streamSinker
{
public:
	MP4FileSinker(taskJsonStruct &stTask, int jsonSocket);
	virtual ~MP4FileSinker();
	virtual	taskJsonStruct& getTaskJson();
	virtual void addDataPacket(DataPacket &dataPacket);
	virtual void addTimedDataPacket(timedPacket &dataPacket);
	virtual void sourceRemoved();//被通知源已经结束
	virtual int handTask(taskScheduleObj *task_obj);
	bool inited();
private:
	void initMP4Streamer();
	void notifyStartSave();
	void notifyStopSave();
	mp4FileStream *m_mp4Stream;
	taskJsonStruct	m_taskJson;
	std::string m_fullFileName;
	bool m_inited;
	int m_jsonSocket;
	unsigned int m_firstPktTime;
};

