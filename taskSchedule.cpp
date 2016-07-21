#include "task_stream_file.h"
#include "taskSchedule.h"
#include "streamSource.h"
#include "util.h"
#include "task_json.h"
#include "rtspClientSource.h"
#include "taskRtspLivingExist.h"
#include "taskSendRtspResbData.h"
#include "taskStreamRtspLiving.h"
#include "h264RtspLiveSinkerSource.h"
#include "taskManagerSinker.h"
#include "configData.h"
#include <chrono>
#include <thread> 
taskSchedule::taskSchedule():m_shutdownLiving(false),m_shutdownJson(false)
{
	taskScheduleInit();
}

taskSchedule &taskSchedule::getInstance()
{
	static taskSchedule data;
	return data;
}


taskSchedule::~taskSchedule()
{
	taskScheduleShutdown();
}

void taskSchedule::addTaskObjPri(taskScheduleObj *task_obj)
{
	if (!task_obj)
	{
		return;
	}
	if (task_obj->getType()>= schedule_start_json_task_async)
	{
		std::lock_guard<std::mutex> guard(m_mutexJsonObjList);
		m_taskJsonScheduleobjList.push_back(task_obj);
		m_scheduleJsonCond.notify_all();
	}
	else
	{
		std::lock_guard<std::mutex> guard(m_mutexLivingObjList);
		m_taskLivingScheduleObjList.push_back(task_obj);
		//m_scheduleLivingCond.notify_one();
		m_scheduleLivingCond.notify_all();
	}
}

void taskSchedule::addTaskObj(taskScheduleObj * taskObj)
{
	std::thread addThread(std::mem_fun(&taskSchedule::addTaskObjPri),this, taskObj);
	addThread.detach();
}

void taskSchedule::executeTaskObj(taskScheduleObj * taskObjInOut)
{
	if (!taskObjInOut)
	{
		LOG_ERROR(configData::getInstance().get_loggerId(), "executeTaskObj get nullptr");
		return;
	}
	taskRtspLivingExist *rtspLivingExist = nullptr;
	taskRtspLivingSdp *rtspLivingSdp = nullptr;
	taskRtspLivingMediaType *rtspLivingMediaType = nullptr;
	std::map<std::string, taskExecutor*>::iterator iter;
	switch (taskObjInOut->getType())
	{
	case schedule_start_rtsp_file:
		break;
	case schedule_start_rtsp_living:
		break;
	case schedule_stop_rtsp_by_name:
	case schedule_stop_rtsp_by_socket:
		stopLivingStreamFile(taskObjInOut);
		break;
	case schedule_pause_rtsp_file:
		pauseLivingStreamFile(taskObjInOut);
		break;
	case schedule_rtp_data:
		break;
	case schedule_rtcp_data:
		break;
	case schedule_stop_json_task:
		break;
	case schedule_rtsp_living_exist:
		LOG_INFO(configData::getInstance().get_loggerId(), "json lock");
		m_mutexTaskJsonExecutor.lock();
		rtspLivingExist = (taskRtspLivingExist*)taskObjInOut;
		iter = m_taskJsonExecutorMap.find(rtspLivingExist->getStreamName());
		rtspLivingExist->setLivingExist(iter != m_taskJsonExecutorMap.end());
		LOG_INFO(configData::getInstance().get_loggerId(), "json unlock");
		m_mutexTaskJsonExecutor.unlock();
		break;
	case schedule_rtsp_living_sdp:
		LOG_INFO(configData::getInstance().get_loggerId(), "json lock");
		m_mutexTaskJsonExecutor.lock();
		rtspLivingSdp = (taskRtspLivingSdp*)taskObjInOut;
		iter = m_taskJsonExecutorMap.find(rtspLivingSdp->getStreamName());
		if (iter==m_taskJsonExecutorMap.end())
		{
			LOG_WARN(configData::getInstance().get_loggerId(),"the living stream not found:" << rtspLivingSdp->getStreamName());
		}
		else
		{
			rtspClientSource* rtspClientSourceInstance = (rtspClientSource*)iter->second;
			//rtspLivingSdp->setSdp(rtspClientSourceInstance->getSdp());
			rtspClientSourceInstance->handTask(taskObjInOut);
		}
		LOG_INFO(configData::getInstance().get_loggerId(), "json unlock");
		m_mutexTaskJsonExecutor.unlock();
		break;
	case schedule_rtsp_living_mediaType:
		LOG_INFO(configData::getInstance().get_loggerId(), "json lock");
		m_mutexTaskJsonExecutor.lock();
		rtspLivingMediaType = (taskRtspLivingMediaType*)taskObjInOut;
		iter = m_taskJsonExecutorMap.find(rtspLivingMediaType->getStreamName());
		if (iter == m_taskJsonExecutorMap.end())
		{
			LOG_WARN(configData::getInstance().get_loggerId(), "the living stream not found:" << rtspLivingMediaType->getStreamName());
		}
		else
		{
			rtspClientSource* rtspClientSourceInstance = (rtspClientSource*)iter->second;
			rtspClientSourceInstance->handTask(taskObjInOut);
			//rtspLivingSdp->setSdp(rtspClientSourceInstance->getSdp());
		}
		LOG_INFO(configData::getInstance().get_loggerId(), "json unlock");
		m_mutexTaskJsonExecutor.unlock();
		break;
	case schedule_start_json_task_async:
		break;
	case schedule_json_closed:
		closeJsonSocket(taskObjInOut);
		break;
	default:
		break;
	}
}

void taskSchedule::taskScheduleInit()
{

	int totalThread = std::thread::hardware_concurrency();
	if (totalThread == 0)
	{
		totalThread = 1;
	}
	//totalThread = 1;
	m_shutdownLiving = false;
	for (int  i = 0; i < totalThread; i++)
	{
		std::thread tmp_thread(taskLivingDistributionThread, this);
		m_threadLiving.push_back(std::move(tmp_thread));
	}

	m_shutdownJson = false;
	for (int  i = 0; i < totalThread; i++)
	{
		std::thread tmp_jsonThread(taskJsonDistributionThread, this);
		m_threadJson.push_back(std::move(tmp_jsonThread));
	}
	//m_threadJson.push_back
	//m_threadJson = std::move(tmp_jsonThread);
}

void taskSchedule::taskScheduleShutdown()
{
	m_shutdownLiving = true;
	m_shutdownJson = true;

	for (int  i = 0; i < m_threadLiving.size(); i++)
	{
		if (m_threadLiving.at(i).joinable())
		{
			m_threadLiving.at(i).join();
		}
	}

	for (int i = 0; i < m_threadJson.size(); i++)
	{
		if (m_threadJson.at(i).joinable())
		{
			m_threadJson.at(i).join();
		}
	}
	LOG_INFO(configData::getInstance().get_loggerId(), "LivingObj lock");
	std::lock_guard<std::mutex> guard(m_mutexLivingObjList);
	LOG_INFO(configData::getInstance().get_loggerId(), "jsonObj lock");
	std::lock_guard<std::mutex> guardJson(m_mutexJsonObjList);

	//清除Json任务
	std::lock_guard<std::mutex> guardMap(m_mutexLivingExecutorMap);
	for (auto i : m_taskJsonExecutorMap)
	{
		if (i.second)
		{
			delete i.second;
			i.second = nullptr;
		}
	}
	m_taskJsonExecutorMap.clear();
	LOG_INFO(configData::getInstance().get_loggerId(), "jsonObj unlock");
	LOG_INFO(configData::getInstance().get_loggerId(), "LivingObj unlock");
}

void taskSchedule::taskLivingDistributionThread(void *lparam)
{
	taskSchedule *caller=static_cast<taskSchedule*>(lparam);
	while (caller->m_shutdownLiving==false)
	{
		std::unique_lock<std::mutex> lk(caller->m_mutexLivingObjList);
		caller->m_scheduleLivingCond.wait(lk, [caller] {return caller->m_taskLivingScheduleObjList.size() != 0; });
		taskScheduleObj *Obj = nullptr;
		Obj = caller->m_taskLivingScheduleObjList.front();
		caller->m_taskLivingScheduleObjList.pop_front();
		lk.unlock();
		if (Obj)
		{
			caller->taskLivingHandler(Obj);
			delete Obj;
		}
	}
}

void taskSchedule::taskLivingHandler(taskScheduleObj *task_obj)
{
	if (!task_obj)
	{
		return;
	}
	do
	{
		task_schedule_type task_type=task_obj->getType();
		switch(task_type)
		{
		case schedule_start_rtsp_file:
			startLivingStreamFile(task_obj);
			break;
		case schedule_stop_rtsp_by_name:
		case schedule_stop_rtsp_by_socket:
		case schedule_stop_rtsp_file_sinker:
			stopLivingStreamFile(task_obj);
			break;
		case schedule_rtcp_data:
			passLivingRTCPMessage(task_obj);
			break;
		case schedule_stop_json_task:
			deleteRtspClientSource(task_obj);
			break;
		case schedule_start_rtsp_living:
			//如果成功，需将该直播作为槽，添加给对应的rtspClientSource
			startLivingRtspRealtimeStream(task_obj);
			break;
		case schedule_send_rtsp_data:
			//把数据添加给对应的直播者，让直播者找时间发送
			sendRtspData(task_obj);
			break;
		case schedele_json_send_notify:
			sendJsonNotify(task_obj);
			break;
		case schedule_json_closed:
			closeJsonSocket(task_obj);
		default:
			break;
		}
	} while (0);
}

void taskSchedule::startLivingStreamFile(taskScheduleObj *task_obj)
{
	if (!task_obj)
	{
		LOGW("nullptr task obj");
		return;
	}
	//开启直播文件任务;
	LOG_INFO(configData::getInstance().get_loggerId(), "startLivingStreamFile lock");
	m_mutexLivingExecutorMap.lock();
	for (auto i: m_taskLivingExecutorList)
	{
		streamSource *tmpSource = (streamSource*)i;
		if (0==tmpSource->handTask(task_obj))
		{
			LOG_INFO(configData::getInstance().get_loggerId(), "startLivingStreamFile unlock");
			m_mutexLivingExecutorMap.unlock();
			return;
		}
	}
	LOG_INFO(configData::getInstance().get_loggerId(), "startLivingStreamFile unlock");
	m_mutexLivingExecutorMap.unlock();
	streamSource *tmpSource=streamSource::createNewFileStreamSource(task_obj);
	if (!tmpSource)
	{
		return;
	}

	if (0 != tmpSource->handTask(task_obj))
	{
		delete tmpSource;
		tmpSource = nullptr;
		LOG_ERROR(configData::getInstance().get_loggerId(), "hand task stream file failed");
		return;
	}

	LOG_INFO(configData::getInstance().get_loggerId(), "startLivingStreamFile lock");
	std::lock_guard<std::mutex> guard(m_mutexLivingExecutorMap);
	m_taskLivingExecutorList.push_back(tmpSource);
	LOG_INFO(configData::getInstance().get_loggerId(), "startLivingStreamFile unlock");
}

void taskSchedule::stopLivingStreamFile(taskScheduleObj *task_obj)
{
	if (!task_obj)
	{
		LOGW("nullptr task obj");
		return;
	}
	//关闭对应的直播任务;
	bool	found_task=false;
	streamSource *tmpSource=0;
	std::list<taskExecutor*> listRemove;
	LOG_INFO(configData::getInstance().get_loggerId(), "stopLivingStreamFile lock");
	std::lock_guard<std::mutex> guard(m_mutexLivingExecutorMap);
	
	for(taskExecutor* it:m_taskLivingExecutorList)
	{
		tmpSource=static_cast<streamSource*>(it);
		if (tmpSource)
		{
			if (tmpSource->handTask(task_obj)==0)
			{
				listRemove.push_back((it));
			}
		}
	}
	for(taskExecutor* it:listRemove)
	{
		//尝试通知Json任务，这个槽没有了
		LOG_INFO(configData::getInstance().get_loggerId(), "json lock");
		m_mutexTaskJsonExecutor.lock();
		for (auto jsonTask : m_taskJsonExecutorMap)
		{
			rtspClientSource *source = (rtspClientSource*)jsonTask.second;
			taskManagerSinker managerSinker(it, false);
			LOG_INFO(configData::getInstance().get_loggerId(), "before handle task");
			source->handTask(&managerSinker);
			LOG_INFO(configData::getInstance().get_loggerId(), "after handle task");
			
		}
		LOG_INFO(configData::getInstance().get_loggerId(), "json unlock");
		m_mutexTaskJsonExecutor.unlock();
		delete(it);
		m_taskLivingExecutorList.remove((it));
	}
	LOG_INFO(configData::getInstance().get_loggerId(), "stopLivingStreamFile unlock");
}

void taskSchedule::pauseLivingStreamFile(taskScheduleObj * task_obj)
{
	if (!task_obj)
	{
		LOGW("nullptr task obj");
		return;
	}
	LOG_INFO(configData::getInstance().get_loggerId(), "pauseLivingStreamFile lock");
	std::lock_guard<std::mutex> guard(m_mutexLivingExecutorMap);
	streamSource *tmpSource = 0;
	for (taskExecutor* it : m_taskLivingExecutorList)
	{
		tmpSource = static_cast<streamSource*>(it);
		if (tmpSource)
		{
			if (tmpSource->handTask(task_obj) == 0)
			{
				break;
			}
		}
	}
	LOG_INFO(configData::getInstance().get_loggerId(), "pauseLivingStreamFile unlock");
}

void taskSchedule::passLivingRTCPMessage(taskScheduleObj * task_obj)
{
	if (!task_obj)
	{
		LOGW("null task obj");
		return;
	}
	streamSource *tmpSource = 0;
	for (taskExecutor* it:m_taskLivingExecutorList)
	{
		tmpSource = static_cast<streamSource*>(it);
		if (tmpSource)
		{
			if (tmpSource->handTask(task_obj))
			{
				break;
			}
		}
	}
}

void taskSchedule::deleteRtspClientSource(taskScheduleObj * task_obj)
{

	if (!task_obj)
	{
		LOGW("nullptr task obj");
		return;
	}

	task_json *realTask = static_cast<task_json*>(task_obj);
	deleteJsonTask(realTask->getJsonString());
}

void taskSchedule::startLivingRtspRealtimeStream(taskScheduleObj * task_obj)
{
	if (!task_obj)
	{
		LOG_ERROR(configData::getInstance().get_loggerId(),"nullptr task obj");
		return;
	}
	streamSinker *tmpSource = streamSource::createNewRealtimeSource(task_obj);
	if (!tmpSource)
	{
		return;
	}
	

	LOG_INFO(configData::getInstance().get_loggerId(), "startLivingRtspRealtimeStream lock");
	std::lock_guard<std::mutex> guard(m_mutexLivingExecutorMap);

	LOG_INFO(configData::getInstance().get_loggerId(), "startLivingRtspRealtimeStream json lock");
	std::lock_guard<std::mutex> guard2(m_mutexTaskJsonExecutor);
	//通知源，有槽了
	taskStreamRtspLiving *taskRtsp = static_cast<taskStreamRtspLiving*>(task_obj);
	
	auto iter=m_taskJsonExecutorMap.find(taskRtsp->get_RTSP_buffer()->source_name);
	if (iter==m_taskJsonExecutorMap.end())
	{
		LOG_ERROR(configData::getInstance().get_loggerId(), "no living source found");
		LOG_INFO(configData::getInstance().get_loggerId(), "startLivingRtspRealtimeStream unlock");
		LOG_INFO(configData::getInstance().get_loggerId(), "startLivingRtspRealtimeStream json unlock");
		delete tmpSource;
		tmpSource = nullptr;
		return;
	}
	taskManagerSinker taskManager(tmpSource, true);
	iter->second->handTask(&taskManager);
	m_taskLivingExecutorList.push_back(tmpSource);
	if (0 != tmpSource->handTask(task_obj))
	{
		LOG_ERROR(configData::getInstance().get_loggerId(), "hand task stream realtime failed");
		LOG_INFO(configData::getInstance().get_loggerId(), "startLivingRtspRealtimeStream unlock");
		LOG_INFO(configData::getInstance().get_loggerId(), "startLivingRtspRealtimeStream json unlock");
		return;
	}
	LOG_INFO(configData::getInstance().get_loggerId(), "startLivingRtspRealtimeStream unlock");
	LOG_INFO(configData::getInstance().get_loggerId(), "startLivingRtspRealtimeStream json unlock");
}

void taskSchedule::sendRtspData(taskScheduleObj * task_obj)
{
	taskSendRtspResbData *realTask = (taskSendRtspResbData*)task_obj;

	LOG_INFO(configData::getInstance().get_loggerId(), "sendRtspData lock");
	std::lock_guard<std::mutex> guard(m_mutexLivingExecutorMap);
	bool	found_task = false;
	streamSource *tmpSource = 0;
	for (taskExecutor* it : m_taskLivingExecutorList)
	{
		tmpSource = static_cast<streamSource*>(it);
		if (tmpSource)
		{
			if (tmpSource->handTask(task_obj) == 0)
			{
				break;
			}
		}
	}
	LOG_INFO(configData::getInstance().get_loggerId(), "sendRtspData unlock");
}

void taskSchedule::sendJsonNotify(taskScheduleObj * task_obj)
{
	taskJsonSendNotify *taskReal = static_cast<taskJsonSendNotify*>(task_obj);
	m_taskJsonSocketMapMutex.lock();
	auto it = m_taskJsonSocketMap.find(taskReal->getSockId());
	if (m_taskJsonSocketMap.end()!=it)
	{
		socketSend(taskReal->getSockId(), taskReal->getData(), taskReal->getSize());
	}
	m_taskJsonSocketMapMutex.unlock();
}

void taskSchedule::closeJsonSocket(taskScheduleObj * task_obj)
{
	taskJsonClosed *taskReal = static_cast<taskJsonClosed*>(task_obj);
	m_taskJsonSocketMapMutex.lock();
	auto it = m_taskJsonSocketMap.find(taskReal->getSockId());
	if (m_taskJsonSocketMap.end() != it)
	{
		m_taskJsonSocketMap.erase(it);
	}
	m_taskJsonSocketMapMutex.unlock();
}
	