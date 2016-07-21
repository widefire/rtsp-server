#include "flvFileSinker.h"
#include "task_stream_stop.h"
#include "task_json.h"
#include "taskSchedule.h"
#include "util.h"
#include "configData.h"

flvFileSinker::flvFileSinker(taskJsonStruct &stTask,int jsonSocket):streamSinker(stTask),m_taskJson(stTask),m_inited(false),
m_jsonSocket(jsonSocket),m_flvWriter(nullptr)
{
	initFlvSinker();
}


flvFileSinker::~flvFileSinker()
{

	if (m_flvWriter)
	{
		delete m_flvWriter;
		m_flvWriter = nullptr;
	}
	//发送存档结束
	notifyStopSave();
}

taskJsonStruct & flvFileSinker::getTaskJson()
{
	// TODO: 在此处插入 return 语句
	return m_taskJson;
}

void flvFileSinker::addDataPacket(DataPacket & dataPacket)
{
}

void flvFileSinker::addTimedDataPacket(timedPacket & dataPacket)
{
	if (!m_inited)
	{
		return;
	}
	m_flvWriter->addTimedPacket(dataPacket);

}

void flvFileSinker::sourceRemoved()
{
	//结束存档通知
	//发出任务，删除自身
	//taskSinkerStop
	taskSinkerStop *taskStop = new taskSinkerStop(m_taskJson.fileNameFlv);
	taskSchedule::getInstance().addTaskObj(taskStop);
}

int flvFileSinker::handTask(taskScheduleObj * task_obj)
{
	auto taskType = task_obj->getType();
	taskSinkerStop *taskStop = nullptr;
	switch (taskType)
	{
	case schedule_manager_sinker:
		break;
	case schedule_stop_rtsp_file_sinker:
		taskStop = static_cast<taskSinkerStop*>(task_obj);
		if (taskStop->getFileName()==m_taskJson.fileNameFlv)
		{
			return 0;
		}
		else
		{
			return -1;
		}
		break;
	default:
		break;
	}
	return -1;
}

bool flvFileSinker::inited()
{
	return m_inited;
} 

void flvFileSinker::initFlvSinker()
{
	m_inited = true;
	do
	{
		//判断目录
		createDirectory(configData::getInstance().getFlvSaveDir().c_str());
		m_taskJson.relativelyDir = getYmd2();
		std::string fullRelativeDir = configData::getInstance().getFlvSaveDir() + getYmd2();
		//一级一级的创建
		std::string strLayer = configData::getInstance().getFlvSaveDir();
		char year[10], month[10], day[10];
		sscanf(m_taskJson.relativelyDir.c_str(), "%[^/]/%[^/]/%[^/]", year, month, day);
		char tmpBuf[100];
		//年
		strLayer = strLayer + year;
		if (0 != createDirectory(strLayer.c_str()))
		{
			m_inited = false;
			break;
		}
		//月
		strLayer = strLayer + "/" + month;
		if (0 != createDirectory(strLayer.c_str()))
		{
			m_inited = false;
			break;
		}
		//日
		strLayer = strLayer + "/" + day;
		if (0 != createDirectory(strLayer.c_str()))
		{
			m_inited = false;
			break;
		}
		//总目录
		int ret = checkDirectoryExist(fullRelativeDir.c_str());
		if (0!=ret)
		{
			ret = createDirectory(fullRelativeDir.c_str());
			if (ret!=0)
			{
				LOG_ERROR(configData::getInstance().get_loggerId(), "create directory " << fullRelativeDir << " failed");
				m_inited = false;
				break;
			}
		}
		//创建文件源
		if (m_taskJson.fileNameFlv.size()==0)
		{
			m_taskJson.fileNameFlv = m_taskJson.taskId + ".flv";
		}
		else
		{
			//不管文件名是否为flv后缀
			//如果存在则失败
		}
		m_fullFileName = configData::getInstance().getFlvSaveDir() + m_taskJson.relativelyDir + "/" + m_taskJson.fileNameFlv;
		ret = access(m_fullFileName.c_str(), 0);
		if (ret==0)
		{
			LOG_ERROR(configData::getInstance().get_loggerId(), "file has existed:" << m_fullFileName);
			m_inited = false;
			break;
		}
		m_flvWriter = new flvWriter();
		m_inited = m_flvWriter->initFile(m_fullFileName);
		if (!m_inited)
		{
			LOG_ERROR(configData::getInstance().get_loggerId(), "file create failed:" << m_fullFileName);
			break;
		}
		//创建成功则设置初始化成功

		//发送存档开始
		notifyStartSave();
	} while (0);
}

void flvFileSinker::notifyStartSave()
{
	if (m_inited)
	{
		taskJsonStruct taskSave;
		taskSave.targetProtocol = taskJsonProtocol_RTSP;
		timeval timeBegin;
		gettimeofday(&timeBegin, 0);

		taskSave.taskId = m_taskJson.taskId;
		taskSave.relativelyDir = m_taskJson.relativelyDir;
		char addr[1024];
		sprintf(addr, "rtsp://%s:%d/%s/%s", get_server_ip(), configData::getInstance().RTSP_server_port(),
			m_taskJson.relativelyDir.c_str(),
			m_taskJson.fileNameFlv.c_str());
		taskSave.fileUrl = addr;
		m_taskJson.timeBegin = taskSave.timeBegin = timeBegin.tv_sec;
		taskSave.timeEnd = 0;
		taskSave.userdefString = m_taskJson.userdefString;
		taskSave.taskType = taskJson::getInstance().getOperatorType(em_operationType::opt_type_saveFile);
		taskSave.status = "start";
		taskSave.saveFlv = true;
		taskSave.saveMP4 = false;

		std::string taskString;
		if (taskJson::getInstance().json2String(taskSave, taskString))
		{
			unsigned int length = htonl(taskString.size());
			
			char *buffSend = new char[4 + taskString.size()];
			memcpy(buffSend, (const void*)&length, 4);
			memcpy(buffSend + 4, (const void*)taskString.c_str(), taskString.size());
			taskJsonSendNotify *taskNotify = new taskJsonSendNotify(buffSend, 4 + taskString.size(), m_jsonSocket);
			taskSchedule::getInstance().addTaskObj(taskNotify);
		}
		else
		{
			LOG_WARN(configData::getInstance().get_loggerId(), "generate live start json failed");
		}
	}
}

void flvFileSinker::notifyStopSave()
{
	if (m_inited)
	{
		taskJsonStruct taskSave;
		taskSave.targetProtocol = taskJsonProtocol_RTSP;
		timeval timeNow;
		gettimeofday(&timeNow, 0);

		taskSave.taskId = m_taskJson.taskId;
		taskSave.relativelyDir = m_taskJson.relativelyDir;
		char addr[1024];
		sprintf(addr, "rtsp://%s:%d/%s/%s", get_server_ip(), configData::getInstance().RTSP_server_port(),
			m_taskJson.relativelyDir.c_str(),
			m_taskJson.fileNameFlv.c_str());
		taskSave.fileUrl = addr;
		taskSave.timeBegin = m_taskJson.timeBegin;
		taskSave.timeEnd = timeNow.tv_sec;
		taskSave.userdefString = m_taskJson.userdefString;
		taskSave.taskType = taskJson::getInstance().getOperatorType(em_operationType::opt_type_saveFile);
		taskSave.status = "sucess";
		taskSave.saveFlv = true;
		taskSave.saveMP4 = false;

		std::string taskString;
		if (taskJson::getInstance().json2String(taskSave, taskString))
		{
			unsigned int length = htonl(taskString.size());

			char *buffSend = new char[4 + taskString.size()];
			memcpy(buffSend, (const void*)&length, 4);
			memcpy(buffSend + 4, (const void*)taskString.c_str(), taskString.size());
			taskJsonSendNotify *taskNotify = new taskJsonSendNotify(buffSend, 4 + taskString.size(), m_jsonSocket);
			taskSchedule::getInstance().addTaskObj(taskNotify);
		}
		else
		{
			LOG_WARN(configData::getInstance().get_loggerId(), "generate live start json failed");
		}
	}
}
