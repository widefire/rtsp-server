
#include "rtspClientSource.h"
#include "configData.h"
#include "base64.h"
#include "util.h"
#include "H264VideoFileStreamSource.h"
#include "task_json.h"
#include "taskSchedule.h"
#include "taskRtspLivingExist.h"
#include "taskManagerSinker.h"

#include <ctime>

rtspClientSource::rtspClientSource(taskJsonStruct & stTask, int jsonSocket) :streamSinker(stTask),
m_validTask(false), m_port(0), m_taskStruct(stTask),
m_createNewFile(false), m_rtspSocket(-1), m_rtpSocket(-1), m_rtcpSocket(-1), m_tcpCacher(0), m_cseq(1), m_end(false),
m_startTime(0), m_endTime(0), m_timeout(60), m_rtpPort(0), m_rtcpPort(0), m_rtpSvrPort(0), m_rtcpSvrPort(0),
m_rtpMulticastPort(0), m_rtcpMulticastPort(0), m_tcpProtocol(false), m_jsonSocket(jsonSocket), m_notifyStarted(false),
m_rtpReceptionStats(0),m_hasCalledDeleteSelf(false)
{
	memset(&m_rtpStats, 0, sizeof(m_rtpStats));
	memset(&m_rtcpHeader, 0, sizeof(m_rtcpHeader));
	memset(&m_rtpHeader, 0, sizeof(m_rtpHeader));
	m_ssrcRR = generate_random32();
	m_tcpCacher = new tcpCacher();
	m_validTask = parseTaskParam();
	if (m_validTask)
	{
		LOG_INFO(configData::getInstance().get_loggerId(), "start task:" << m_taskStruct.targetAddress);
		std::thread tmpThread(std::mem_fun(&rtspClientSource::rtspStreamStart), this);
		m_streamThread = std::move(tmpThread);

	}
	else
	{
		//通知任务调度者删除自己
		//未加入队列，不用删除
	}
}

std::string rtspClientSource::getTaskId()
{
	return m_taskStruct.taskId;
}

bool rtspClientSource::taskValid()
{
	return m_validTask;
}


rtspClientSource::~rtspClientSource()
{
	//
	m_hasCalledDeleteSelf = true;
	m_end = true;
	if (m_rtspSocket >= 0&&m_rtspSocket!=-1)
	{
		LOG_INFO(configData::getInstance().get_loggerId(), "socket id" << m_rtspSocket);
		closeSocket(m_rtspSocket); m_rtspSocket = -1;
	}

	if (m_rtpSocket >=0&&m_rtpSocket!=-1)
	{
		LOG_INFO(configData::getInstance().get_loggerId(), "socket id" << m_rtpSocket);
		closeSocket(m_rtpSocket); m_rtpSocket = -1;
	}
	if (m_rtcpSocket >=0&&m_rtcpSocket!=-1 )
	{
		LOG_INFO(configData::getInstance().get_loggerId(), "socket id" << m_rtcpSocket);
		closeSocket(m_rtcpSocket); m_rtcpSocket = -1;
	}
	

	if (m_streamThread.joinable())
	{
		m_streamCondition.notify_all();
		m_streamThread.join();
	}
	if (m_tcpCacher)
	{
		delete m_tcpCacher;
		m_tcpCacher = 0;
	}
	m_rtspDataMutex.lock();
	for (auto i : m_rtspDataList)
	{
		delete[]i.data;
	}
	m_rtspDataMutex.unlock();

	m_rtpDataMutex.lock();
	for (auto i : m_rtpDataList)
	{
		delete[]i.data;
	}
	m_rtpDataMutex.unlock();

	m_rtcpDataMutex.lock();
	for (auto i : m_rtcpDataList)
	{
		delete[]i.data;
	}
	m_rtcpDataMutex.unlock();
	for (auto i : m_rtpDataForProcesser)
	{
		delete[]i.data;
	}
	if (m_rtpReceptionStats)
	{
		delete m_rtpReceptionStats;
		m_rtpReceptionStats = 0;
	}

	m_keyframeMutex.lock();
	if (m_keyframe.data)
	{
		delete[]m_keyframe.data;
		m_keyframe.data = nullptr;
	}
	m_keyframeMutex.unlock();
	LOG_INFO(configData::getInstance().get_loggerId(), "delete rtsp client source:" << m_taskStruct.targetAddress);
}

taskJsonStruct & rtspClientSource::getTaskJson()
{
	return m_taskStruct;
	// TODO: 在此处插入 return 语句
}

void rtspClientSource::sourceRemoved()
{
	LOG_ERROR(configData::getInstance().get_loggerId(), "this func may not be called");
}
//
//bool rtspClientSource::addSink(taskExecutor * sink)
//{
//	std::lock_guard<std::mutex> guard(m_sinkerMutex);
//	m_sinkerList.push_back(sink);
//	return true;
//}
//
//bool rtspClientSource::removeSink(taskExecutor * sink)
//{
//	std::lock_guard<std::mutex> guard(m_sinkerMutex);
//	m_sinkerList.remove(sink);
//	return false;
//}

int rtspClientSource::handTask(taskScheduleObj * task_obj)
{
	int ret = 0;
	do
	{
		taskRtspLivingMediaType *mediaType=nullptr;
		taskRtspLivingSdp		*taskSdp = nullptr;
		taskManagerSinker		*managerSinker = nullptr;
		streamSinker			*sinker = nullptr;
		if (!task_obj)
		{
			ret = -1;
			break;
		}
		switch (task_obj->getType())
		{
		case schedule_rtsp_living_sdp:
			taskSdp = static_cast<taskRtspLivingSdp*>(task_obj);
			if (m_sessionList.size()<=0)
			{
				ret = -1;
				break;
			}
			if (m_sessionList.front().m_sdp.size()>0)
			{
				taskSdp->setSdp(m_sessionList.front().m_sdp);
				break;
			}

			if (!m_sessionList.front().generateSdp())
			{
				LOG_ERROR(configData::getInstance().get_loggerId(), "generate sdp failed");
				break;
			}
			taskSdp->setSdp(m_sessionList.front().m_sdp);
			break;
		case schedule_rtsp_living_mediaType:
			mediaType = static_cast<taskRtspLivingMediaType*>(task_obj);
			if (m_sessionList.size()<=0)
			{
				ret = -1;
				break;
			}
			mediaType->setMediaType(m_sessionList.front().m_payloadFmt);
			break;
		case schedule_manager_sinker:
			managerSinker = static_cast<taskManagerSinker*>(task_obj);
			m_sinkerMutex.lock();
			if (managerSinker->bAdd())
			{
				m_sinkerList.push_back(managerSinker->getSinker());
				if (m_sessionList.size())
				{
					//m_sessionList.at(0).m_sps
					sinker = static_cast<streamSinker*>(managerSinker->getSinker());
					if (m_sessionList.at(0).m_sps&&m_sessionList.at(0).m_pps)
					{
						DataPacket spsPkt, ppsPkt;
						spsPkt.data = (char*)m_sessionList.at(0).m_sps;
						spsPkt.size = m_sessionList.at(0).m_sps_len;
						ppsPkt.data = (char*)m_sessionList.at(0).m_pps;
						ppsPkt.size = m_sessionList.at(0).m_pps_len;
						sinker->addDataPacket(spsPkt);
						sinker->addDataPacket(ppsPkt);
						//如果有关键帧
						m_keyframeMutex.lock();
						if (m_keyframe.data)
						{
							sinker->addDataPacket(m_keyframe);
						}
						m_keyframeMutex.unlock();
					}
					//sinker->addDataPacket();
				}
			}
			else
			{
				taskExecutor *tmpExecutre = nullptr;
				for (auto  i:m_sinkerList)
				{
					if (i==managerSinker->getSinker())
					{
						managerSinker->getSinker()->handTask(task_obj);
						m_sinkerList.remove(managerSinker->getSinker());
						break;
					}
				}
				
			}
			m_sinkerMutex.unlock();
			break;
		default:
			break;
		}
	} while (0);
	return ret;
}

bool rtspClientSource::parseTaskParam()
{
	bool result = true;
	char *trash = nullptr;
	char *subStr = nullptr;
	char *tmp1 = nullptr, *tmp2 = nullptr;
	do
	{
		int ssRet = 0;
		//地址和端口号和control
		//rtsp://[<username>[:<password>]@]<server-address-or-name>[:<port>][/<stream-name>]
		//rtsp://admin:12345@192.0.0.64:554/h264/ch1/main/av_stream
		const char *prefix = "rtsp://";
		const int prefixLength = 7;
		trash = new char[m_taskStruct.targetAddress.size() + 1];
		subStr = new char[m_taskStruct.targetAddress.size() + 1];
		tmp1 = new char[m_taskStruct.targetAddress.size() + 1];
		tmp2 = new char[m_taskStruct.targetAddress.size() + 1];
		if (strnicmp(prefix, m_taskStruct.targetAddress.c_str(), 7) != 0)
		{
			result = false;
			break;
		}
		//下一个/之前是否有@来判断是否有用户名密码
		const char *p = m_taskStruct.targetAddress.c_str() + prefixLength;
		ssRet = sscanf(p, "%[^/]", trash);
		if (ssRet != 1 || strlen(trash) <= 0)
		{
			result = false;
			break;
		}
		char *pat = strchr(trash, '@');
		if (pat)
		{
			//有用户名和密码
			ssRet = sscanf(p, "%[^@]", subStr);
			if (ssRet != 1 || strlen(subStr) <= 0)
			{
				result = false;
				break;
			}
			//读取用户名和密码
			ssRet = sscanf(subStr, "%[^:]:%s", tmp1, tmp2);
			if (ssRet != 2)
			{
				result = false;
				break;
			}
			m_username = tmp1;
			m_password = tmp2;
		}
		//下一个/之前是否有:来判断是否有端口号
		int usrpsdSize = 0;
		if (pat)
		{
			usrpsdSize = m_username.size() + 1 + m_password.size() + 1;
		}
		p += usrpsdSize;
		ssRet = sscanf(p, "%[^/]", trash);
		if (ssRet != 1 || strlen(trash) <= 0)
		{
			result = false;
			break;
		}
		char* colon = strchr(trash, ':');
		if (colon != 0)
		{
			//有端口号
			ssRet = sscanf(trash, "%[^:]:%hu", tmp1, &m_port);
			if (ssRet != 2)
			{
				result = false;
				break;
			}
			m_svrAddr = tmp1;
		}
		else
		{
			//只有地址，采用默认端口号
			m_port = RTSP_DEFAULT_PORT;
			m_svrAddr = trash;
		}
		//最后为流名称
		p += (strlen(trash) + 1);
		m_streamName = p;
		//文件模式
		if (m_taskStruct.saveType == taskJsonSaveType_CreateNew)
		{
			m_createNewFile = true;
		}
		else
		{
			m_createNewFile = false;
		}
		//任务ID
		if (m_taskStruct.taskId.size() <= 0)
		{
			generateGUID64(m_taskStruct.taskId);
		}
	} while (0);
	if (trash)
	{
		delete[]trash;
		trash = nullptr;
	}
	if (subStr)
	{
		delete[]subStr;
		subStr = nullptr;
	}
	if (tmp1)
	{
		delete[]tmp1;
		tmp1 = nullptr;
	}
	if (tmp2)
	{
		delete[]tmp2;
		tmp2 = nullptr;
	}
	return result;
}

void rtspClientSource::rtspStreamStart()
{
	do
	{
		//由于sdp的特殊性 中间就有可能有/r/n/r/n所以sdp之后再启动接收线程
		//创套接字
		char rtspBuffer[RTSP_BUFFER_SIZE];
		std::string strRet;
		int recvRet;
		m_rtspSocket = socketCreateTcpClient(m_svrAddr.c_str(), m_port);
		if (m_rtspSocket < 0)
		{
			LOG_WARN(configData::getInstance().get_loggerId(), "socket error");
			break;
		}
		timeval time;
		time.tv_sec = s_tcpTimeoutSec;
		time.tv_usec = 0;
		setSocketTimeout(m_rtspSocket, time);
		//启动接收线程
		/*std::thread recvThread(std::mem_fun(&rtspClientSource::tcpRecv), this);
		m_recvThread = std::move(recvThread);*/
		//OPTION
		if (!sendOptions())
		{
			LOG_WARN(configData::getInstance().get_loggerId(), "options  failed");
			break;
		}
		/*std::unique_lock<std::mutex> lk_option(m_mutexCacher);
		m_streamCondition.wait(lk_option);
		std::list<DataPacket> dataList;
		m_tcpCacher->getValidBuffer(dataList, parseRtpRtcpRtspData);
		if (dataList.size() <= 0 || dataList.size() > 1)
		{
			break;
		}
		lk_option.unlock();
		std::string strRet;
		for (auto i : dataList)
		{
			strRet = i.data;
			strRet.resize(i.size);
		}*/
		recvRet = socketRecv(m_rtspSocket, rtspBuffer, RTSP_BUFFER_SIZE);
		if (recvRet <= 0)
		{
			LOG_WARN(configData::getInstance().get_loggerId(), "recv failed");
			break;
		}
		strRet = rtspBuffer;
		strRet.resize(recvRet);
		if (!parseRtspStatus(strRet))
		{
			LOG_WARN(configData::getInstance().get_loggerId(), "invalid options resb status");
			break;
		}
		if (!parseOptions(strRet))
		{
			LOG_WARN(configData::getInstance().get_loggerId(), "bad options resb");
			break;
		}
		//DESCRIBE
		if (!sendDescribe())
		{
			LOG_WARN(configData::getInstance().get_loggerId(), "send describe failed");
			break;
		}
		/*std::unique_lock<std::mutex> lk_describe(m_mutexCacher);
		m_streamCondition.wait(lk_describe);
		m_tcpCacher->getValidBuffer(dataList, parseRtpRtcpRtspData);
		if (dataList.size() <= 0 || dataList.size() > 1)
		{
			break;
		}
		lk_describe.unlock();
		for (auto i : dataList)
		{
			strRet = i.data;
			strRet.resize(i.size);
		}*/
		recvRet = socketRecv(m_rtspSocket, rtspBuffer, RTSP_BUFFER_SIZE);
		if (recvRet <= 0)
		{
			LOG_WARN(configData::getInstance().get_loggerId(), "recv failed");
			break;
		}
		strRet = rtspBuffer;
		strRet.resize(recvRet);
		if (!parseRtspStatus(strRet))
		{
			LOG_WARN(configData::getInstance().get_loggerId(), "invalid describe resb status");
			break;
		}
		if (!parseDescribe(strRet))
		{
			LOG_WARN(configData::getInstance().get_loggerId(), "bad describe resb");
			break;
		}
		if (m_sessionList.size())
		{
			if (m_sessionList.at(0).m_RAWRAWUDP)
			{
				//RAW/RAW/UDP
				rtspUdp();
			}
			else
			{
				//try RTP/AVP/UDP and thren try RTP/AVP/TCP
				//udp丢包严重，不考虑
				rtspTcp();
				break;
				/*if (!rtspUdp())
				{
					rtspTcp();
				}*/
			}
		}
	} while (0);
	notifyLiveEnded();
	//通知任务调度器，删除该任务
	if (!m_hasCalledDeleteSelf)
	{
		task_json *stopTask = new task_json(m_taskStruct.taskId);
		taskSchedule::getInstance().addTaskObj(stopTask);
	}
}

void rtspClientSource::tcpRecv()
{
	char recvBuf[RTSP_BUFFER_SIZE];
	while (!m_end)
	{
		int ret = socketRecv(m_rtspSocket, recvBuf, RTSP_BUFFER_SIZE);
		if (ret <= 0)
		{
			int err = socketError();
			m_end = true;
			break;
		}
		std::lock_guard<std::mutex> guard(m_mutexCacher);
		if (0 != m_tcpCacher->appendBuffer(recvBuf, ret))
		{
			break;
		}
		m_streamCondition.notify_one();
	}
	m_streamCondition.notify_one();
}

bool rtspClientSource::rtspUdp()
{
	bool result = true;
	char rtspBuffer[RTSP_BUFFER_SIZE];
	std::string strRet;
	int recvRet;
	do
	{

		if (!creaetRtpRtcpSocket())
		{
			LOG_WARN(configData::getInstance().get_loggerId(), "create rtp rtcp socket for udp failed;");
			result = false;
			break;
		}
		//setup
		if (!sendSetup(false, m_sessionList.at(0).m_RAWRAWUDP))
		{
			LOG_WARN(configData::getInstance().get_loggerId(), "send setup failed;");
			result = false;
			break;
		}
		/*std::unique_lock<std::mutex> lk_setup(m_mutexCacher);
		m_streamCondition.wait(lk_setup);
		std::list<DataPacket> dataList;
		m_tcpCacher->getValidBuffer(dataList, parseRtpRtcpRtspData);
		if (dataList.size() <= 0 || dataList.size() > 1)
		{
			result = false;
			break;
		}
		lk_setup.unlock();
		std::string strRet;
		for (auto i : dataList)
		{
			strRet = i.data;
			strRet.resize(i.size);
		}*/
		recvRet = socketRecv(m_rtspSocket, rtspBuffer, RTSP_BUFFER_SIZE);
		if (recvRet <= 0)
		{
			LOG_WARN(configData::getInstance().get_loggerId(), "recv failed");
			result = false;
			break;
		}
		strRet = rtspBuffer;
		strRet.resize(recvRet);
		strRet = strRet;
		//解析setup
		if (!parseSetup(strRet))
		{
			result = false;
			LOG_WARN(configData::getInstance().get_loggerId(), "parse setup failed");
			break;
		}

		//play
		if (!sendPlay())
		{
			LOG_WARN(configData::getInstance().get_loggerId(), "send play failed;");
			result = false;
			break;
		}
		//发送rtcp 空数据用于穿透udp
		netPenetrate();
		//!发送rtcp 空数据用于穿透udp


		/*if (!sendPlay())
		{
			LOG_WARN(configData::getInstance().get_loggerId(), "send play failed;");
			result = false;
			break;
		}*/
		//发送play后启动接收线程
		//30s超时值，如果超时未收到数据，返回false,尝试TCP方式
		timeval time;
		time.tv_sec = 30;
		time.tv_usec = 0;
		setSocketTimeout(m_rtpSocket, time);
		//RTCP不需要超时值，因为可能没有RTCP
		sockaddr_in	remoteAddr;
		int nAddrlen = sizeof remoteAddr;
		remoteAddr.sin_family = AF_INET;
		remoteAddr.sin_port = htons(m_rtpPort);
		remoteAddr.sin_addr.s_addr = inet_addr(m_svrAddr.c_str());
		recvRet = socketRecvfrom(m_rtpSocket, rtspBuffer, RTSP_BUFFER_SIZE, 0, (sockaddr*)&remoteAddr, nAddrlen);
		if (recvRet <= 0)
		{
			int err = socketError();
			result = false;
			break;
		}
		else
		{
			//将这个包加入收到的队列
			m_rtpDataMutex.lock();
			DataPacket datapacket;
			datapacket.size = recvRet;
			datapacket.data = new char[recvRet];
			memcpy(datapacket.data, rtspBuffer, recvRet);
			m_rtpDataList.push_back(datapacket);
			m_rtpDataMutex.unlock();
			//启动接收线程

			std::thread rtspThread(std::mem_fun(&rtspClientSource::rtspProcessThread), this);
			std::thread heartbeatThread(std::mem_fun(&rtspClientSource::rtspHeartbeatThread), this);
			std::thread rtpThread(std::mem_fun(&rtspClientSource::rtpProcessThread), this);
			std::thread rtcpThread(std::mem_fun(&rtspClientSource::rtcpProcessThread), this);


			if (rtspThread.joinable())
			{
				rtspThread.join();
			}
			if (heartbeatThread.joinable())
			{
				heartbeatThread.join();
			}
			if (rtpThread.joinable())
			{
				rtpThread.join();
			}
			if (rtcpThread.joinable())
			{
				rtcpThread.join();
			}

		}
		/*recvRet = socketRecv(m_rtspSocket, rtspBuffer, RTSP_BUFFER_SIZE);
		if (recvRet<=0)
		{
			LOG_WARN(configData::getInstance().get_loggerId(), "recv play failed;");
			result = false;
			break;
		}
		strRet = rtspBuffer;
		strRet.resize(recvRet);
		strRet = strRet;
		if (!parsePlay(strRet))
		{
			result = false;
		}*/
	} while (0);

	return result;
}

void rtspClientSource::rtspTcp()
{
	char rtspBuffer[RTSP_BUFFER_SIZE];
	std::string strRet;
	int recvRet;
	do
	{
		//setup
		//setup
		if (!sendSetup(true, m_sessionList.at(0).m_RAWRAWUDP))
		{
			LOG_WARN(configData::getInstance().get_loggerId(), "send setup failed;");
			break;
		}
		recvRet = socketRecv(m_rtspSocket, rtspBuffer, RTSP_BUFFER_SIZE);
		if (recvRet <= 0)
		{
			LOG_WARN(configData::getInstance().get_loggerId(), "recv failed");
			break;
		}
		strRet = rtspBuffer;
		strRet.resize(recvRet);
		strRet = strRet;
		//解析setup
		if (!parseSetup(strRet))
		{
			LOG_WARN(configData::getInstance().get_loggerId(), "parse setup failed");
			break;
		}
		//play
		if (!sendPlay())
		{
			LOG_WARN(configData::getInstance().get_loggerId(), "send play failed;");
			break;
		}
		//rtsp thread
		//heart thread
		//启动接收线程

		std::thread rtspThread(std::mem_fun(&rtspClientSource::rtspProcessThread), this);
		std::thread heartbeatThread(std::mem_fun(&rtspClientSource::rtspHeartbeatThread), this);

		if (rtspThread.joinable())
		{
			rtspThread.join();
		}
		if (heartbeatThread.joinable())
		{
			heartbeatThread.join();
		}
	} while (0);
}

char * rtspClientSource::getLine(char * startOfLine)
{
	for (char *ptr = startOfLine; *ptr != '\0'; ptr++)
	{
		if (*ptr == '\r')
		{
			*ptr = '\0';
			ptr++;
			if (*ptr == '\n')
			{
				ptr++;
			}
			return ptr;
		}
		if (*ptr == '\n')
		{
			*ptr = '\0';
			ptr++;
			return ptr;
		}
	}
	return 0;
}

int rtspClientSource::getLineCount(char const * pString)
{
	int numLine = 0;
	int index = 0;
	do
	{
		if (pString[index] == '\r')
		{
			if (pString[index + 1] == '\n')
			{
				numLine++;
				char t = pString[index - 1];
				continue;
			}
		}
		else if (pString[index] == '\0')
		{
			break;
		}
	} while (pString[index++] != '\0');
	return	numLine;
}

bool rtspClientSource::checkHeader(char const * line, char const * headerName, char const *& headerParams)
{
	if (strncmp(line, headerName, strlen(headerName)) != 0)
	{
		return false;
	}
	int indexParam = strlen(headerName);
	while (line[indexParam] != '\0' && (line[indexParam] == ' ' || line[indexParam] == '\t'))
	{
		indexParam++;
	}
	if (line[indexParam] == '\0')
	{
		return	false;
	}
	headerParams = &line[indexParam];
	return	true;
}

bool rtspClientSource::parseSdpLine(char const * inputLine, char const *& nextLine)
{
	nextLine = nullptr;
	for (char const* ptr = inputLine; *ptr != '\0'; ++ptr)
	{
		if (*ptr == '\r' || *ptr == '\n') {
			++ptr;
			while (*ptr == '\r' || *ptr == '\n') ++ptr;
			nextLine = ptr;
			if (nextLine[0] == '\0') nextLine = nullptr;
			break;
		}
	}

	if (inputLine[0] == '\r' || inputLine[0] == '\n')
		return true;
	if (strlen(inputLine) < 2 || inputLine[1] != '='
		|| inputLine[0] < 'a' || inputLine[0] > 'z')
	{

		return false;
	}

	return true;
}

bool rtspClientSource::parseSDPLine_s(char const * sdpLine)
{
	bool result = false;
	char *buf = new char[strlen(sdpLine) + 1];
	do
	{
		if (1 == sscanf(sdpLine, "s=%[^\r\n]", buf))
		{
			m_sessionName = buf;
			result = true;
		}
	} while (0);
	if (buf)
	{
		delete[]buf;
	}
	return result;
}

bool rtspClientSource::parseSDPLine_i(char const * sdpLine)
{
	bool result = false;
	char *buf = new char[strlen(sdpLine) + 1];
	do
	{
		if (1 == sscanf(sdpLine, "i=%[^\r\n]", buf))
		{
			m_sessionDescription = buf;
			result = true;
		}
	} while (0);
	if (buf)
	{
		delete[]buf;
	}
	return result;
}

bool rtspClientSource::parseSDPLine_c(char const * sdpLine)
{
	bool result = false;
	char *buf = new char[strlen(sdpLine) + 1];
	do
	{
		if (1 == sscanf(sdpLine, "c=IN IP4 %[^/\r\n]", buf))
		{
			m_connectionEndpointName = buf;

			result = true;
		}
	} while (0);
	if (buf)
	{
		delete[]buf;
	}
	return result;
}

bool rtspClientSource::parseSDPAttribute_type(char const * sdpLine)
{
	bool result = false;
	char *buf = new char[strlen(sdpLine) + 1];
	do
	{
		if (1 == sscanf(sdpLine, "a=type: %[^ ]", buf))
		{
			m_mediaSessionType = buf;

			result = true;
		}
	} while (0);
	if (buf)
	{
		delete[]buf;
	}
	return result;
}

bool rtspClientSource::parseSDPAttribute_control(char const * sdpLine)
{
	bool result = false;
	char *buf = new char[strlen(sdpLine) + 1];
	do
	{
		if (1 == sscanf(sdpLine, "a=control: %s", buf))
		{
			m_controlPath = buf;

			result = true;
		}
	} while (0);
	if (buf)
	{
		delete[]buf;
	}
	return result;
}

bool rtspClientSource::parseSDPAttribute_range(char const * sdpLine)
{
	bool result = false;
	do
	{
		if (2 == sscanf(sdpLine, "a=range: npt = %lg - %lg", &m_startTime, &m_endTime) ||
			2 == sscanf(sdpLine, "a=range:npt=%lg-%lg", &m_startTime, &m_endTime))
		{
			m_bClock = false;
			result = true;
			break;
		}
		else
		{
			char *pS = new char[strlen(sdpLine) + 1];
			char *pE = new char[strlen(sdpLine) + 1];
			int ssResult = sscanf(sdpLine, "a=range: clock = %[^-\r\n]-%[^\r\n]", pS, pE);
			if (ssResult == 2)
			{
				m_startClock = pS;
				m_endClock = pE;
				m_bClock = true;

				result = true;
			}
			else
			{
				m_startTime = 0;
				m_endTime = 0;
				m_bClock = false;
			}
			delete[]pS;
			delete[]pE;
		}
	} while (0);
	return result;
}

bool rtspClientSource::parseSDPAttribute_source_filter(char const * sdpLine)
{
	bool result = false;
	do
	{

	} while (0);
	return result;
}

bool rtspClientSource::parseSdpAttribute_rtpmap(const char * sdpDescription, rtspSession &subSession)
{
	bool	bRet = false;
	unsigned int rtpmapPayloadFmt;
	unsigned int numChannels = 1;
	char *strCodecName = new char[strlen(sdpDescription)];
	if (sscanf(sdpDescription, "a=rtpmap: %u %[^/]/%u/%u",
		&rtpmapPayloadFmt, strCodecName, &subSession.m_uRtpTimestampFrequency, &numChannels) == 4 ||
		sscanf(sdpDescription, "a=rtpmap: %u %[^/]/%u",
			&rtpmapPayloadFmt, strCodecName, &subSession.m_uRtpTimestampFrequency) == 3 ||
		sscanf(sdpDescription, "a=rtpmap: %u %s",
			&rtpmapPayloadFmt, strCodecName) == 2)
	{
		bRet = true;
		subSession.m_strCodecName = strCodecName;
		subSession.m_payloadFmt = rtpmapPayloadFmt;
	}
	delete[]strCodecName;
	return	bRet;
}

bool rtspClientSource::parseSdpAttribute_control(const char * sdpDescription, rtspSession &subSession)
{
	bool	bRet = true;
	char *strControlPath = new char[strlen(sdpDescription)];
	if (sscanf(sdpDescription, "a=control: %s", strControlPath) == 1)
	{
		bRet = true;
		subSession.m_control = strControlPath;
	}
	else
	{
		bRet = false;
	}
	delete[]strControlPath;
	return	bRet;
}

bool rtspClientSource::parseSdpAttribute_fmtp(const char * sdpDescription, rtspSession &subSession)
{
	bool bRet = true;

	if (strncmp(sdpDescription, "a=fmtp:", 7) == 0)
	{
		char *strKey = new char[strlen(sdpDescription) + 1];
		char *strValue = new char[strlen(sdpDescription) + 1];
		while (*sdpDescription != '\0'&&*sdpDescription != '\r'&&*sdpDescription != '\n')
		{
			int scanfResult = sscanf(sdpDescription, " %[^=; \t\r\n] = %[^; \t\r\n]", strKey, strValue);
			if (scanfResult == 2)
			{
				if (strcmp(strKey, "profile-level-id") == 0)
				{
					subSession.m_profile_level_id = strValue;
				}
				else if (strcmp(strKey, "packetization-mode") == 0)
				{
					subSession.m_packetization_mode = atoi(strValue);
				}
				else if (strcmp(strKey, "sprop-parameter-sets") == 0)
				{
					char* sep = strchr(strValue, ',');
					int		sps_size = sep - strValue;
					int		pps_size = strlen(strValue) - sps_size;

					char *sps_base64 = new char[sps_size + 1];
					memset(sps_base64, '\0', sps_size + 1);
					memcpy(sps_base64, strValue, sps_size);
					subSession.m_sps_base64 = sps_base64;
					char *pps_base64 = new char[pps_size + 1];
					memset(pps_base64, '\0', pps_size + 1);
					memcpy(pps_base64, sep + 1, pps_size);
					subSession.m_pps_base64 = pps_base64;
				}
			}
			while (*sdpDescription != '\0' && *sdpDescription != '\r' && *sdpDescription != '\n' && *sdpDescription != ';') ++sdpDescription;
			while (*sdpDescription == ';') ++sdpDescription;
		}
		delete[]strKey;
		delete[]strValue;
		if (subSession.m_sps_base64.size())
		{
			subSession.m_sps = base64_decode((char*)subSession.m_sps_base64.c_str(),
				strlen(subSession.m_sps_base64.c_str()), &subSession.m_sps_len);
		}
		if (subSession.m_pps_base64.size())
		{
			subSession.m_pps = base64_decode((char*)subSession.m_pps_base64.c_str(),
				strlen(subSession.m_pps_base64.c_str()), &subSession.m_pps_len);
		}

	}
	else
	{
		bRet = false;
	}

	return	bRet;
}

bool rtspClientSource::parseTransport(const char * pTransport)
{
	bool result = true;
	char *protocol = new char[strlen(pTransport)];
	do
	{
		if (sscanf(pTransport, "Transport: %[^;]", protocol) == 1)
		{
			if (strcmp(protocol, RTSP_RTP_AVP) == 0)
			{
				m_tcpProtocol = false;
			}
			else
			{
				m_tcpProtocol = true;
			}
			const char* pValues = pTransport + strlen("Transport: ") + strlen(protocol) + strlen(";");
			char* pValue = new char[strlen(pTransport)];
			char* tempArray = new char[strlen(pTransport)];
			while (sscanf(pValues, "%[^;]", pValue) == 1)
			{
				pValues += strlen(pValue);
				if (strcmp(pValue, "unicast") == 0)
				{
					m_multicast = false;
				}
				else if (sscanf(pValue, "destination=%[^;]", tempArray) == 1)
				{
					m_destination = tempArray;
				}
				else if (sscanf(pValue, "source=%[^;]", tempArray) == 1)
				{
					m_source = tempArray;
				}
				else if (sscanf(pValue, "client_port=%hu-%hu", &m_rtpPort, &m_rtcpPort) == 2)
				{
				}
				else if (sscanf(pValue, "server_port=%hu-%hu", &m_rtpSvrPort, &m_rtcpSvrPort) == 2)
				{
				}
				else if (sscanf(pValue, "port=%hu-%hu", &m_rtpMulticastPort, &m_rtcpMulticastPort) == 2 ||
					sscanf(pValue, "port=%hu", &m_rtpMulticastPort) == 1)
				{
				}
				if (pValues[0] == ';')
				{
					pValues++;
				}

			}
			delete[]pValue;
			pValue = nullptr;
			delete[]tempArray;
			tempArray = nullptr;
		}
		else
		{
			result = false;
		}
	} while (0);
	if (protocol)
	{
		delete[]protocol;
		protocol = nullptr;
	}
	return result;
}

bool rtspClientSource::parseSession(const char * pSession)
{
	bool result = true;
	char *strSession = new char[strlen(pSession)];
	if (sscanf(pSession, "Session: %[^;]", strSession) == 1)
	{
		m_sessionId = strSession;
		const char *pTimeOut = pSession + strlen("Session: ") + strlen(strSession);
		if (sscanf(pTimeOut, ";timeout=%d", &m_timeout) != 1)
		{
			m_timeout = 60;
		}

		//在setup中可能获得RTSP的超时值,然而在setup后 udp 方式可能导致无法收到rtsp响应
		timeval time;
		time.tv_sec = m_timeout * 2;
		time.tv_usec = 0;
		setSocketTimeout(m_rtspSocket, time);
	}
	else
	{
		result = false;
	}
	delete[]strSession;
	return result;
}

bool rtspClientSource::parseRtspStatus(std::string rtspBuf)
{
	bool result = true;
	int offset = strlen(RTSP_VER);
	const char *seq = nullptr;
	char buf[8];
	do
	{
		if (strncmp(rtspBuf.c_str(), RTSP_VER, offset) != 0)
		{
			result = false;
			LOG_WARN(configData::getInstance().get_loggerId(), "invalid rtsp status");
			break;
		}
		seq = strchr(rtspBuf.c_str() + offset + 1, ' ');
		if (!seq)
		{
			result = false;
			LOG_WARN(configData::getInstance().get_loggerId(), "no seq in rtsp resb");
			break;
		}
		strncpy(buf, rtspBuf.c_str() + offset, seq - rtspBuf.c_str() - offset);

		int status = atoi(buf);
		if (status != 200)
		{
			result = false;
			LOG_WARN(configData::getInstance().get_loggerId(), "rtsp status err:" << status);
			break;
		}
	} while (0);
	return result;
}

bool rtspClientSource::sendOptions()
{
	bool result = true;
	do
	{
		char sendBuf[RTSP_BUFFER_SIZE];
		sprintf(sendBuf,
			RTSP_METHOD_OPTIONS " %s " RTSP_VER RTSP_EL
			HDR_CSEQ ": %d " RTSP_EL
			HDR_USER_AGENT ": %s " RTSP_EL RTSP_EL,
			m_taskStruct.targetAddress.c_str(), m_cseq++, configData::getInstance().get_server_name());
		int sendLen = strlen(sendBuf);
		sendLen = socketSend(m_rtspSocket, sendBuf, sendLen);
		if (sendLen <= 0)
		{
			result = false;
			break;
		}
	} while (0);
	return result;
}

bool rtspClientSource::sendOptionsWithSession()
{
	bool result = true;
	do
	{
		char sendBuf[RTSP_BUFFER_SIZE];
		sprintf(sendBuf,
			RTSP_METHOD_OPTIONS " %s " RTSP_VER RTSP_EL
			HDR_CSEQ ": %d " RTSP_EL
			HDR_SESSION ": %s" RTSP_EL
			HDR_USER_AGENT ": %s " RTSP_EL RTSP_EL,
			m_taskStruct.targetAddress.c_str(),
			m_cseq++,
			m_sessionId.c_str(),
			configData::getInstance().get_server_name());
		int sendLen = strlen(sendBuf);
		sendLen = socketSend(m_rtspSocket, sendBuf, sendLen);
		if (sendLen <= 0)
		{
			result = false;
			break;
		}
	} while (0);
	return result;
}

bool rtspClientSource::parseOptions(std::string rtspBuf)
{
	bool result = true;
	char *pLine = nullptr;
	do
	{
		char *pLineStore = nullptr;
		pLine = new char[rtspBuf.size() + 1];
		strcpy(pLine, rtspBuf.c_str());
		pLineStore = pLine;
		char *lineStart = nullptr;
		char *nextLineStart = pLine;
		do
		{
			lineStart = nextLineStart;
			nextLineStart = getLine(lineStart);
		} while (lineStart[0] == '\0'&&nextLineStart != nullptr);
		int numLine = 0;
		numLine = getLineCount(rtspBuf.c_str());
		strcpy(pLine, rtspBuf.c_str());
		lineStart = nextLineStart = pLine;

		while (true)
		{
			nextLineStart = getLine(lineStart);
			if (lineStart[0] == '\0' || nextLineStart == 0)
			{
				break;
			}
			const char*	strHeaderParam = 0;
			if (checkHeader(lineStart, "CSeq:", strHeaderParam))
			{
				int cseq;
				if (sscanf(strHeaderParam, "%u", &cseq) != 1 || cseq < 0)
				{
					LOG_WARN(configData::getInstance().get_loggerId(), "bad CSeq" << lineStart);
					result = false;
					break;
				}
			}
			else if (checkHeader(lineStart, "Date:", strHeaderParam))
			{
				m_strDate = strHeaderParam;
			}
			else if (checkHeader(lineStart, "Public:", strHeaderParam))
			{
				char* strNextPublic = const_cast<char*>(strHeaderParam);
				char* strParam = strNextPublic;
				int numPublic = 0;
				while (strNextPublic != 0)
				{
					strNextPublic = strstr(strParam, ",");
					if (strNextPublic == 0)
					{
						if (strlen(strParam) > 0)
						{
							numPublic++;
						}
						break;
					}
					strNextPublic++;
					while (*strNextPublic == ' ')
					{
						strNextPublic++;
					}
					strParam = strNextPublic;
					numPublic++;
				}
				if (numPublic > 0)
				{
					strNextPublic = const_cast<char*>(strHeaderParam);
					strParam = strNextPublic;
					int curFunc = 0;
					while (strNextPublic != 0)
					{
						std::string strFunc;
						strNextPublic = strstr(strParam, ",");
						if (strNextPublic != 0)
						{
							int nFuncNameLen = strNextPublic - strParam + 1;
							strParam[nFuncNameLen - 1] = '\0';
							strFunc = strParam;
							m_publicFuncList.push_back(strFunc);
						}
						else
						{
							if (strlen(strParam) > 0)
							{
								strFunc = strParam;
								m_publicFuncList.push_back(strFunc);
							}
							break;
						}
						strNextPublic++;
						while (*strNextPublic == ' ')
						{
							strNextPublic++;
						}
						strParam = strNextPublic;
						curFunc++;
					}
				}
			}
			lineStart = nextLineStart;
		}
	} while (0);
	if (pLine)
	{
		delete[]pLine;
	}
	return result;
}


bool rtspClientSource::sendDescribe()
{
	bool result = true;
	do
	{
		char sendBuf[RTSP_BUFFER_SIZE];
		sprintf(sendBuf,
			RTSP_METHOD_DESCRIBE " %s " RTSP_VER RTSP_EL
			HDR_CSEQ ": %d " RTSP_EL
			HDR_USER_AGENT ": %s " RTSP_EL RTSP_EL,
			m_taskStruct.targetAddress.c_str(), m_cseq++, configData::getInstance().get_server_name());
		int sendLen = strlen(sendBuf);
		sendLen = socketSend(m_rtspSocket, sendBuf, sendLen);
		if (sendLen <= 0)
		{
			result = false;
			break;
		}
	} while (0);
	return result;
}

bool rtspClientSource::parseDescribe(std::string rtspBuf)
{
	bool result = true;
	do
	{
		//取sdp
		int index = 0;
		int nlen = rtspBuf.size();
		std::string strSDP;
		while (index + 4 < nlen)
		{
			if (rtspBuf.at(index + 0) == '\r'&&rtspBuf.at(index + 1) == '\n'
				&&rtspBuf.at(index + 2) == '\r'&&rtspBuf.at(index + 3) == '\n')
			{
				const char*	p = rtspBuf.c_str() + index + 4;
				if (strlen(p) > 0)
				{
					strSDP = p;
				}
				else
				{
					result = false;
					break;
				}
			}
			index++;
		}
		//mediasession
		const char *sdpLine = strSDP.c_str();
		const char *nextSdpLine = nullptr;
		while (true)
		{
			if (!parseSdpLine(sdpLine, nextSdpLine))
			{
				result = false;
				break;
			}
			if (sdpLine[0] == 'm')
			{
				break;
			}
			sdpLine = nextSdpLine;
			if (sdpLine == nullptr)
			{
				break;
			}

			//type
			//control
			//range
			if (parseSDPLine_s(sdpLine))
				continue;
			if (parseSDPLine_i(sdpLine))
				continue;
			if (parseSDPLine_c(sdpLine))
				continue;
			if (parseSDPAttribute_control(sdpLine))
				continue;
			if (parseSDPAttribute_range(sdpLine))
				continue;
			if (parseSDPAttribute_type(sdpLine))
				continue;
			if (parseSDPAttribute_source_filter(sdpLine))
				continue;
		}
		if (!result)
		{
			break;
		}
		//tool
		while (sdpLine != nullptr)
		{
			rtspSession tmpSession;

			m_sessionList.push_back(tmpSession);
			rtspSession &lastSession = m_sessionList.back();
			//解析m
			if (strstr(sdpLine, "RAW/RAW/UDP"))
			{
				lastSession.m_RAWRAWUDP = true;
			}
			else
			{
				lastSession.m_RAWRAWUDP = false;
			}
			while (true)
			{
				sdpLine = nextSdpLine;
				if (sdpLine == NULL)
					break;
				if (!parseSdpLine(sdpLine, nextSdpLine))
					return false;

				//解析m内容直到下一个m

				if (sdpLine[0] == 'm')
				{
					break;
				}
				if (parseSdpAttribute_rtpmap(sdpLine, lastSession))
				{
					continue;
				}
				if (parseSdpAttribute_control(sdpLine, lastSession))
				{
					continue;
				}
				if (parseSdpAttribute_fmtp(sdpLine, lastSession))
				{
					continue;
				}
			}
		}
	} while (0);
	return result;
}

bool rtspClientSource::sendSetup(bool tcp, bool rawrawudp)
{
	bool result = true;
	do
	{
		char sendBuf[RTSP_BUFFER_SIZE];
		char transport[20];
		char inter_port[128];
		if (tcp)
		{
			strcpy(transport, "RTP/AVP/TCP");
			strcpy(inter_port, "interleaved=0-1");
		}
		else
		{
			if (rawrawudp)
			{
				strcpy(transport, "RAW/RAW/UDP");
			}
			else
			{
				strcpy(transport, "RTP/AVP/UDP");
			}
			//create udp port;
			sprintf(inter_port, "client_port=%d-%d", m_rtpPort, m_rtcpPort);
		}
		sprintf(sendBuf,
			RTSP_METHOD_SETUP " %s/%s " RTSP_VER RTSP_EL
			HDR_CSEQ ": %d " RTSP_EL
			HDR_USER_AGENT ": %s " RTSP_EL
			HDR_TRANSPORT ": %s;unicast;%s " RTSP_EL RTSP_EL,
			m_taskStruct.targetAddress.c_str(), m_sessionList[0].m_control.c_str(),
			m_cseq++,
			configData::getInstance().get_server_name(),
			transport, inter_port
		);
		int sendLen = strlen(sendBuf);
		sendLen = socketSend(m_rtspSocket, sendBuf, sendLen);
		if (sendLen <= 0)
		{
			result = false;
			break;
		}
	} while (0);
	return result;
}

bool rtspClientSource::creaetRtpRtcpSocket()
{
	int iRet = 0;
	do
	{
		//系统自动分配RTP端口号;
		closeSocket(m_rtpSocket);	m_rtpSocket = -1;
		closeSocket(m_rtcpSocket);	m_rtcpSocket = -1;
		m_rtpSocket = socketCreateUdpServer(0);
		if (m_rtpSocket < 0)
		{
			iRet = -1;
			break;
		}
		//读取端口号;
		if (0 != get_source_port(m_rtpSocket, m_rtpPort))
		{
			printf("get udp RTP source port failed;\n");
			iRet = -1;
			closeSocket(m_rtpSocket);	m_rtpSocket = -1;
			break;
		}
		//端口号奇偶;
		if ((m_rtpPort & 1) != 0)
		{
			closeSocket(m_rtpPort);	m_rtpSocket = -1;
			continue;
		}
		//RTCP
		m_rtcpSocket = socketCreateUdpServer(m_rtpPort + 1);
		if (m_rtcpSocket < 0)
		{
			closeSocket(m_rtpSocket);	m_rtpSocket = -1;
			closeSocket(m_rtcpSocket);	m_rtcpSocket = -1;
			continue;
		}
		if (0 != get_source_port(m_rtcpSocket, m_rtcpPort))
		{
			printf("get udp RTCP source port failed;\n");
			closeSocket(m_rtpSocket);	m_rtpSocket = -1;
			closeSocket(m_rtcpSocket);	m_rtcpSocket = -1;
			iRet = -1;
			break;
		}
		if (m_rtcpPort != m_rtpPort + 1)
		{
			iRet = -1;
			break;
		}
		break;
	} while (true);
	return (iRet == 0);
}

bool rtspClientSource::parseSetup(std::string rtspBuf)
{
	bool result = true;
	do
	{
		//status
		result = parseRtspStatus(rtspBuf);
		if (!result)
		{
			break;
		}
		const char *lineStart = nullptr, *nextLineStart = nullptr;
		lineStart = nextLineStart = rtspBuf.c_str();
		while (true)
		{
			lineStart = nextLineStart;
			nextLineStart = getLine(const_cast<char*>(lineStart));
			if (lineStart[0] == '\0' || nextLineStart == nullptr)
			{
				break;
			}

			//transport
			if (parseTransport(lineStart))
			{
				continue;
			}
			//session
			if (parseSession(lineStart))
			{
				continue;
			}
		}
	} while (0);
	return result;
}

bool rtspClientSource::sendPlay()
{
	bool result = true;
	do
	{
		char sendBuf[RTSP_BUFFER_SIZE];
		sprintf(sendBuf,
			RTSP_METHOD_PLAY " %s " RTSP_VER RTSP_EL
			HDR_CSEQ ": %d " RTSP_EL
			HDR_USER_AGENT ": %s " RTSP_EL
			HDR_SESSION ": %s" RTSP_EL
			HDR_RANGE ": npt=0.0- " RTSP_EL RTSP_EL,
			m_taskStruct.targetAddress.c_str(),
			m_cseq++,
			configData::getInstance().get_server_name(),
			m_sessionId.c_str()
		);
		int sendLen = strlen(sendBuf);
		sendLen = socketSend(m_rtspSocket, sendBuf, sendLen);
		if (sendLen <= 0)
		{
			result = false;
			break;
		}
	} while (0);
	return result;
}

bool rtspClientSource::parsePlay(std::string rtspBuf)
{
	bool result = true;
	do
	{
		result = parseRtspStatus(rtspBuf);
		if (!result)
		{
			break;
		}
	} while (0);
	return result;
}

void rtspClientSource::netPenetrate()
{
	for (int i = 0; i < 2; i++)
	{
		sockaddr_in	remoteAddr;
		remoteAddr.sin_family = AF_INET;
		remoteAddr.sin_port = htons(m_rtpSvrPort);
		remoteAddr.sin_addr.s_addr = inet_addr(m_svrAddr.c_str());
		int nAddrlen = sizeof remoteAddr;
		int iRet = sendto(m_rtpSocket, "    ", 4, 0, (sockaddr*)&remoteAddr, nAddrlen);
		if (iRet <= 0)
		{
			LOG_WARN(configData::getInstance().get_loggerId(), "net peretrate failed:" << socketError());
		}
		remoteAddr.sin_port = htons(m_rtcpSvrPort);
		iRet = sendto(m_rtcpSocket, "    ", 4, 0, (sockaddr*)&remoteAddr, nAddrlen);
		if (iRet <= 0)
		{
			LOG_WARN(configData::getInstance().get_loggerId(), "net peretrate failed:" << socketError());
		}
	}
}

void rtspClientSource::rtspProcessThread()
{
	int ret = 0;
	const int rtspBufSize = 0xffff;
	char buf[rtspBufSize];
	tcpCacher rtspCacher;
	std::list<DataPacket> datalilst;
	bool getFrame = false;
	int channel = 0;
	while (!m_end)
	{
#if 0
		ret = socketRecv(m_rtspSocket, buf, rtspBufSize);
		if (ret <= 0)
		{
			m_end = true;
			LOG_WARN(configData::getInstance().get_loggerId(), "rtsp process thread recv failed");
			break;
		}

		if (0 != rtspCacher.appendBuffer(buf, ret))
		{
			LOG_WARN(configData::getInstance().get_loggerId(), "add buff to rtsp cacher failed");
			m_end = true;
			break;
		}
		rtspCacher.getValidBuffer(datalilst, parseRtpRtcpRtspData);

#else
		channel = 0;
		if (!rtspRecvPacket(buf, rtspBufSize, ret, channel))
		{
			m_end = true;
			break;
			//continue;
		}
#endif // 0
		if (ret > 0)
		{
			if (buf[0] == 'R')
			{

			}
			else
			{
				if (channel == 0)
				{
					switch (m_sessionList[0].m_payloadFmt)
					{
					case RTSP_payload_h264:
						addRtpDataToProcesser(buf, ret, m_sessionList[0].m_payloadFmt);
						break;
					default:
						break;
					}
				}
				else if (channel == 1)
				{
					processRtcpData(buf, ret);
				}
			}
		}
	}
	closeRtspRtpRtcpSocket();
}

bool rtspClientSource::rtspRecvPacket(char *buff, int bufSize, int &outSize, int &channel)
{
	enum TCP_Reading_State
	{
		AWAITING_DOLLAR,
		AWAITING_STREAM_CHANNEL_ID,
		AWAITING_SIZE1,
		AWAITING_SIZE2,
		AWAITING_PACKET_DATA,
		AWAITING_RTSP_END,
	};
	TCP_Reading_State readState = AWAITING_DOLLAR;
	bool result = true;
	int recvRet = 0;
	char c;
	int cur = 0;
	char tmp8;
	unsigned char size1, size2;
	unsigned short pkgSize = 0;
	bool getPkg = false;
	while (!getPkg)
	{
		if (readState != AWAITING_PACKET_DATA)
		{
			recvRet = socketRecv(m_rtspSocket, &tmp8, 1);
			if (recvRet <= 0)
			{
				m_end = true;
				result = false;
				break;
			}
		}
		switch (readState)
		{
		case AWAITING_DOLLAR:
			if (tmp8 == '$')
			{
				readState = AWAITING_STREAM_CHANNEL_ID;

				//buff[cur++] = tmp8;
			}
			else
			{
				if (tmp8 != 'R')
				{
					continue;
				}
				readState = AWAITING_RTSP_END;

				buff[cur++] = tmp8;
			}
			break;
		case AWAITING_STREAM_CHANNEL_ID:
			readState = AWAITING_SIZE1;
			if (tmp8 != 0 && tmp8 != 1)
			{
				result = false;
				break;
			}
			if (tmp8 == 1)
			{
				tmp8 = 1;
			}
			channel = tmp8;
			break;
		case AWAITING_SIZE1:
			size1 = tmp8;
			readState = AWAITING_SIZE2;
			break;
		case AWAITING_SIZE2:
			readState = AWAITING_PACKET_DATA;
			size2 = tmp8;
			break;
		case AWAITING_PACKET_DATA:
			pkgSize = ((size2 << 8) | size1);
			pkgSize = ntohs(pkgSize);
			if (pkgSize > bufSize)
			{
				result = false;
				break;
			}

			while (cur < pkgSize && !m_end)
			{
				recvRet = socketRecv(m_rtspSocket, buff + cur, pkgSize - cur);
				if (recvRet <= 0)
				{
					m_end = true;
					result = false;
					break;
				}
				cur += recvRet;
			}
			if (cur != pkgSize)
			{
				result = false;
				break;
			}
			outSize = pkgSize;
			getPkg = true;
			break;
		case AWAITING_RTSP_END:
			if (recvRet == 1)
			{
				if (cur >= bufSize)
				{
					result = false;
					break;
				}
				buff[cur++] = tmp8;
				if (cur > 4)
				{
					if (buff)
					{
						if (buff[cur - 4] == '\r'&&buff[cur - 3] == '\n'&&
							buff[cur - 2] == '\r'&&buff[cur - 1] == '\n')
						{
							buff[cur] = '\0';

							getPkg = true;
							outSize = cur;
						}
					}
				}
			}
			break;
		default:
			break;
		}
		if (!result)
		{
			break;
		}
	}
	return getPkg;
}

void rtspClientSource::rtpProcessThread()
{
	int ret = 0;
	char buf[DEFAULT_MTU_2];
	while (!m_end)
	{
		//ret = socketRecv(m_rtpSocket, buf, DEFAULT_MTU_2);
		sockaddr_in	remoteAddr;
		remoteAddr.sin_family = AF_INET;
		remoteAddr.sin_port = htons(m_rtpPort);
		remoteAddr.sin_addr.s_addr = inet_addr(m_svrAddr.c_str());
		int nAddrlen = sizeof remoteAddr;
		ret = socketRecvfrom(m_rtpSocket, buf, DEFAULT_MTU_2, 0, (sockaddr*)&remoteAddr, nAddrlen);
		if (ret <= 0)
		{
			m_end = true;
			LOG_WARN(configData::getInstance().get_loggerId(), "rtp process thread recv failed");
			break;
		}
		if (!m_sessionList.size())
		{
			m_end = true;
			break;
		}
		switch (m_sessionList[0].m_payloadFmt)
		{
		case RTSP_payload_h264:
			addRtpDataToProcesser(buf, ret, m_sessionList[0].m_payloadFmt);
			break;
		default:
			break;
		}
	}
	closeRtspRtpRtcpSocket();
}

void rtspClientSource::rtcpProcessThread()
{
	int ret = 0;
	char buf[DEFAULT_MTU_2];
	while (!m_end)
	{
		//ret = socketRecv(m_rtcpSocket, buf, DEFAULT_MTU_2);
		sockaddr_in	remoteAddr;
		remoteAddr.sin_family = AF_INET;
		remoteAddr.sin_port = htons(m_rtcpPort);
		remoteAddr.sin_addr.s_addr = inet_addr(m_svrAddr.c_str());
		int nAddrlen = sizeof remoteAddr;
		ret = socketRecvfrom(m_rtpSocket, buf, DEFAULT_MTU_2, 0, (sockaddr*)&remoteAddr, nAddrlen);
		if (ret <= 0)
		{
			m_end = true;
			LOG_WARN(configData::getInstance().get_loggerId(), "rtcp process thread recv failed");
			break;
		}
		if (!m_sessionList.size())
		{
			m_end = true;
			break;
		}
		processRtcpData(buf, ret);
	}
	closeRtspRtpRtcpSocket();
}

void rtspClientSource::rtspHeartbeatThread()
{
	
	timeval timeNow;
	gettimeofday(&timeNow, 0);
	m_lastRtpRecvedTime = timeNow.tv_sec;
	while (!m_end)
	{
		//发送OPTIONS
		if (!sendOptionsWithSession())
		{
			LOG_WARN(configData::getInstance().get_loggerId(), "send options with session failed");
			m_end = true;
			break;
		}
		//检测上一个rtp包的时间，如果超过超时值，同样关闭连接
		
		gettimeofday(&timeNow, 0);
		if (timeNow.tv_sec - m_lastRtpRecvedTime > m_timeout/2)
		{
			LOG_WARN(configData::getInstance().get_loggerId(), "rtp pkg timeout ,no packet recved");
			m_end = true;
			break;
		}
		
		for (int  i = 0; i < m_timeout/2; i++)
		{
			std::this_thread::sleep_for(std::chrono::seconds(1));
			if (m_end)
			{
				break;
			}
			gettimeofday(&timeNow, 0);
			if (timeNow.tv_sec - m_lastRtpRecvedTime > m_timeout / 2)
			{
				LOG_WARN(configData::getInstance().get_loggerId(), "rtp pkg timeout ,no packet recved");
				m_end = true;
				break;
			}
		}
		if (m_end)
		{
			break;
		}
		sendRR();
	}
	closeRtspRtpRtcpSocket();
}

void rtspClientSource::rtpStatsUpdate(RTP_header & rtpHeader)
{
	m_rtpStats.rtp_received++;
	if (rtpHeader.seq > m_rtpStats.highest_seq)
	{
		m_rtpStats.highest_seq = rtpHeader.seq;
	}
	if (rtpHeader.seq == 65535)
	{
		m_rtpStats.rtp_cycles++;
	}
	//printf("%d\n,",rtp_header.seq);
	m_rtpStats.rtp_identifier = rtpHeader.ssrc;
	timeval	nowTime;
	gettimeofday(&nowTime, 0);
	if (m_rtpStats.last_recv_time.tv_sec == 0)
	{
		m_rtpStats.first_seq = rtpHeader.seq;
		gettimeofday(&m_rtpStats.last_recv_time, 0);
	}
	else
	{
		unsigned int transit = m_rtpStats.delay_snc_last_SR;
		int delta = transit - m_rtpStats.transit;
		m_rtpStats.transit = transit;
		if (delta < 0)
		{
			delta = -delta;
		}
		m_rtpStats.jitter += ((1.0 / 16.0)*((double)delta - m_rtpStats.jitter));
		m_rtpStats.rtp_ts = rtpHeader.ts;
	}
}

void rtspClientSource::sendRR()
{
	if (!m_rtpReceptionStats)
	{
		return;
	}
	//unsigned int  packet_size=32;
	unsigned int  packet_size = 56;
	unsigned int	tmp32;
	unsigned short	tmp16;
	unsigned char	tmp8;
	unsigned char framingHeader[4];

	framingHeader[0] = '$';
	framingHeader[1] = 1;
	framingHeader[2] = (unsigned char)((packet_size & 0xFF00) >> 8);
	framingHeader[3] = (unsigned char)((packet_size & 0xFF));
	if (m_sessionList.front().m_RAWRAWUDP)
	{
		
	}
	else
	{

		socketSend(m_rtspSocket, (char*)framingHeader, 4);
	}

	const int max_buf_size = 51200;
	unsigned char *pRRBuf = new unsigned char[max_buf_size];
	int	cur = 0;
	unsigned int rtcp_hdr = 0;

	int		num_report_block = 1;
	rtcp_hdr = 0x80000000;//version 2; pad:0
	rtcp_hdr |= (num_report_block << 24);//RC
	rtcp_hdr |= (RTCP_RR << 16);//PT
	rtcp_hdr |= (1 + 6 * num_report_block);
	rtcp_hdr = htonl(rtcp_hdr);
	memcpy(&pRRBuf[cur], &rtcp_hdr, sizeof rtcp_hdr);
	cur += 4;
	unsigned int ssrc_sender = m_ssrcRR;
	tmp32 = htonl(ssrc_sender);
	memcpy(&pRRBuf[cur], &tmp32, sizeof tmp32);
	cur += 4;

	//report block;
	//ssrc
	unsigned int ssrc_rtp = m_rtpReceptionStats->SSRC();
	tmp32 = htonl(ssrc_rtp);
	memcpy(&pRRBuf[cur], &tmp32, sizeof tmp32);
	cur += 4;

	unsigned highestExtSeqNumReceived = m_rtpReceptionStats->highestExtSeqNumReceived();

	unsigned totNumExpected
		= highestExtSeqNumReceived - m_rtpReceptionStats->baseExtSeqNumReceived();
	int totNumLost = totNumExpected - m_rtpReceptionStats->totalNumPacketsReceived();
	// 'Clamp' this loss number to a 24-bit signed value:
	if (totNumLost > 0x007FFFFF) {
		totNumLost = 0x007FFFFF;
	}
	else if (totNumLost < 0) {
		if (totNumLost < -0x00800000) totNumLost = 0x00800000; // unlikely, but...
		totNumLost &= 0x00FFFFFF;
	}

	unsigned numExpectedSinceLastReset
		= highestExtSeqNumReceived - m_rtpReceptionStats->lastResetExtSeqNumReceived();
	int numLostSinceLastReset
		= numExpectedSinceLastReset - m_rtpReceptionStats->numPacketsReceivedSinceLastReset();
	unsigned char lossFraction;
	if (numExpectedSinceLastReset == 0 || numLostSinceLastReset < 0) {
		lossFraction = 0;
	}
	else {
		lossFraction = (unsigned char)
			((numLostSinceLastReset << 8) / numExpectedSinceLastReset);
	}
	//unsigned int tmp32;

	tmp32 = htonl(((lossFraction << 24) | totNumLost));
	memcpy(&pRRBuf[cur], &tmp32, sizeof tmp32);
	cur += 4;
	tmp32 = htonl(highestExtSeqNumReceived);
	memcpy(&pRRBuf[cur], &tmp32, sizeof tmp32);
	cur += 4;

	//tmp32=htonl(g_rtp_reception_stats->jitter());
	tmp32 = htonl(0.0);
	memcpy(&pRRBuf[cur], &tmp32, sizeof tmp32);
	cur += 4;

	unsigned NTPmsw = m_rtpReceptionStats->lastReceivedSR_NTPmsw();
	unsigned NTPlsw = m_rtpReceptionStats->lastReceivedSR_NTPlsw();
	unsigned LSR = ((NTPmsw & 0xFFFF) << 16) | (NTPlsw >> 16); // middle 32 bits
	tmp32 = htonl(LSR);
	memcpy(&pRRBuf[cur], &tmp32, sizeof tmp32);
	cur += 4;

	// Figure out how long has elapsed since the last SR rcvd from this src:
	struct timeval const& LSRtime = m_rtpReceptionStats->lastReceivedSR_time(); // "last SR"
	struct timeval timeNow, timeSinceLSR;
	gettimeofday(&timeNow, NULL);
	if (timeNow.tv_usec < LSRtime.tv_usec) {
		timeNow.tv_usec += 1000000;
		timeNow.tv_sec -= 1;
	}
	timeSinceLSR.tv_sec = timeNow.tv_sec - LSRtime.tv_sec;
	timeSinceLSR.tv_usec = timeNow.tv_usec - LSRtime.tv_usec;
	// The enqueued time is in units of 1/65536 seconds.
	// (Note that 65536/1000000 == 1024/15625)
	unsigned DLSR;
	if (LSR == 0) {
		DLSR = 0;
	}
	else {
		DLSR = (timeSinceLSR.tv_sec << 16)
			| ((((timeSinceLSR.tv_usec << 11) + 15625) / 31250) & 0xFFFF);
	}
	tmp32 = htonl(DLSR);
	memcpy(&pRRBuf[cur], &tmp32, sizeof tmp32);
	cur += 4;
	//////////////////////////////////////////////////////////////////////////

	//SDES
	unsigned int numBytes = 4;
	int name_len = 0;
	char* CNAME = m_rtpReceptionStats->CNAME(name_len);
	numBytes += name_len;
	numBytes += 1;

	rtcp_hdr = 0x81000000;
	rtcp_hdr |= (RTCP_SDES << 16);
	rtcp_hdr |= numBytes;
	tmp32 = htonl(rtcp_hdr);
	memcpy(&pRRBuf[cur], &tmp32, sizeof tmp32);
	cur += 4;

	tmp32 = htonl(m_ssrcRR);
	memcpy(&pRRBuf[cur], &tmp32, sizeof tmp32);
	cur += 4;

	memcpy(&pRRBuf[cur], CNAME, name_len);
	cur += name_len;
	unsigned numPaddingBytesNeeded = 4 - (cur % 4);
	unsigned char const zero = '\0';
	while (numPaddingBytesNeeded-- > 0)
	{
		memcpy(&pRRBuf[cur], &zero, 1);
		cur++;
	}
	//////////////////////////////////////////////////////////////////////////
	if (m_sessionList.front().m_RAWRAWUDP)
	{
		sockaddr_in	remoteAddr;
		remoteAddr.sin_family = AF_INET;
		remoteAddr.sin_port = htons(m_rtpSvrPort);
		remoteAddr.sin_addr.s_addr = inet_addr(m_svrAddr.c_str());
		int nAddrlen = sizeof remoteAddr;
		int iRet = sendto(m_rtpSocket, (const char*)pRRBuf, cur, 0, (sockaddr*)&remoteAddr, nAddrlen);
		if (iRet <= 0)
		{
			LOG_WARN(configData::getInstance().get_loggerId(), "net peretrate failed:" << socketError());
		}
	}
	else
	{

		int sendResult = socketSend(m_rtspSocket, (char*)pRRBuf, cur);
		if (sendResult<cur)
		{
			sendResult = -1;
		}
	}

	m_rtpReceptionStats->resetStats();
	delete[]pRRBuf;
}

void rtspClientSource::addRtpDataToProcesser(const char * data, int size, int type)
{
	std::lock_guard<std::mutex> guard(m_processerMutex);
	if (!data || size <= 0)
	{
		LOG_ERROR(configData::getInstance().get_loggerId(), "why input empty data?");
		return;
	}
	CBitReader	bitReader;
	int rtpDataSize = 0;
	int rtpPadSize = 0;
	bool getFrameEnd = false;
	unsigned long rtpTimeMs = 0;

	static bool bool1 = false, bool2 = false;
	if (bool1==false)
	{
		LOG_INFO(configData::getInstance().get_loggerId(), "get first frame start");
		bool1 = true;
	}
	if (0 != bitReader.SetBitReader((unsigned char*)data, size, true))
	{
		LOG_ERROR(configData::getInstance().get_loggerId(), "bitreader failed,maybe size too big:" << size);
		return;
	}
	//发送直播开始通知
	/*std::once_flag oc;
	std::call_once(oc, std::mem_fun(&rtspClientSource::notifyTaskStarted), this);*/
	if (!m_notifyStarted)
	{
		m_notifyStarted = true;
		notifyLiveStarted();
	}
	switch (type)
	{
	case RTSP_payload_h264:
		//读取有效的rtp数据，更新rtp状态，如果得到帧结束，发送给所有的sinker
		m_rtpHeader.version = bitReader.ReadBits(2);
		m_rtpHeader.padding = bitReader.ReadBit();
		m_rtpHeader.extension = bitReader.ReadBit();
		m_rtpHeader.cc = bitReader.ReadBits(4);
		m_rtpHeader.marker = bitReader.ReadBit();
		m_rtpHeader.payload_type = bitReader.ReadBits(7);
		m_rtpHeader.seq = bitReader.ReadBits(16);
		m_rtpHeader.ts = bitReader.Read32Bits();
		m_rtpHeader.ssrc = bitReader.Read32Bits();
		//LOG_DEBUG(configData::getInstance().get_loggerId(), "ssrc:" << m_rtpHeader.seq);
		for (int i = 0; i < m_rtpHeader.cc; i++)
		{
			//m_rtpHeader.csrc[i] = m_BitReader.Read32Bits();
			bitReader.Read32Bits();
		}
		if (m_rtpHeader.extension)
		{
			LOG_WARN(configData::getInstance().get_loggerId(), "rtp extension ? recv rtp header failed");
			RTP_header_extension extension;
			extension.profile = bitReader.ReadBits(16);
			extension.length = bitReader.ReadBits(16);
			bitReader.IgnoreBytes(extension.length);
		}
		if (m_rtpHeader.version != 2 || m_rtpHeader.payload_type != RTSP_payload_h264)
		{
			LOG_WARN(configData::getInstance().get_loggerId(), "invalid rtp data");
			break;
		}
		if (m_rtpHeader.padding)
		{
			rtpPadSize = data[size - 1];
		}
		rtpDataSize = size - bitReader.CurrentByteCount() - rtpPadSize;
		if (!m_rtpReceptionStats)
		{
			m_rtpReceptionStats = new RTPReceptionStats(m_rtpHeader.ssrc, m_rtpHeader.seq);
		}
		timeval resultPresentTime;
		bool	resultHasBeenSyncedUsingRTCP;
		m_rtpReceptionStats->noteIncomingPacket(m_rtpHeader.seq,
			m_rtpHeader.ts,
			m_sessionList.front().m_uRtpTimestampFrequency,
			true,
			resultPresentTime,
			resultHasBeenSyncedUsingRTCP, 0);
		//rtpStatsUpdate(m_rtpHeader);

		// 处理RTP更新for RTCP
		//!处理RTP更新for RTCP
		//将rtp时间戳改变为毫秒
		rtpTimeMs=m_rtpHeader.ts/90;
		//!将rtp时间戳改变为毫秒
		getFrameEnd = false;
		parseH264Payload(m_rtpDataForProcesser, (const char*)bitReader.CurrentByteData(), rtpDataSize, getFrameEnd);
		if (getFrameEnd)
		{
			if (bool2 == false)
			{
				LOG_INFO(configData::getInstance().get_loggerId(), "get first frame end");
				bool2 = true;
			}
			std::lock_guard<std::mutex> guard_sinker(m_sinkerMutex);
			for (auto i:m_rtpDataForProcesser)
			{
				int	nal_type = (i.data[0] & 0x1f);
				if (nal_type == 5)
				{
					m_keyframeMutex.lock();
					if (m_keyframe.data)
					{
						delete[]m_keyframe.data;
					}
					m_keyframe.size = i.size;
					m_keyframe.data = new char[m_keyframe.size];
					memcpy(m_keyframe.data, i.data, i.size);
					m_keyframeMutex.unlock();
				}
			}
			for (auto i : m_sinkerList)
			{
				streamSinker *sinker = (streamSinker*)i;
				if (m_rtpDataForProcesserTimeOffset.size()>0)
				{
					for (int j = 0; j < m_rtpDataForProcesserTimeOffset.size(); j++)
					{
						timedPacket tp;
						tp.data.push_back(m_rtpDataForProcesser[j]);
						tp.packetType = raw_h264_frame;
						tp.timestamp = rtpTimeMs+m_rtpDataForProcesserTimeOffset[j];
						sinker->addTimedDataPacket(tp);
					}
				}
				else
				{
					for (auto j : m_rtpDataForProcesser)
					{
						//sinker->addDataPacket(j);
						//判读帧类型

						timedPacket tp;
						tp.data.push_back(j);
						tp.packetType = raw_h264_frame;
						tp.timestamp = rtpTimeMs;
						sinker->addTimedDataPacket(tp);
					}
				}
			}
			for (auto i : m_rtpDataForProcesser)
			{
				delete[]i.data;
			}
			m_rtpDataForProcesser.clear();
			m_rtpDataForProcesserTimeOffset.clear();
			//更新rtp包时间
			timeval timeNow;
			gettimeofday(&timeNow, 0);
			m_lastRtpRecvedTime = timeNow.tv_sec;
		}
		break;
	default:
		LOG_WARN(configData::getInstance().get_loggerId(), "payload type:" << type << " may not processer now");
		break;
	}
}

void rtspClientSource::processRtcpData(const char * data, int size)
{
	CBitReader bitReader;
	bitReader.SetBitReader((unsigned char*)data, size, true);
	if (size < sizeof(m_rtcpHeader))
	{
		return;
	}
	m_rtcpHeader.version = bitReader.ReadBits(2);
	m_rtcpHeader.padding = bitReader.ReadBit();
	m_rtcpHeader.rc = bitReader.ReadBits(5);
	m_rtcpHeader.pt = bitReader.ReadBits(8);
	m_rtcpHeader.length = bitReader.ReadBits(16);
	switch (m_rtcpHeader.pt)
	{
	case RTCP_SR:
		m_RTCP_header_SR.ssrc = bitReader.ReadBits(32);
		m_RTCP_header_SR.ntp_timestamp_MSW = bitReader.Read32Bits();
		m_RTCP_header_SR.ntp_timestamp_LSW = bitReader.Read32Bits();
		m_RTCP_header_SR.rtp_timestamp = bitReader.Read32Bits();
		m_RTCP_header_SR.pkt_count = bitReader.Read32Bits();
		m_RTCP_header_SR.octet_count = bitReader.Read32Bits();
		if (m_rtpReceptionStats == 0)
		{
			m_rtpReceptionStats = new RTPReceptionStats(m_RTCP_header_SR.ssrc);
		}
		m_rtpReceptionStats->noteIncomingSR(m_RTCP_header_SR.ntp_timestamp_MSW,
			m_RTCP_header_SR.ntp_timestamp_LSW, m_RTCP_header_SR.rtp_timestamp);
		//发送RTCP报告
		sendRR();
		break;
	default:
		break;
	}
}

void rtspClientSource::parseH264Payload(std::vector<DataPacket>& rtpData, const char * data, int size, bool & getFrameEnd)
{
	CBitReader bitReader;
	bitReader.SetBitReader((unsigned char*)data, size, true);
	do
	{
		int nal_forbidden_zero = bitReader.ReadBit();
		if (nal_forbidden_zero != 0)
		{
			LOG_ERROR(configData::getInstance().get_loggerId(), "nal forbidden zero is not zero:" << nal_forbidden_zero);
			break;
		}
		int nal_nri = bitReader.ReadBits(2);
		int nal_type = bitReader.ReadBits(5);
		//RFC 3984
		if (nal_type >= NAL_TYPE_SINGLE_NAL_MIN&&
			nal_type <= NAL_TYPE_SINGLE_NAL_MAX)
		{
			//单一包,直接拷贝数据
			for (auto i : rtpData)
			{
				delete[]i.data;
			}
			rtpData.clear();
			DataPacket singlePkt;
			singlePkt.size = size;
			singlePkt.data = new char[size];
			memcpy(singlePkt.data, data, size);
			rtpData.push_back(singlePkt);
			getFrameEnd = true;
			break;
		}
		else if (nal_type == NAL_TYPE_STAP_A)
		{
			const char *q = nullptr;
			unsigned short nalu_size = 0;
			int index = 1;
			q = data;

			for (auto i : rtpData)
			{
				delete[]i.data;
			}
			rtpData.clear();

			while (index < size)
			{
				//nalu_size = ((q[index] << 8) | (q[index + 1]));
				memcpy(&nalu_size, q + index, 2);
				nalu_size = ntohs(nalu_size);
				index += 2;
				if (nalu_size == 0)
				{
					index++;
					continue;
				}
				if (size - index < nalu_size)
				{
					LOG_WARN(configData::getInstance().get_loggerId(), "size out of range");
					break;
				}
				DataPacket stapA;
				stapA.size = nalu_size;
				stapA.data = new char[nalu_size];
				memcpy(stapA.data, q + index, nalu_size);
				rtpData.push_back(stapA);
				index += nalu_size;
			}
			getFrameEnd = true;
			break;
		}
		else if (nal_type == NAL_TYPE_STAP_B)
		{

			//std::once_flag oc;
			//std::call_once(oc, std::mem_fun(&rtspClientSource::warnOnce), this, "NAL_TYPE_STAP_B not tested");
			const char *q = nullptr;
			unsigned short nalu_size = 0;
			unsigned short don = 0;
			int index = 1;
			q = data;

			for (auto i : rtpData)
			{
				delete[]i.data;
			}
			rtpData.clear();

			//don
			memcpy(&don, q + 1, 2);
			don = ntohs(don);
			index += 2;

			while (index < size)
			{
				//nalu_size = ((q[index] << 8) | (q[index + 1]));
				memcpy(&nalu_size, q + index, 2);
				nalu_size = ntohs(nalu_size);
				index += 2;
				if (nalu_size == 0)
				{
					index++;
					continue;
				}
				if (size - index < nalu_size)
				{
					LOG_WARN(configData::getInstance().get_loggerId(), "size out of range");
					break;
				}
				DataPacket stapA;
				stapA.size = nalu_size;
				stapA.data = new char[nalu_size];
				memcpy(stapA.data, q + index, nalu_size);
				rtpData.push_back(stapA);
				index += nalu_size;
			}
			getFrameEnd = true;
			break;
		}
		else if (nal_type == NAL_TYPE_MTAP16)
		{
			//LOG_WARN(configData::getInstance().get_loggerId(), "not process rtp nal type" << nal_type);

			/*std::once_flag oc;
			std::call_once(oc, std::mem_fun(&rtspClientSource::warnOnce), this, "NAL_TYPE_MTAP16 not tested");*/
			const char *q = nullptr;
			unsigned short nalu_size = 0;
			unsigned short don = 0;
			unsigned short dob = 0;
			unsigned short dond = 0;
			unsigned short ts;
			int index = 1;
			q = data;

			for (auto i : rtpData)
			{
				delete[]i.data;
			}
			rtpData.clear();
			m_rtpDataForProcesserTimeOffset.clear();

			dob = bitReader.ReadBits(16);
			index += 2;
			while (index < size)
			{
				memcpy(&nalu_size, q + index, 2);
				nalu_size = ntohs(nalu_size);
				index += 2;
				memcpy(&dond, q + index, 1);
				dond = ntohs(dond);
				index += 1;
				memcpy(&ts, q + index, 2);
				ts = ntohs(ts);
				index += 2;

				if (size - index < nalu_size)
				{
					LOG_WARN(configData::getInstance().get_loggerId(), "size out of range");
					break;
				}
				DataPacket mtap16;
				mtap16.size = nalu_size;
				mtap16.data = new char[nalu_size];
				memcpy(mtap16.data, q + index, nalu_size);
				rtpData.push_back(mtap16);
				m_rtpDataForProcesserTimeOffset.push_back(ts);
				index += nalu_size;
			}
			break;
		}
		else if (nal_type == NAL_TYPE_MTAP24)
		{
			//LOG_WARN(configData::getInstance().get_loggerId(), "not process rtp nal type" << nal_type);

			/*std::once_flag oc;
			std::call_once(oc, std::mem_fun(&rtspClientSource::warnOnce), this, "NAL_TYPE_MTAP24 not tested");*/
			const char *q = nullptr;
			unsigned short nalu_size = 0;
			unsigned short don = 0;
			unsigned short dob = 0;
			unsigned short dond = 0;
			unsigned int ts;
			int index = 1;
			q = data;

			for (auto i : rtpData)
			{
				delete[]i.data;
			}
			rtpData.clear();
			m_rtpDataForProcesserTimeOffset.clear();

			dob = bitReader.ReadBits(16);
			index += 2;
			while (index < size)
			{
				memcpy(&nalu_size, q + index, 2);
				nalu_size = ntohs(nalu_size);
				index += 2;
				memcpy(&dond, q + index, 1);
				dond = ntohs(dond);
				index += 1;

				CBitReader tsReader;
				tsReader.SetBitReader((unsigned char*)data + index, size - index, true);
				ts = tsReader.ReadBits(24);
				index += 3;

				DataPacket mtap16;
				if (size - index < nalu_size)
				{
					LOG_WARN(configData::getInstance().get_loggerId(), "size out of range");
					break;
				}
				mtap16.size = nalu_size;
				mtap16.data = new char[nalu_size];
				memcpy(mtap16.data, q + index, nalu_size);
				rtpData.push_back(mtap16);
				m_rtpDataForProcesserTimeOffset.push_back(ts);
				index += nalu_size;
			}
			break;
		}
		else if (nal_type == NAL_TYPE_FU_A)
		{
			const char *pData = data;

			unsigned char h264_start_bit = pData[1] & 0x80;
			unsigned char h264_end_bit = pData[1] & 0x40;
			unsigned char h264_type = pData[1] & 0x1F;
			unsigned char h264_nri = (pData[0] & 0x60) >> 5;
			unsigned char h264_key = (h264_nri << 5) | h264_type;
			if (h264_start_bit)
			{

			}
			getFrameEnd = false;
			if (h264_end_bit)
			{
				getFrameEnd = true;
			}
			if (h264_start_bit)
			{
				for (auto i : rtpData)
				{
					delete[]i.data;
				}
				rtpData.clear();
				DataPacket startPkt;
				startPkt.size = 1;
				startPkt.data = new char[1];
				startPkt.data[0] = h264_key;
				rtpData.push_back(startPkt);
			}
			if (rtpData.size() == 0)
			{
				//LOG_WARN(configData::getInstance().get_loggerId(), "lost udp frame");
				break;
			}
			int totalSize = size - 2 + rtpData.front().size;

			char *tmpBuf = new char[totalSize];
			memcpy(tmpBuf, rtpData.front().data, rtpData.front().size);
			memcpy(tmpBuf + rtpData.front().size, data + 2, size - 2);
			delete[]rtpData.front().data;
			rtpData.front().data = tmpBuf;
			rtpData.front().size = totalSize;
		}
		else if (nal_type == NAL_TYPE_FU_B)
		{
			//std::call_once()
			/*std::once_flag oc;
			std::call_once(oc, std::mem_fun(&rtspClientSource::warnOnce), this, "NAL_TYPE_FU_B not tested");*/
			const char *pData = data;

			unsigned char h264_start_bit = pData[1] & 0x80;
			unsigned char h264_end_bit = pData[1] & 0x40;
			unsigned char h264_type = pData[1] & 0x1F;
			unsigned char h264_nri = (pData[0] & 0x60) >> 5;
			unsigned char h264_key = (h264_nri << 5) | h264_type;
			unsigned short don = 0;
			if (h264_start_bit)
			{

			}
			getFrameEnd = false;
			if (h264_end_bit)
			{
				getFrameEnd = true;
			}
			if (h264_start_bit)
			{
				for (auto i : rtpData)
				{
					delete[]i.data;
				}
				rtpData.clear();
				DataPacket startPkt;
				startPkt.size = 1;
				startPkt.data = new char[1];
				startPkt.data[0] = h264_key;
				rtpData.push_back(startPkt);
			}
			if (rtpData.size() == 0)
			{
				//LOG_WARN(configData::getInstance().get_loggerId(), "lost udp frame");
				break;
			}
			//FU indicator=1 FU header=1 DON=2
			memcpy(&don, data + 2, 2);
			don = ntohs(don);
			int prefSize = 1 + 1 + 2;
			int totalSize = size - prefSize + rtpData.front().size;
			char *tmpBuf = new char[totalSize];
			memcpy(tmpBuf, rtpData.front().data, rtpData.front().size);
			memcpy(tmpBuf + rtpData.front().size, data + prefSize, size - prefSize);
			delete[]rtpData.front().data;
			rtpData.front().data = tmpBuf;
			rtpData.front().size = totalSize;
			break;
		}
	} while (0);
}

void rtspClientSource::warnOnce(std::string logData)
{
	LOG_WARN(configData::getInstance().get_loggerId(), logData);
}

void rtspClientSource::closeRtspRtpRtcpSocket()
{
	if (m_rtspSocket >= 0&&m_rtspSocket!=-1)
	{
		closeSocket(m_rtspSocket);
		m_rtspSocket = -1;
	}
	if (m_rtpSocket&&m_rtpSocket!=-1)
	{
		closeSocket(m_rtpSocket);
		m_rtpSocket = -1;
	}
	if (m_rtcpSocket&&m_rtcpSocket!=-1)
	{
		closeSocket(m_rtcpSocket);
		m_rtcpSocket = -1;
	}
}

void rtspClientSource::notifyLiveStarted()
{
	if (m_taskStruct.liveRTSP)
	{
		taskJsonStruct taskStart;
		taskStart.targetProtocol = taskJsonProtocol_RTSP;
		timeval timeBegin;
		gettimeofday(&timeBegin, 0);
		m_taskStruct.timeBegin = taskStart.timeBegin = timeBegin.tv_sec;
		taskStart.userdefString = m_taskStruct.userdefString;
		taskStart.taskId = m_taskStruct.taskId;
		taskStart.taskType = taskJson::getInstance().getOperatorType(em_operationType::opt_type_livingStart);
		//taskStart.targetAddress = get_server_ip();
		char addr[1024];
		sprintf(addr, "rtsp://%s:%d/live/%s", get_server_ip(), configData::getInstance().RTSP_server_port(),
			m_taskStruct.taskId.c_str());
		m_taskStruct.targetAddress=taskStart.targetAddress = addr;
		std::string taskString;
		if (taskJson::getInstance().json2String(taskStart, taskString))
		{
			unsigned int length = htonl(taskString.size());
			/*char *buf = new char[2 + taskString.size()];
			memcpy(buf, &length, 2);
			memcpy(buf + 2, taskString.c_str(), taskString.size());
			if (socketSend(m_jsonSocket, buf, taskString.size() + 2) <= 0)
			{
				LOG_WARN(configData::getInstance().get_loggerId(), "send status failed");
			}
			delete[]buf;*/
			
			/*socketSend(m_jsonSocket,(const char*) &length, 2);
			socketSend(m_jsonSocket, (const char*)taskString.c_str(), taskString.size());*/
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

void rtspClientSource::notifyLiveEnded()
{

	//拉流结束，发送直播结束Json给客户端
	if (m_taskStruct.liveRTSP)
	{
		taskJsonStruct taskEnd;
		taskEnd.targetProtocol = taskJsonProtocol_RTSP;
		taskEnd.taskId = m_taskStruct.taskId;
		taskEnd.taskType = taskJson::getInstance().getOperatorType(em_operationType::opt_type_livingEnd);
		taskEnd.userdefString = m_taskStruct.userdefString;
		taskEnd.timeBegin = m_taskStruct.timeBegin;
		char addr[1024];
		sprintf(addr, "RTSP://%s:%d/live/%s", get_server_ip(), configData::getInstance().RTSP_server_port(),
			m_taskStruct.taskId.c_str());
		taskEnd.targetAddress = addr;
		timeval timeEnd;
		gettimeofday(&timeEnd, 0);
		taskEnd.timeEnd = timeEnd.tv_sec;
		std::string taskString;
		if (taskJson::getInstance().json2String(taskEnd, taskString))
		{
			unsigned int length = htonl(taskString.size());
			
			/*socketSend(m_jsonSocket, (const char*)&length, 2);
			socketSend(m_jsonSocket, (const char*)taskString.c_str(), taskString.size());*/
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
	//通知所有的sinker，任务结束，让sinker自身去处理删除事务
	m_sinkerMutex.lock();
	for (auto i:m_sinkerList )
	{
		streamSinker *sinker = (streamSinker*)i;
		sinker->sourceRemoved();
	}
	m_sinkerMutex.unlock();
}

rtspSession::rtspSession() :m_uRtpTimestampFrequency(0), m_packetization_mode(0),
m_sps(0), m_pps(0), m_sps_len(0), m_pps_len(0), m_RAWRAWUDP(false)
{
}

rtspSession::~rtspSession()
{
	if (m_sps)
	{
		delete[]m_sps;
		m_sps = nullptr;
	}
	if (m_pps)
	{
		delete[]m_pps;
		m_pps = nullptr;
	}
}

bool rtspSession::generateSdp()
{
	bool result = true;
	do
	{
		LOG_INFO(configData::getInstance().get_loggerId(), "the sdp only support fmt 96 H264 now");
		const char * media_type = "video";
		unsigned char rtp_payload_type = m_payloadFmt;
		const char *ip_address = "0.0.0.0";
		const char *rtpmap_line = "a=rtpmap:96 H264/90000\r\n";
		const char *rtcpmux_line = "";
		//const char *range_line = "";
		const char *range_line = "a=range:npt=0-\r\n";
		const char *fmtplines=
			"a=fmtp:96 packetization-mode=1;"
			"\r\n";
		char sdp_lines[2048];
		//a=control是音视频控制频道，如果只有视频则只有一个，如果音视频都有则有两个，
		//一个连接只能发一个频道的数据，如果要同时收音视频，需要两个连接
		const char *streamId = RTSP_CONTROL_ID_0;
		unsigned int sdp_fmt_leng = 0;
		char const* const sdp_fmt =
			"m=%s %u RTP/AVP %d\r\n"
			"c=IN IP4 %s\r\n"
			"b=AS:%u\r\n"
			"%s"
			"%s"
			"%s"
			"%s"
			"a=control:%s\r\n";
		sdp_fmt_leng += strlen(sdp_fmt) +
			strlen(media_type) + 8 +
			strlen(ip_address) +
			20 +
			strlen(rtpmap_line) +
			strlen(rtcpmux_line) +
			strlen(range_line) +
			strlen(fmtplines) +
			strlen(streamId);
		sprintf(sdp_lines,
			sdp_fmt,
			media_type,
			0,
			rtp_payload_type,
			ip_address,
			500,
			rtpmap_line,
			rtcpmux_line,
			range_line,
			fmtplines,
			streamId);
		//
		unsigned int sdp_length = 0;
		sdp_length += strlen(sdp_lines);
		//just set duration 0 now;
		//const char *range_line = "a=range:npt=0-\r\n";
		char descr[RTSP_BUFFER_SIZE];
		char *pIp = const_cast<char*>(get_server_ip());
		const char *description_sdp_string = "H.264 Video,streamed by RTSP server";
		const char *server_name = configData::getInstance().get_server_name();
		const char *server_version = configData::getInstance().get_server_version();
		const char *source_filter_line = "";
		const char *misc_sdp_lines = "";
		timeval time_now;
		gettimeofday(&time_now, 0);

		char const* const sdp_prefix_fmt =
			"v=0\r\n"
			"o=- %ld%06ld %d IN IP4 %s\r\n"
			"s=%s\r\n"
			"i=%s\r\n"
			"t=0 0\r\n"
			"a=tool:%s%s\r\n"
			"a=type:broadcast\r\n"
			"a=control:*\r\n"
			"%s"
			"%s"
			"a=x-qt-text-nam:%s\r\n"
			"a=x-qt-text-inf:%s\r\n"
			"%s";
		sdp_length += strlen(sdp_prefix_fmt) +
			46 + strlen(pIp) +
			strlen(range_line) +
			strlen(description_sdp_string) * 2 +
			strlen(server_name) * 2 +
			strlen(server_version) +
			strlen(source_filter_line) +
			strlen(misc_sdp_lines);
		sprintf(descr, sdp_prefix_fmt,
			time_now.tv_sec, time_now.tv_usec, 1, pIp,
			description_sdp_string,
			m_control.c_str(),
			server_name,
			server_version,
			source_filter_line,
			range_line,
			description_sdp_string,
			m_control.c_str(),
			sdp_lines);
		m_sdp = descr;
	} while (0);
	return result;
}

rtspSession::rtspSession(const rtspSession &R)
{
	this->m_strCodecName = R.m_strCodecName;
	this->m_uRtpTimestampFrequency = R.m_uRtpTimestampFrequency;
	this->m_packetization_mode = R.m_packetization_mode;
	this->m_profile_level_id = R.m_profile_level_id;
	this->m_sps_base64 = R.m_sps_base64;
	this->m_pps_base64 = R.m_pps_base64;
	this->m_control = R.m_control;
	this->m_RAWRAWUDP = R.m_RAWRAWUDP;
	this->m_sps_len = R.m_sps_len;
	this->m_pps_len = R.m_pps_len;
	this->m_sps = nullptr;
	this->m_pps = nullptr;

	this->m_payloadFmt = R.m_payloadFmt;

	if (this->m_sps_len > 0 && R.m_sps)
	{
		this->m_sps = new unsigned char[this->m_sps_len];
		memcpy(this->m_sps, R.m_sps, this->m_sps_len);
	}
	if (this->m_pps_len > 0 && R.m_pps)
	{
		this->m_pps = new unsigned char[this->m_pps_len];
		memcpy(this->m_pps, R.m_pps, this->m_pps_len);
	}
}
