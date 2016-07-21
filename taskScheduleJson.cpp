#include "taskSchedule.h"
#include "task_json.h"
#include "taskJsonStructs.h"
#include "rtspClientSource.h"
#include "flvFileSinker.h"
#include "MP4FileSinker.h"
#include "FlvVideoFileStreamSource.h"
#include "taskManagerSinker.h"
#include "configData.h"

void taskSchedule::taskJsonDistributionThread(taskSchedule * caller)
{
	while (caller->m_shutdownJson==false)
	{
		std::unique_lock<std::mutex> lk(caller->m_mutexJsonObjList);
		caller->m_scheduleJsonCond.wait(lk, [caller] {return caller->m_taskJsonScheduleobjList.size() != 0; });
		taskScheduleObj *Obj = nullptr;
		Obj = caller->m_taskJsonScheduleobjList.front();
		caller->m_taskJsonScheduleobjList.pop_front();
		lk.unlock();
		if (Obj)
		{
			caller->taskJsonHandler(Obj);
			delete Obj;
		}
	}
}

void taskSchedule::taskJsonHandler(taskScheduleObj * taskObj)
{
	//Json任务处理
	//无效任务
	if (!taskObj||schedule_start_json_task_async!=taskObj->getType())
	{
		return;
	}
	//加入socket map
	m_taskJsonSocketMapMutex.lock();
	auto it = m_taskJsonSocketMap.find(((task_json*)taskObj)->getSockId());
	if (it==m_taskJsonSocketMap.end())
	{
		std::mutex *tmpMutex = new std::mutex;
		m_taskJsonSocketMap.insert(std::pair<int, std::mutex*>(((task_json*)taskObj)->getSockId(), tmpMutex));
	}
	m_taskJsonSocketMapMutex.unlock();
	//解析任务Json字符串
	parseJsonTask(taskObj);
}

void taskSchedule::parseJsonTask(taskScheduleObj * taskObj)
{
	task_json *realTask = static_cast<task_json*>(taskObj);
	std::string strTask = realTask->getJsonString();
	int sockId = realTask->getSockId();
	bool result;
	//将字符串转换为Json结构体
	taskJsonStruct stTask;
	result = taskJson::getInstance().string2Json(strTask, stTask);
	if (!result)
	{
		LOG_WARN(configData::getInstance().get_loggerId(), "parseJsonTask failed:" << strTask);
		return;
	}
	//所有任务都不会放在执行列表中，
	//仅createTask成功后放在执行列表中,以ID为Key，用新的函数添加和移除，不是Json任务
	std::string status, taskId, userDefString;
	if (stTask.taskType==taskJson::getInstance().getOperatorType(em_operationType::opt_type_createTask))
	{

		//有可能两个线程创建同样的任务，导致出错，所以创任务需外部枷锁
		//S
		//创一个任务实例，加入Json任务列表，如果任务已经存在，发送创任务返回，可以直播后异步发起直播开始通知
		//任务结束时异步发出直播结束，存档通知（可能有，可能没有）
		if (stTask.targetProtocol== taskJsonProtocol_RTSP)
		{
			LOG_INFO(configData::getInstance().get_loggerId(), "createTask RTSP json lock");
			std::lock_guard<std::mutex> guardTaskJson(m_mutexTaskJsonExecutor);
			if (stTask.taskId.size()>0)
			{
				taskExecutor *taskOld = nullptr;
				auto findRet = m_taskJsonExecutorMap.find(taskId);
				if (findRet != m_taskJsonExecutorMap.end())
				{
					taskOld = findRet->second;
				}
				//如果不加锁，可能导致上行代码没有taskOld，下一行代码时已经有了taskOld了
				if (taskOld!=0)
				{
					//发送已存在
					status = taskJsonStatus_Existing;
					taskId = stTask.taskId;
					userDefString = stTask.userdefString;
					sendCreateTaskReturn(sockId, status, taskId, userDefString);
					LOG_INFO(configData::getInstance().get_loggerId(), "createTask RTSP json unlock");
					return;
				}
			}
			rtspClientSource *rtspClient = new rtspClientSource(stTask,sockId);
			if (false == rtspClient->taskValid())
			{
				//无效 直接删除
				delete rtspClient;
				rtspClient = nullptr;
				//发送失败
				status = taskJsonStatus_Failed;
				taskId = stTask.taskId;
				userDefString = stTask.userdefString;
				sendCreateTaskReturn(sockId, status, taskId, userDefString);
			}
			else
			{
				//addJsonTask(rtspClient->getTaskId(), rtspClient);
				m_taskJsonExecutorMap.insert(std::pair<std::string, taskExecutor*>(rtspClient->getTaskId(), rtspClient));
				//发送成功
				status = taskJsonStatus_Success;
				taskId = rtspClient->getTaskId();
				userDefString = stTask.userdefString;
				sendCreateTaskReturn(sockId, status, taskId, userDefString);
			}
			//添加存储任务，如果有的话
			if (stTask.saveFlv)
			{
				stTask.taskId = rtspClient->getTaskId();
				flvFileSinker *flvSinker = new flvFileSinker(stTask,sockId);
				if (!flvSinker->inited())
				{
					//发送存档失败
					sendSaveFileFailed(sockId, rtspClient->getTaskId(), flvSinker->getTaskJson().fileNameFlv,
						stTask.userdefString);
					delete flvSinker;
					flvSinker = nullptr;
				}
				else
				{

					//存档开始由sinker收到帧时自己发送
					//通知源有槽了
					taskManagerSinker taskManager(flvSinker, true);
					rtspClient->handTask(&taskManager);
					LOG_INFO(configData::getInstance().get_loggerId(), "push flv lock");
					m_mutexLivingExecutorMap.lock();
					m_taskLivingExecutorList.push_back(flvSinker);
					LOG_INFO(configData::getInstance().get_loggerId(), "push flv unlock");
					m_mutexLivingExecutorMap.unlock();
				}
			}
			if (stTask.saveMP4)
			{
				stTask.taskId = rtspClient->getTaskId();
				auto *mp4Sinker = new MP4FileSinker(stTask, sockId);
				if (!mp4Sinker->inited())
				{
					sendSaveFileFailed(sockId, rtspClient->getTaskId(), mp4Sinker->getTaskJson().fileNameMp4,
						stTask.userdefString);
					delete mp4Sinker;
					mp4Sinker = nullptr;
				}
				else
				{
					taskManagerSinker taskManager(mp4Sinker, true);
					rtspClient->handTask(&taskManager);
					LOG_INFO(configData::getInstance().get_loggerId(), "push mp4 lock");
					m_mutexLivingExecutorMap.lock();
					m_taskLivingExecutorList.push_back(mp4Sinker);
					LOG_INFO(configData::getInstance().get_loggerId(), "push mp4 unlock");
					m_mutexLivingExecutorMap.unlock();
				}
			}
			LOG_INFO(configData::getInstance().get_loggerId(), "createTask RTSP json unlock");
		}
		else if (stTask.targetProtocol==taskJsonProtocol_RTMP)
		{
			LOG_WARN(configData::getInstance().get_loggerId(), "not support " << taskJsonProtocol_RTMP);
		}
		else if (stTask.targetProtocol==taskJsonProtocol_HTTP)
		{
			LOG_WARN(configData::getInstance().get_loggerId(), "not support " << taskJsonProtocol_HTTP);
		}
	}
	else if (stTask.taskType == taskJson::getInstance().getOperatorType(em_operationType::opt_type_createTaskReturn))
	{
		//C
	}
	else if (stTask.taskType == taskJson::getInstance().getOperatorType(em_operationType::opt_type_deleteTask))
	{
		//S
		//删除任务，如果任务存在则直接，返回成功，失败则返回失败
		//然后异步结束任务,最后由任务自身异步发起结束通知
		if (stTask.taskId.size()==0)
		{
			deleteAllJsonTask();
			status = taskJsonStatus_Success;
			userDefString = stTask.userdefString;
			sendDeleteTaskReturn(sockId, status, taskId, userDefString);
		}
		else
		{
			if (deleteJsonTask(stTask.taskId))
			{
				status = taskJsonStatus_Success;
				taskId = stTask.taskId;
				userDefString = stTask.userdefString;
				sendDeleteTaskReturn(sockId, status, taskId, userDefString);
			}
			else
			{
				//发送不存在
				status = taskJsonStatus_Failed;
				taskId = stTask.taskId;
				userDefString = stTask.userdefString;
				sendDeleteTaskReturn(sockId, status, taskId, userDefString);
			}
		}
	}
	else if (stTask.taskType == taskJson::getInstance().getOperatorType(em_operationType::opt_type_deleteTaskReturn))
	{
		//C
	}
	else if (stTask.taskType == taskJson::getInstance().getOperatorType(em_operationType::opt_type_disconnect))
	{
		//C
	}
	else if (stTask.taskType == taskJson::getInstance().getOperatorType(em_operationType::opt_type_heartbeat))
	{
		//S
	}
	else if (stTask.taskType == taskJson::getInstance().getOperatorType(em_operationType::opt_type_saveFile))
	{
		//C
	}
	else if (stTask.taskType == taskJson::getInstance().getOperatorType(em_operationType::opt_type_livingEnd))
	{
		//C
	}
	else if (stTask.taskType == taskJson::getInstance().getOperatorType(em_operationType::opt_type_livingStart))
	{
		//C
	}
	else if (stTask.taskType == taskJson::getInstance().getOperatorType(em_operationType::opt_type_getLivingList))
	{
		//S C
		std::lock_guard<std::mutex> guard(m_mutexLivingExecutorMap);
		taskJsonStruct livingList;
		livingList.taskType = stTask.taskType;
		for (auto i:m_taskJsonExecutorMap)
		{
			streamSinker *pSinker = (streamSinker*)i.second;
			livingList.livingList.push_back(pSinker->getTaskJson());
		}
		std::string taskString;
		if (taskJson::getInstance().json2String(livingList, taskString))
		{
			unsigned int length = htonl(taskString.size());

			int size = 4 + taskString.size();
			char *data = new char[size];
			memcpy(data, (const void*)&length, 4);
			memcpy(data + 4, (const void*)taskString.c_str(), taskString.size());
			LOG_INFO(configData::getInstance().get_loggerId(), "livingList size:" << size);
			SendGetLivingList(sockId, data, size);
		}
		else
		{
			LOG_WARN(configData::getInstance().get_loggerId(), "generate status json failed");
		}
	}
	else
	{
		LOG_WARN(configData::getInstance().get_loggerId(), "invalid json" << strTask);
		return;
	}
}

void taskSchedule::SendGetLivingList(int sockId, char * data, int size)
{
	taskJsonSendNotify *taskNotify = new taskJsonSendNotify(data, size, sockId);
	taskSchedule::getInstance().addTaskObj(taskNotify);
}


void taskSchedule::sendCreateTaskReturn(int sockId, std::string & status, std::string & taskId, std::string & userDefString)
{
	taskJsonStruct taskStatus;
	taskStatus.taskType = taskJson::getInstance().getOperatorType(em_operationType::opt_type_createTaskReturn);
	taskStatus.status = status;
	taskStatus.taskId = taskId;
	taskStatus.userdefString = userDefString;
	std::string taskString;
	if (taskJson::getInstance().json2String(taskStatus, taskString))
	{
		unsigned int length = htonl(taskString.size());

		char *buffSend = new char[4 + taskString.size()];
		memcpy(buffSend, (const void*)&length, 4);
		memcpy(buffSend + 4, (const void*)taskString.c_str(), taskString.size());
		taskJsonSendNotify *taskNotify = new taskJsonSendNotify(buffSend, 4 + taskString.size(), sockId);
		taskSchedule::getInstance().addTaskObj(taskNotify);
	}
	else
	{
		LOG_WARN(configData::getInstance().get_loggerId(), "generate status json failed");
	}
}

void taskSchedule::sendDeleteTaskReturn(int sockId, std::string & status, std::string & taskId, std::string & userDefString)
{
	taskJsonStruct taskStatus;
	taskStatus.taskType = taskJson::getInstance().getOperatorType(em_operationType::opt_type_deleteTaskReturn);
	taskStatus.status = status;
	taskStatus.taskId = taskId;
	taskStatus.userdefString = userDefString;
	std::string taskString;
	if (taskJson::getInstance().json2String(taskStatus, taskString))
	{
		unsigned int length = htonl(taskString.size());


		char *buffSend = new char[4 + taskString.size()];
		memcpy(buffSend, (const void*)&length, 4);
		memcpy(buffSend + 4, (const void*)taskString.c_str(), taskString.size());
		taskJsonSendNotify *taskNotify = new taskJsonSendNotify(buffSend, 4 + taskString.size(), sockId);
		taskSchedule::getInstance().addTaskObj(taskNotify);
	}
	else
	{
		LOG_WARN(configData::getInstance().get_loggerId(), "generate status json failed");
	}
}

void taskSchedule::sendSaveFileFailed(int sockId, std::string taskId, std::string fileName, std::string & userDefString)
{
	taskJsonStruct taskStatus;
	taskStatus.taskType = taskJson::getInstance().getOperatorType(em_operationType::opt_type_saveFile);
	taskStatus.status = "failed";
	taskStatus.taskId = taskId;
	taskStatus.userdefString = userDefString;
	taskStatus.fileNameFlv = fileName;
	std::string taskString;
	if (taskJson::getInstance().json2String(taskStatus, taskString))
	{
		unsigned int length = htonl(taskString.size());

		char *buffSend = new char[4 + taskString.size()];
		memcpy(buffSend, (const void*)&length, 4);
		memcpy(buffSend + 4, (const void*)taskString.c_str(), taskString.size());
		taskJsonSendNotify *taskNotify = new taskJsonSendNotify(buffSend, 4 + taskString.size(), sockId);
		taskSchedule::getInstance().addTaskObj(taskNotify);
	}
	else
	{
		LOG_WARN(configData::getInstance().get_loggerId(), "generate status json failed");
	}
}


bool taskSchedule::addJsonTask(std::string taskId, taskExecutor * task)
{
	LOG_INFO(configData::getInstance().get_loggerId(), "addJsonTask json lock");
	std::lock_guard<std::mutex> guardMap(m_mutexTaskJsonExecutor);
	auto findRet = m_taskJsonExecutorMap.find(taskId);
	if (findRet!=m_taskJsonExecutorMap.end())
	{
		LOG_INFO(configData::getInstance().get_loggerId(), "addJsonTask json unlock");
		return false;
	}
	m_taskJsonExecutorMap.insert(std::pair<std::string, taskExecutor*>(taskId, task));
	LOG_INFO(configData::getInstance().get_loggerId(), "addJsonTask json unlock");
	return true;
}

bool taskSchedule::deleteJsonTask(std::string taskId)
{
	LOG_INFO(configData::getInstance().get_loggerId(), "deleteJsonTask json lock");
	std::lock_guard<std::mutex> guardMap(m_mutexTaskJsonExecutor);
	auto findRet = m_taskJsonExecutorMap.find(taskId);
	if (findRet == m_taskJsonExecutorMap.end())
	{
		LOG_INFO(configData::getInstance().get_loggerId(), "deleteJsonTask json unlock");
		return false;
	}
	if (findRet->second)
	{
		std::vector<taskExecutor*>	executorDelete;
		executorDelete.push_back(findRet->second);
		std::thread threadDelete(std::mem_fun(&taskSchedule::deleteJsonExecetor), this, executorDelete);
		if (threadDelete.joinable())
		{
			threadDelete.detach();
		}
	}
	m_taskJsonExecutorMap.erase(taskId);
	LOG_INFO(configData::getInstance().get_loggerId(), "deleteJsonTask json unlock");
	return true;
}

bool taskSchedule::deleteAllJsonTask()
{

	LOG_INFO(configData::getInstance().get_loggerId(), "deleteAllJsonTask json lock");
	std::lock_guard<std::mutex>	guardMap(m_mutexTaskJsonExecutor);
	std::vector<taskExecutor*>	executorDelete;
	for (auto i:m_taskJsonExecutorMap)
	{
		executorDelete.push_back(i.second);
	}
	m_taskJsonExecutorMap.clear();
	std::thread threadDelete(std::mem_fun(&taskSchedule::deleteJsonExecetor), this, executorDelete);
	if (threadDelete.joinable())
	{
		threadDelete.detach();
	}
	LOG_INFO(configData::getInstance().get_loggerId(), "deleteAllJsonTask json unlock");
	return true;
}

void taskSchedule::deleteJsonExecetor(std::vector<taskExecutor*> execetor)
{
	for (auto i : execetor) 
	{
		delete i;
		i = nullptr;
	}
	execetor.clear();
}

taskExecutor * taskSchedule::getJsonTask(std::string taskId)
{
	std::lock_guard<std::mutex> guardMap(m_mutexLivingExecutorMap);
	LOG_INFO(configData::getInstance().get_loggerId(), "getJsonTask lock");
	auto findRet = m_taskJsonExecutorMap.find(taskId);
	if (findRet == m_taskJsonExecutorMap.end())
	{
		return nullptr;
	}
	LOG_INFO(configData::getInstance().get_loggerId(), "getJsonTask unlock");
	return findRet->second;
}