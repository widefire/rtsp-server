#include "util.h"
#include "configData.h"
#include "MP4FileSinker.h"
#include "task_stream_stop.h"
#include "taskSchedule.h"
#include "task_json.h"



MP4FileSinker::MP4FileSinker(taskJsonStruct & stTask, int jsonSocket):streamSinker(stTask),
m_inited(false),m_jsonSocket(jsonSocket),m_mp4Stream(nullptr),m_firstPktTime(-1),
m_taskJson(stTask)
{
	initMP4Streamer();
}

MP4FileSinker::~MP4FileSinker()
{
	if (m_mp4Stream)
	{
		delete m_mp4Stream; 
		m_mp4Stream = nullptr;
	}
	notifyStopSave();
}

taskJsonStruct & MP4FileSinker::getTaskJson()
{
	// TODO: 在此处插入 return 语句
	return m_taskJson;
}

void MP4FileSinker::addDataPacket(DataPacket & dataPacket)
{
}

void MP4FileSinker::addTimedDataPacket(timedPacket & dataPacket)
{
	if (!m_inited)
	{
		return;
	}
	if (m_firstPktTime==-1)
	{
		m_firstPktTime = dataPacket.timestamp;
	}

	dataPacket.timestamp -= m_firstPktTime;
	m_mp4Stream->addTimedDataPacket(dataPacket);
}

void MP4FileSinker::sourceRemoved()
{
	taskSinkerStop *taskStop = new taskSinkerStop(m_taskJson.fileNameMp4);
	taskSchedule::getInstance().addTaskObj(taskStop);
}

int MP4FileSinker::handTask(taskScheduleObj * task_obj)
{
	int iRet = 0;
	auto taskType = task_obj->getType();
	taskSinkerStop *taskStop = nullptr;
	switch (taskType)
	{
	case schedule_stop_rtsp_file_sinker:
		taskStop = static_cast<taskSinkerStop*>(task_obj);
		if (taskStop->getFileName()==m_taskJson.fileNameMp4)
		{
			iRet = 0;
		}
		else
		{
			iRet = -1;
		}
		break;
	default:
		iRet = -1;
		break;
	}
	return iRet;
}

bool MP4FileSinker::inited()
{
	return m_inited;
}

void MP4FileSinker::initMP4Streamer()
{
	m_inited = true;
	do
	{
		//目录
		createDirectory(configData::getInstance().getMP4SaveDir().c_str());
		m_taskJson.relativelyDir = getYmd2();
		std::string fullRelativeDir = configData::getInstance().getMP4SaveDir() + getYmd2();
		//一级一级的创建
		std::string strLayer = configData::getInstance().getMP4SaveDir();
		char year[10], month[10], day[10];
		sscanf(m_taskJson.relativelyDir.c_str(), "%[^/]/%[^/]/%[^/]", year, month, day);
		char tmpBuf[100];
		//年
		strLayer = strLayer + year;
		if (0!=createDirectory(strLayer.c_str()))
		{
			m_inited = false;
			break;
		}
		//月
		strLayer = strLayer + "/" + month;
		if (0!=createDirectory(strLayer.c_str()))
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
		if (0 != ret)
		{
			ret = createDirectory(fullRelativeDir.c_str());
			if (ret != 0)
			{
				LOG_ERROR(configData::getInstance().get_loggerId(), "create directory " << fullRelativeDir << " failed");
				m_inited = false;
				break;
			}
		}
		//文件名
		if (m_taskJson.fileNameMp4.size() == 0)
		{
			m_taskJson.fileNameMp4 = m_taskJson.taskId + ".mp4";
		}
		m_fullFileName = configData::getInstance().getMP4SaveDir() + m_taskJson.relativelyDir + "/" + m_taskJson.fileNameMp4;
		ret = access(m_fullFileName.c_str(), 0);
		if (ret == 0)
		{
			LOG_ERROR(configData::getInstance().get_loggerId(), "file has existed:" << m_fullFileName);
			m_inited = false;
			break;
		}
		//创建sink
		m_mp4Stream = new mp4FileStream(m_fullFileName);
		//发送开始通知
		notifyStartSave();

	} while (0);
}

void MP4FileSinker::notifyStartSave()
{
	if (m_inited)
	{
		//MP4只有存档完成通知
		timeval timeBegin;
		gettimeofday(&timeBegin, 0);
		m_taskJson.timeBegin = timeBegin.tv_sec;
	}
}

void MP4FileSinker::notifyStopSave()
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
		sprintf(addr, "http://%s:%d/%s/%s", get_server_ip(), configData::getInstance().httpFilePort(),
			m_taskJson.relativelyDir.c_str(),
			m_taskJson.fileNameMp4.c_str());
		taskSave.fileUrl = addr;
		taskSave.timeBegin = m_taskJson.timeBegin;
		taskSave.timeEnd = timeNow.tv_sec;
		taskSave.userdefString = m_taskJson.userdefString;
		taskSave.taskType = taskJson::getInstance().getOperatorType(em_operationType::opt_type_saveFile);
		taskSave.status = "sucess";
		taskSave.saveMP4 = true;
		taskSave.saveFlv = false;

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
