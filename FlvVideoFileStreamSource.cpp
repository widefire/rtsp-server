#include "FlvVideoFileStreamSource.h"
#include "configData.h"
#include "taskSchedule.h"
#include "task_stream_stop.h"
#include "task_stream_file.h"
#include "task_RTCP_message.h"
#include "taskSendRtspResbData.h"
#include "h264FileParser.h"
#include "util.h"


FlvVideoFileStreamSource::FlvVideoFileStreamSource(const char *file_name, LP_RTSP_session session) :
	m_fileInited(false), m_IdxExist(false), m_hasCalledDelete(false), m_flvParser(nullptr),
	m_flvIdxReader(nullptr), m_session(nullptr), m_RTSP_socket_id(0), m_rtp_socket(0),
	m_rtcp_socket(0), m_rtcp_ssrc(0), m_rtp_pkg_counts(0), m_rtp_data_size(0),
	m_seq(0), m_seq_num(0), m_sendEnd(false), m_lastRtpTime(0)
{
	std::string fullName = configData::getInstance().getFlvSaveDir() + file_name;
	m_session = deepcopy_RTSP_session(session);
	m_flvParser = new flvParser(fullName);
	m_fileInited = m_flvParser->inited();
	std::string idxName = fullName + idxStuff;
	m_flvIdxReader = new flvIdxReader();
	m_IdxExist = m_flvIdxReader->initReader(idxName);

}


FlvVideoFileStreamSource::~FlvVideoFileStreamSource()
{
	m_hasCalledDelete = true;
	shutdown();
}

int FlvVideoFileStreamSource::handTask(taskScheduleObj * task_obj)
{
	int iRet = 0;
	do
	{
		if (0 == task_obj)
		{
			iRet = -1;
			break;
		}
		task_schedule_type task_type = task_obj->getType();
		taskStreamStop *task_stream_stop = nullptr;
		taskStreamPause *task_stream_pause = nullptr;
		taskRtcpMessage *task_RTCP_message = nullptr;
		taskSendRtspResbData *taskSendRtspData = nullptr;

		switch (task_type)
		{
		case schedule_start_rtsp_file:
			iRet = startStream(task_obj);
			break;
		case schedule_stop_rtsp_by_socket:
			task_stream_stop = static_cast<taskStreamStop*>(task_obj);
			if (task_stream_stop->getSocketId() == m_RTSP_socket_id)
			{

				iRet = stopStream(task_obj);
				break;
			}
			else
			{
				iRet = -1;
				break;
			}
			break;
		case schedule_stop_rtsp_by_name:
			task_stream_stop = static_cast<taskStreamStop*>(task_obj);
			if (task_stream_stop->getSessionName() == m_session->session_id)
			{
				iRet = stopStream(task_obj);
				break;
			}
			else
			{
				iRet = -1;
				break;
			}
			break;
		case schedule_pause_rtsp_file:
			task_stream_pause = static_cast<taskStreamPause*>(task_obj);
			if (task_stream_pause->getSessionName() == m_session->session_id)
			{
				m_hasCalledDelete = true;
				m_sendEnd = true;
				if (m_sendThread.joinable())
				{
					m_sendThread.join();
				}
				m_sendEnd = false;
				m_hasCalledDelete = false;

				iRet = 0;
				break;
			}
			else
			{
				iRet = -1;
				break;
			}
			break;
			break;
		case schedule_rtcp_data:
			task_RTCP_message = static_cast<taskRtcpMessage*>(task_obj);
			RTCP_process *rtcp_process;
			rtcp_process = task_RTCP_message->get_RTCP_message();
			if (!rtcp_process)
			{
				iRet = -1;
				break;
			}
			m_rtcp_ssrc = rtcp_process->RTCP_SSRC();
			if (rtcp_process->RTP_ssrc(0) == m_session->ssrc)
			{
				iRet = 0;
				break;
			}
			else
			{
				iRet = -1;
				break;
			}
			break;
		case schedule_send_rtsp_data:
			taskSendRtspData = static_cast<taskSendRtspResbData*>(task_obj);
			if (taskSendRtspData->getSessionId() != m_session->session_id)
			{
				iRet = -1;
				break;
			}
			m_mutexRtspSend.lock();
			m_vectorRtspSendData.push_back(taskSendRtspData->getDataSend());
			m_mutexRtspSend.unlock();
			break;
		default:
			iRet = -1;
			break;
		}
	} while (0);
	return iRet;
}

int FlvVideoFileStreamSource::startStream(taskScheduleObj * task_obj)
{
	int iRet = 0;
	do
	{
		m_hasCalledDelete = false;
		if (!m_fileInited || task_obj->getType() != task_schedule_type::schedule_start_rtsp_file)
		{
			iRet = -1;
			break;
		}

		taskStreamFile *task_real = static_cast<taskStreamFile*>(task_obj);
		if (!m_session || m_session->session_id != task_real->get_RTSP_session()->session_id ||
			m_session->ssrc != task_real->get_RTSP_session()->ssrc)
		{
			iRet = -1;
			break;
		}
		m_RTSP_socket_id = task_real->get_RTSP_buffer()->sock_id;
		memcpy(&m_sock_addr, &task_real->get_RTSP_buffer()->sock_addr, sizeof m_sock_addr);
		if (task_real->get_RTSP_buffer()->session_list.size() <= 0)
		{
			iRet = -1;
			break;
		}
		/*if (m_session)
		{
			delete m_session;
			m_session = nullptr;
		}
		m_session = deepcopy_RTSP_session(task_real->get_RTSP_session());*/
		LOG_INFO(configData::getInstance().get_loggerId(), "begin stream:ssrc:" << m_session->ssrc);

		if (m_session->transport_type == RTSP_OVER_UDP)
		{
			if (m_rtp_socket == 0)
			{

				m_rtp_socket = socketCreateUdpClient();
			}
			if (m_rtcp_socket == 0)
			{

				m_rtcp_socket = socketCreateUdpClient();
			}
		}
		m_startTime = task_real->getStartTime();
		m_endTime = task_real->genEndTime();
		//将文件seek到指定时间
		if (m_sendThread.joinable())
		{
			m_hasCalledDelete = true;
			m_sendEnd = true;
			m_sendThread.join();
			m_sendEnd = false;
			m_hasCalledDelete = false;
		}
		//m_seq = m_seq_num = 0;
		seekToTime(m_startTime);
		//!将文件seek到指定时间
		std::thread sendThread(std::mem_fun(&FlvVideoFileStreamSource::sendThread), this);
		m_sendThread = std::move(sendThread);
	} while (0);
	return iRet;
}

int FlvVideoFileStreamSource::stopStream(taskScheduleObj * task_obj)
{
	int iRet = 0;
	do
	{
		m_sendEnd = true;

	} while (0);
	return iRet;
}

int FlvVideoFileStreamSource::notify_stoped()
{
	int iRet = 0;
	do
	{
		if (m_hasCalledDelete)
		{
			break;
		}
		taskStreamStop *stopTask = new taskStreamStop(m_session->session_id);
		//taskSchedule::getInstance().addTaskObj(stopTask);
		taskSchedule::getInstance().addTaskObj(stopTask);
	} while (0);
	return iRet;
}

void FlvVideoFileStreamSource::sendThread()
{
	if (!m_fileInited)
	{
		return;
	}
	//控制时间;
	unsigned int firstPacketTimeMs = 0;
	unsigned int curPacketTimeMs = 0;
	unsigned int firstSendTimeMs = 0;
	unsigned int curSendTimeMs = 0;

	firstPacketTimeMs = m_startTime * 1000;
	timeval tmpTime;
	gettimeofday(&tmpTime, 0);
	m_rtcp_time = tmpTime;
	firstSendTimeMs = tmpTime.tv_sec * 1000 + tmpTime.tv_usec / 1000;
	std::lock_guard<std::mutex> guard(m_mutexSend);
	//send data seek cache

	/*if (m_lastKeyframe.size()>0)
	{
		timedPacket pktSend;
		for (auto i : m_lastKeyframe)
		{
			pktSend.packetType = raw_h264_frame;
			pktSend.timestamp = m_startTime * 1000+1;
			pktSend.data.push_back(i);
			break;
		}

		if (0 != sendFrame(pktSend))
		{
			m_sendEnd = true;
		}
	}
	if (m_startTime>0)
	{
		int a = 32;
	}*/
	//!send data seek cache
	while (!m_sendEnd)
	{
		//读取帧
		timedPacket pkt = m_flvParser->getNextFrame();
		//没有数据
		if (pkt.data.size() <= 0)
		{
			m_sendEnd = true;
			break;
		}
		//不是h264数据
		if (pkt.packetType != raw_h264_frame)
		{
			if (pkt.data.size() > 0)
			{
				for (auto i : pkt.data)
				{
					if (i.data != nullptr)
					{
						delete[]i.data;
						i.data = nullptr;
					}
				}
				pkt.data.clear();
			}
			continue;
		}
		//时间范围
		if (m_endTime > m_startTime)
		{
			if (pkt.timestamp > m_endTime * 1000)
			{
				LOG_INFO(configData::getInstance().get_loggerId(), "play out of range time");
				break;
			}
		}
		//sps pps
		for (auto i : pkt.data)
		{
			if (i.size <= 0)
			{
				continue;
			}
			h264_nal_type nalType = (h264_nal_type)(i.data[0] & 0x1f);
			if (nalType == h264_nal_sps)
			{
				if (m_sps.data)
				{
					delete[]m_sps.data;
					m_sps.data = nullptr;
				}
				m_sps.data = new char[i.size];
				m_sps.size = i.size;
				memcpy(m_sps.data, i.data, i.size);
			}
			else if (nalType == h264_nal_pps)
			{
				if (m_pps.data)
				{
					delete[]m_pps.data;
					m_pps.data = nullptr;
				}
				m_pps.data = new char[i.size];
				m_pps.size = i.size;
				memcpy(m_pps.data, i.data, i.size);
			}
			else
			{
				//非sps pps 直接发送
				//sleep
				gettimeofday(&tmpTime, 0);
				curSendTimeMs = tmpTime.tv_sec * 1000 + tmpTime.tv_usec / 1000;
				curPacketTimeMs = pkt.timestamp;
				if (curPacketTimeMs - firstPacketTimeMs > curSendTimeMs - firstSendTimeMs)
				{
					int delta = curPacketTimeMs - firstPacketTimeMs - (curSendTimeMs - firstSendTimeMs);
					if (delta < 1000)
					{
						std::this_thread::sleep_for(std::chrono::milliseconds(delta));
					}
				}
				//!sleep
				timedPacket pktSend;
				pktSend.packetType = pkt.packetType;
				pktSend.timestamp = pkt.timestamp;
				pktSend.data.push_back(i);
				if (0 != sendFrame(pktSend))
				{
					m_sendEnd = true;
				}
			}
		}
		for (auto i : pkt.data)
		{
			if (i.data)
			{
				delete[]i.data;
				i.data = nullptr;
			}
		}

	}

	m_session->start_rtp_time = m_lastRtpTime;

	if (m_hasCalledDelete)
	{

		LOG_INFO(configData::getInstance().get_loggerId(), "pause stream thread,sock id:" << m_RTSP_socket_id);
	}
	else
	{

		notify_stoped();
		LOG_INFO(configData::getInstance().get_loggerId(), "shutdown stream thread,sock id:" << m_RTSP_socket_id);
	}
}

unsigned short FlvVideoFileStreamSource::incrementSeq()
{
	if (m_seq++ == 0xffff)
	{
		m_seq = 0;
		m_seq_num++;
	}
	return m_seq;
}

void FlvVideoFileStreamSource::fillRtpHeader2Buf(RTP_header & rtp_header, char * buf, unsigned int timeFrame)
{
	//根据rtp初始时间和帧时间计算
	//unsigned int timeDelta = timeFrame - m_startTime*1000;
	unsigned int timeDelta = timeFrame;
	rtp_header.ts = m_session->start_rtp_time + timeDelta *(RTP_video_freq / 1000);
	if (m_lastRtpTime > rtp_header.ts)
	{
		int a = 32;
	}
	m_lastRtpTime = rtp_header.ts;
	rtp_header.version = 2;
	rtp_header.padding = 0;
	rtp_header.extension = 0;
	rtp_header.cc = 0;
	rtp_header.marker = 0;
	rtp_header.payload_type = RTP_H264_Payload_type;
	rtp_header.ssrc = htonl(m_session->ssrc);
	rtp_header.seq = htons(incrementSeq());
	//32bit转换为网络字节序;
	rtp_header.ts = htonl(rtp_header.ts);
	int header_size = sizeof rtp_header;
	//生成RTP头;
	int rtp_header_cur = 0;
	buf[rtp_header_cur++] = ((rtp_header.version << 6) | (rtp_header.padding << 5) |
		(rtp_header.extension << 4) | (rtp_header.cc));
	buf[rtp_header_cur++] = ((rtp_header.marker << 7) | (rtp_header.payload_type));
	memcpy(buf + rtp_header_cur, &rtp_header.seq, 2);
	rtp_header_cur += 2;
	memcpy(buf + rtp_header_cur, &rtp_header.ts, 4);
	rtp_header_cur += 4;
	memcpy(buf + rtp_header_cur, &rtp_header.ssrc, 4);
	rtp_header_cur += 4;
}

void FlvVideoFileStreamSource::shutdown()
{
	m_sendEnd = true;
	if (m_sendThread.joinable())
	{
		m_sendThread.join();
	}
	m_mutexSend.lock();
	m_mutexSend.unlock();
	if (m_flvParser)
	{
		delete m_flvParser;
		m_flvParser = nullptr;
	}
	if (m_flvIdxReader)
	{
		delete m_flvIdxReader;
		m_flvIdxReader = nullptr;
	}
	if (m_session)
	{
		delete m_session;
		m_session = nullptr;
	}
	if (m_rtp_socket >= 0)
	{
		closeSocket(m_rtp_socket);
		m_rtp_socket = -1;
	}
	if (m_rtcp_socket >= 0)
	{
		closeSocket(m_rtcp_socket);
		m_rtcp_socket = -1;
	}

	if (m_sps.data)
	{
		delete[]m_sps.data;
		m_sps.data = nullptr;
	}
	if (m_pps.data)
	{
		delete[]m_pps.data;
		m_pps.data = nullptr;
	}
	for (auto i : m_lastKeyframe)
	{
		if (i.data)
		{
			delete[]i.data;
			i.data = nullptr;
		}
	}
	m_lastKeyframe.clear();
}

void FlvVideoFileStreamSource::initStreamFile()
{
}

int FlvVideoFileStreamSource::sendFrame(timedPacket & packetSend)
{
	//目前只发送h264
	int iRet = 0;
	int sendRet = 0;
	int h264_tcp_header_size = 4;
	int h264_tcp_data_size = SOCKET_PACKET_SIZE - h264_tcp_header_size;
	std::list<DataPacket>	pkgs;
	sockaddr_in	remote_addr;
	int remote_addr_len = sizeof(sockaddr_in);
	memset(&remote_addr, 0, sizeof remote_addr);

	do
	{
		if (!m_session)
		{
			iRet = -1;
			break;
		}
		//rtsp 反馈 一般是rtsp心跳
		m_mutexRtspSend.lock();
		for (auto i : m_vectorRtspSendData)
		{
			socketSend(m_RTSP_socket_id, i.c_str(), i.size());
		}
		m_vectorRtspSendData.clear();
		m_mutexRtspSend.unlock();

		if (packetSend.packetType != raw_h264_frame)
		{
			break;
		}
		for (auto i : packetSend.data)
		{
			//rtp

			//tcp
			if (m_session->transport_type == RTSP_OVER_TCP)
			{
				iRet = generateSendableFrame(pkgs, packetSend, h264_tcp_data_size);
				if (iRet != 0)
				{
					//Just send bye;
					RTCP_process Rtcp_process;
					DataPacket bye_packet;
					bye_packet.data = 0;
					Rtcp_process.generate_Bye_Buf(bye_packet, m_rtcp_ssrc);
					char	tcp_header[4];
					tcp_header[0] = '$';
					tcp_header[1] = m_session->RTCP_channel;
					tcp_header[2] = (bye_packet.size >> 8);
					tcp_header[3] = (bye_packet.size & 0xff);
					sendRet = socketSend(m_RTSP_socket_id, tcp_header, 4);
					sendRet = socketSend(m_RTSP_socket_id, bye_packet.data, bye_packet.size);
					delete[]bye_packet.data;
					bye_packet.data = 0;
					break;
				}
				//添加TCP头,发送数据;
				std::list<DataPacket>::iterator it;
				for (it = pkgs.begin(); it != pkgs.end(); it++)
				{
					//添加TCP头即：$ channel size ;
					char	tcp_header[4];
					tcp_header[0] = '$';
					tcp_header[1] = m_session->RTP_channel;
					tcp_header[2] = ((*it).size >> 8);
					tcp_header[3] = ((*it).size & 0xff);
					sendRet = socketSend(m_RTSP_socket_id, tcp_header, 4);
					if (sendRet <= 0)
					{
						iRet = -1;
						break;
					}
					sendRet = socketSend(m_RTSP_socket_id, (*it).data, (*it).size);
					if (sendRet <= 0)
					{
						iRet = -1;
						break;
					}
					m_rtp_pkg_counts++;
					m_rtp_data_size += (*it).size;
					delete[](*it).data;
					//LOG_INFO(configData::getInstance().get_loggerId(),  "time");
				}
				pkgs.clear();
			}

			//udp
			else if (m_session->transport_type == RTSP_OVER_UDP)
			{
				iRet = generateSendableFrame(pkgs, packetSend, h264_tcp_data_size + h264_tcp_header_size);
				if (iRet != 0)
				{
					//Just send bye;
					memset(&remote_addr, 0, sizeof remote_addr);
					remote_addr.sin_family = AF_INET;
					remote_addr.sin_port = htons(m_session->RTCP_c_port);
					memcpy(&remote_addr.sin_addr, &m_sock_addr, sizeof m_sock_addr);
					RTCP_process Rtcp_process;
					DataPacket bye_packet;
					bye_packet.data = 0;
					Rtcp_process.generate_Bye_Buf(bye_packet, m_rtcp_ssrc);
					sendRet = socketSendto(m_rtcp_socket, bye_packet.data, bye_packet.size, 0, (const sockaddr*)&remote_addr, remote_addr_len);
					delete[]bye_packet.data;
					bye_packet.data = 0;
					break;
				}
				std::list<DataPacket>::iterator it;
				for (it = pkgs.begin(); it != pkgs.end(); it++)
				{
					memset(&remote_addr, 0, sizeof remote_addr);
					remote_addr.sin_family = AF_INET;
					remote_addr.sin_port = htons(m_session->RTP_c_port);
					memcpy(&remote_addr.sin_addr, &m_sock_addr, sizeof m_sock_addr);
					sendRet = socketSendto(m_rtp_socket, (*it).data, (*it).size, 0, (const sockaddr*)&remote_addr, remote_addr_len);
					m_rtp_pkg_counts++;
					m_rtp_data_size += (*it).size;
					delete[](*it).data;
				}
				pkgs.clear();
			}
			else
			{
				iRet = -1;
				break;
			}
			//!rtp

			//rtcp
			//RTCP
			timeval time_now;
			gettimeofday(&time_now, 0);
			if (time_now.tv_sec > m_rtcp_time.tv_sec + 30)
			{
				m_rtcp_time = time_now;
				//SR
				DataPacket SR_packet, SDES_packet;
				iRet = RTCP_process::generate_RTCP_SR_buf(SR_packet, m_rtcp_ssrc, m_rtp_pkg_counts, m_rtp_data_size);
				if (iRet != 0)
				{
					break;
				}
				//SDES
				iRet = RTCP_process::generate_SDES_Buf(SDES_packet, m_rtcp_ssrc);
				if (iRet != 0)
				{
					break;
				}
				if (m_session->transport_type == RTSP_OVER_TCP)
				{
					char	tcp_header[4];
					int total_size = SR_packet.size + SDES_packet.size;
					tcp_header[0] = '$';
					tcp_header[1] = m_session->RTCP_channel;
					tcp_header[2] = (total_size >> 8);
					tcp_header[3] = (total_size & 0xff);
					sendRet = socketSend(m_RTSP_socket_id, tcp_header, 4);

					if (sendRet <= 0)
					{
						iRet = -1;
						break;
					}
					sendRet = socketSend(m_RTSP_socket_id, SR_packet.data, SR_packet.size);
					if (sendRet <= 0)
					{
						iRet = -1;
						break;
					}
					sendRet = socketSend(m_RTSP_socket_id, SDES_packet.data, SDES_packet.size);
					if (sendRet <= 0)
					{
						iRet = -1;
						break;
					}
				}
				else
				{
					/*LOG_WARN(configData::getInstance().get_loggerId(), "not send udp");
					break;*/
					memset(&remote_addr, 0, sizeof remote_addr);
					remote_addr.sin_family = AF_INET;
					remote_addr.sin_port = htons(m_session->RTCP_c_port);
					memcpy(&remote_addr.sin_addr, &m_sock_addr, sizeof m_sock_addr);
					sendRet = socketSendto(m_rtcp_socket, SR_packet.data, SR_packet.size, 0, (const sockaddr*)&remote_addr, remote_addr_len);
					if (sendRet <= 0)
					{
						iRet = -1;
						break;
					}
					sendRet = socketSendto(m_rtcp_socket, SDES_packet.data, SDES_packet.size, 0, (const sockaddr*)&remote_addr, remote_addr_len);
					if (sendRet <= 0)
					{
						iRet = -1;
						break;
					}
				}

				delete[]SR_packet.data;
				SR_packet.data = 0;
				delete[]SDES_packet.data;
				SDES_packet.data = 0;
			}
			//!rtcp
		}
	} while (0);
	return iRet;
}

int FlvVideoFileStreamSource::generateSendableFrame(std::list<DataPacket>& frames, timedPacket & packetSend, int frame_size)
{
	const int RTP_HEADER_SIZE = 12;
	const int FU_header_size = 1;
	const int F_NRI_type_size = 1;
	//264 本身包含 F NRI ＴＹＰＥ；
	int payload_size = frame_size - RTP_HEADER_SIZE - F_NRI_type_size;
	int iRet = 0;

	bool read_again = false;
	char rtp_header_data[12];

	RTP_header  rtp_header;
	do
	{
		if (packetSend.packetType != raw_h264_frame)
		{
			LOG_ERROR(configData::getInstance().get_loggerId(), "not support fmt:" << packetSend.packetType << " now");
			iRet = -1;
			break;
		}
		for (auto i : packetSend.data)
		{
			if (i.size <= 0)
			{
				continue;
			}
			h264_nal_type	nal_type = (h264_nal_type)(i.data[0] & 0x1f);
			if (h264_nal_slice_idr == nal_type)
			{
				//发送sps pps
				char *stap_a_data = 0;
				int stap_a_len = 0;
				DataPacket sps_pps_pkg;
				//添加SPS，PPS这个STAP_A包;RFC3984 5.7.1;
				stap_a_len = 1 + 2 + 2 + m_sps.size + m_pps.size;//
				stap_a_data = new char[1 + 2 + 2 + m_sps.size + m_pps.size];
				int stap_cur;
				stap_cur = 0;
				//STAP_A hdr即 forbiddenzero nri type
				unsigned char forbidden_zero_bit, nri, type;
				forbidden_zero_bit = 0;
				//提取nri;
				nri = ((m_sps.data[0] & 0x60) >> 5);
				type = NAL_TYPE_STAP_A;
				stap_a_data[stap_cur] = ((forbidden_zero_bit << 7) | (nri << 5) | (type));
				stap_cur++;
				//sps size;
				unsigned short nal_size;
				nal_size = m_sps.size;
				nal_size = htons(nal_size);
				memcpy(stap_a_data + stap_cur, &nal_size, 2);
				stap_cur += 2;
				//sps data;
				memcpy(stap_a_data + stap_cur, m_sps.data, m_sps.size);
				stap_cur += m_sps.size;
				//pps size;
				nal_size = m_pps.size;
				nal_size = htons(nal_size);
				memcpy(stap_a_data + stap_cur, &nal_size, 2);
				stap_cur += 2;
				//pps data;
				memcpy(stap_a_data + stap_cur, m_pps.data, m_pps.size);
				stap_cur += m_pps.size;
				sps_pps_pkg.size = RTP_HEADER_SIZE + stap_a_len;
				sps_pps_pkg.data = new char[sps_pps_pkg.size];
				fillRtpHeader2Buf(rtp_header, rtp_header_data, packetSend.timestamp);
				memcpy(sps_pps_pkg.data, rtp_header_data, RTP_HEADER_SIZE);
				memcpy(sps_pps_pkg.data + RTP_HEADER_SIZE, stap_a_data, stap_a_len);
				delete[]stap_a_data;
				frames.push_back(sps_pps_pkg);
			}
			//单一包(数据较小)和分片包(数据较大);
			if (i.size <= payload_size)
			{
				//单一包和STAP包：F NRI TYPE H264的nal自带，无需添加;
				DataPacket single_pkg;

				single_pkg.size = i.size + RTP_HEADER_SIZE;
				single_pkg.data = new char[single_pkg.size];

				fillRtpHeader2Buf(rtp_header, rtp_header_data, packetSend.timestamp);
				memcpy(single_pkg.data, rtp_header_data, RTP_HEADER_SIZE);
				memcpy(single_pkg.data + RTP_HEADER_SIZE, i.data, i.size);
				frames.push_back(single_pkg);
			}
			//分片包;FU_A;RFC 3984 page29;
			else
			{
				//FU_A :F NRI TYPE的TYPE为24，真正的TYPE是 SER TYPE的TYPE;
				unsigned char FU_S, FU_E, FU_R, FU_Type;
				unsigned char forbidden_zero_bit, nri, type;
				forbidden_zero_bit = 0;
				//提取nri;
				nri = ((i.data[0] & 0x60) >> 5);
				type = NAL_TYPE_FU_A;
				FU_Type = nal_type;
				//FU header size 1;
				payload_size -= FU_header_size;
				int num_frames = i.size / payload_size;
				int cur_frame;
				if (num_frames*payload_size < i.size)
				{
					num_frames++;
				}
				cur_frame = 0;
				DataPacket	fu_a_pkg;
				//已经读取的数据量;
				int cur_frame_cur;
				//pkg保存进度;
				int pkg_cur;
				//第一帧;1 0 0
				FU_S = 1;
				FU_E = 0;
				FU_R = 0;
				//不需要 F NRI TYPE;
				cur_frame_cur = 1;
				pkg_cur = 0;
				fu_a_pkg.size = payload_size + FU_header_size + F_NRI_type_size + RTP_HEADER_SIZE;
				fu_a_pkg.size = frame_size;
				fu_a_pkg.data = new char[fu_a_pkg.size];

				fillRtpHeader2Buf(rtp_header, rtp_header_data, packetSend.timestamp);
				memcpy(fu_a_pkg.data + pkg_cur, rtp_header_data, RTP_HEADER_SIZE);
				pkg_cur += RTP_HEADER_SIZE;
				fu_a_pkg.data[pkg_cur] = ((forbidden_zero_bit << 7) | (nri << 5) | (type));
				pkg_cur += F_NRI_type_size;
				fu_a_pkg.data[pkg_cur] = ((FU_S << 7) | (FU_E << 6) | (FU_R << 5) | (FU_Type));
				pkg_cur += FU_header_size;
				memcpy(fu_a_pkg.data + pkg_cur, i.data + cur_frame_cur, payload_size);
				pkg_cur += payload_size;
				cur_frame_cur += payload_size;
				frames.push_back(fu_a_pkg);
				cur_frame++;
				//中间帧:0 0 0
				FU_S = 0;
				FU_E = 0;
				FU_R = 0;

				while (cur_frame + 1 < num_frames)
				{
					pkg_cur = 0;
					fu_a_pkg.size = payload_size + F_NRI_type_size + FU_header_size + RTP_HEADER_SIZE;
					fu_a_pkg.size = frame_size;
					fu_a_pkg.data = new char[fu_a_pkg.size];

					fillRtpHeader2Buf(rtp_header, rtp_header_data, packetSend.timestamp);
					memcpy(fu_a_pkg.data + pkg_cur, rtp_header_data, RTP_HEADER_SIZE);
					pkg_cur += RTP_HEADER_SIZE;
					fu_a_pkg.data[pkg_cur] = ((forbidden_zero_bit << 7) | (nri << 5) | (type));
					pkg_cur += F_NRI_type_size;
					fu_a_pkg.data[pkg_cur] = ((FU_S << 7) | (FU_E << 6) | (FU_R << 5) | (FU_Type));
					pkg_cur += FU_header_size;
					memcpy(fu_a_pkg.data + pkg_cur, i.data + cur_frame_cur, payload_size);
					pkg_cur += payload_size;
					cur_frame_cur += payload_size;
					frames.push_back(fu_a_pkg);
					cur_frame++;
				}
				//最后帧:0 1 0
				FU_S = 0;
				FU_E = 1;
				FU_R = 0;

				pkg_cur = 0;
				//剩下的数据大小;
				int last_frame_size;
				last_frame_size = i.size - cur_frame_cur;
				if (last_frame_size > 0)
				{
					fu_a_pkg.size = i.size - cur_frame_cur + F_NRI_type_size + FU_header_size + RTP_HEADER_SIZE;
					fu_a_pkg.data = new char[fu_a_pkg.size];

					fillRtpHeader2Buf(rtp_header, rtp_header_data, packetSend.timestamp);
					memcpy(fu_a_pkg.data + pkg_cur, rtp_header_data, RTP_HEADER_SIZE);
					pkg_cur += RTP_HEADER_SIZE;
					fu_a_pkg.data[pkg_cur] = ((forbidden_zero_bit << 7) | (nri << 5) | (type));
					pkg_cur += F_NRI_type_size;
					fu_a_pkg.data[pkg_cur] = ((FU_S << 7) | (FU_E << 6) | (FU_R << 5) | (FU_Type));
					pkg_cur += FU_header_size;
					memcpy(fu_a_pkg.data + pkg_cur, i.data + cur_frame_cur, last_frame_size);
					pkg_cur += last_frame_size;
					cur_frame_cur += last_frame_size;
					frames.push_back(fu_a_pkg);
				}
			}
		}
	} while (0);
	return iRet;
}

int FlvVideoFileStreamSource::seekToTime(double targetTimeS)
{
	int iRet = 0;
	do
	{
		if (!m_fileInited)
		{
			iRet = -1;
			break;
		}
		//读取sps pps
		while (true)
		{
			if (m_sps.data&&m_pps.data)
			{
				break;
			}
			timedPacket tmpPacket;
			tmpPacket = m_flvParser->getNextFrame();
			if (tmpPacket.data.size() <= 0)
			{
				break;
			}
			for (auto i : tmpPacket.data)
			{
				if (tmpPacket.packetType == raw_h264_frame)
				{
					if (i.size <= 0)
					{
						continue;
					}
					h264_nal_type nalType = (h264_nal_type)(i.data[0] & 0x1f);
					if (nalType == h264_nal_sps)
					{
						if (m_sps.data)
						{
							delete[]m_sps.data;
							m_sps.data = nullptr;
						}
						m_sps.data = new char[i.size];
						m_sps.size = i.size;
						memcpy(m_sps.data, i.data, i.size);
					}
					else if (nalType == h264_nal_pps)
					{
						if (m_pps.data)
						{
							delete[]m_pps.data;
							m_pps.data = nullptr;
						}
						m_pps.data = new char[i.size];
						m_pps.size = i.size;
						memcpy(m_pps.data, i.data, i.size);
					}
				}
			}
			for (auto i : tmpPacket.data)
			{
				if (i.data)
				{
					delete[]i.data;
					i.data = nullptr;
				}
			}
			tmpPacket.data.clear();
		}

		//long long seekPos = 0;
		if (m_IdxExist)
		{
			flvIdx idx;
			if (!m_flvIdxReader->getNearestIdx(m_startTime, idx))
			{
				m_sendEnd = true;
				iRet = -1;
				break;
			}
			if (idx.offset == 0)
			{
				m_sendEnd = true;
				iRet = -1;
				break;
			}
			//seekPos = idx.offset;
			m_flvParser->seekToPos(idx.offset);
			return 0;
		}
		else
		{
			//从第一帧开始
			m_flvParser->seekToPos(0);
		}
		//找到最近的时间
		bool end = false;
		while (!end)
		{
			timedPacket packet = m_flvParser->getNextFrame(raw_h264_frame);
			if (packet.data.size() <= 0)
			{
				iRet = -1;
				break;
			}
			for (auto i : packet.data)
			{
				if (i.size <= 0)
				{
					continue;
				}
				double curPktTimeS = (double)packet.timestamp / 1000.0;

				if (curPktTimeS >= m_startTime)
				{
					end = true;
					break;
				}
			}

			for (auto i : packet.data)
			{
				if (i.data)
				{
					delete[]i.data;
					i.data = nullptr;
				}
			}
		}
	} while (0);
	return iRet;
}

