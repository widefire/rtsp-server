#ifndef _TASK_SCHEDULE_H_
#define _TASK_SCHEDULE_H_

#include "task_schedule_obj.h"
#include "task_executor.h"

#include <list>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <map>

class taskSchedule
{
public:
	static taskSchedule &getInstance();
	taskSchedule();
	virtual ~taskSchedule();
	//添加任务到任务队列，异步
	void addTaskObj(taskScheduleObj *taskObj);
	//执行任务，同步返回结果
	void executeTaskObj(taskScheduleObj *taskObjInOut);
protected:
private:
	taskSchedule(taskSchedule&)=delete;
	void taskScheduleInit();
	void taskScheduleShutdown();
	void addTaskObjPri(taskScheduleObj *task_obj);
	//分为直播任务和Json相关任务
	//直播任务包括播文件和播直播流给客户端，耗时短，直接单线程
	//Json任务包括拉流 存文件等，可能耗时很长，需异步
	///直播任务
	//分发任务;
	static void taskLivingDistributionThread(void *lparam);
	//处理并删除任务;
	void taskLivingHandler(taskScheduleObj *task_obj);
	//任务处理;
	//开始一个文件直播;
	void startLivingStreamFile(taskScheduleObj *task_obj);
	//结束一个文件直播;
	void stopLivingStreamFile(taskScheduleObj *task_obj);
	//暂停一个文件直播
	void pauseLivingStreamFile(taskScheduleObj *task_obj);
	//转发处理RTCP信息;
	void passLivingRTCPMessage(taskScheduleObj *task_obj);
	//删除一个RTSP客户端
	void deleteRtspClientSource(taskScheduleObj *task_obj);
	//添加一个实时流直播
	void startLivingRtspRealtimeStream(taskScheduleObj *task_obj);
	//发送RTSP数据
	void sendRtspData(taskScheduleObj *task_obj);
	//发送json通知
	void sendJsonNotify(taskScheduleObj *task_obj);
	//关闭json套接字
	void closeJsonSocket(taskScheduleObj *task_obj);
	//任务列表;
	std::vector<std::thread>	m_threadLiving;
	volatile bool m_shutdownLiving;
	std::list<taskScheduleObj*>	m_taskLivingScheduleObjList;
	std::mutex	m_mutexLivingObjList;
	//正在执行的任务;
	std::list<taskExecutor*>		m_taskLivingExecutorList;
	std::mutex	m_mutexLivingExecutorMap;
	std::condition_variable m_scheduleLivingCond;
	//!直播任务
	///Json任务 需考虑线程安全
	//分发任务，取任务，拿给处理线程,可以同时创几个线程来分发任务，因为Json任务时独立的;
	static void taskJsonDistributionThread(taskSchedule *caller);
	//处理任务
	void taskJsonHandler(taskScheduleObj *taskObj);
	void parseJsonTask(taskScheduleObj * taskObj);
	void SendGetLivingList(int sockId,char *data,int size);
	void sendCreateTaskReturn(int sockId,std::string &status,std::string &taskId,std::string &userDefString);
	void sendDeleteTaskReturn(int sockId,std::string &status, std::string &taskId, std::string &userDefString);
	void sendSaveFileFailed(int sockId, std::string taskId, std::string fileName, std::string &userDefString);
	//待处任务列表
	volatile bool m_shutdownJson;
	std::vector<std::thread> m_threadJson;
	std::list<taskScheduleObj*> m_taskJsonScheduleobjList;
	std::mutex m_mutexJsonObjList;
	std::condition_variable m_scheduleJsonCond;
	//处理中的任务列表
	bool addJsonTask(std::string taskId, taskExecutor* task);
	bool deleteJsonTask(std::string taskId);
	bool deleteAllJsonTask();
	void deleteJsonExecetor(std::vector<taskExecutor*> execetor);
	taskExecutor* getJsonTask(std::string taskId);
	std::map<std::string, taskExecutor*> m_taskJsonExecutorMap;
	std::mutex m_mutexTaskJsonExecutor;
	std::map<int, std::mutex*> m_taskJsonSocketMap;
	std::mutex	m_taskJsonSocketMapMutex;
	//!json任务
};


#endif
