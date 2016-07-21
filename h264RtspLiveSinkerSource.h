#pragma once
#include "realtimeSinker.h"
#include "task_schedule_obj.h"
#include "RTSP.h"
#include <list>
#include <vector>
#include <thread>
#include <mutex>

#include <condition_variable>

class h264RtspLiveSinker :
	public streamSinker
{
public:
	h264RtspLiveSinker(const char *streamName, LP_RTSP_session session,taskJsonStruct &stTask);
	virtual ~h264RtspLiveSinker();
	virtual	taskJsonStruct& getTaskJson();
	virtual void addDataPacket(DataPacket &dataPacket);
	virtual void addTimedDataPacket(timedPacket &dataPacket);
	virtual void sourceRemoved();
	virtual int handTask(taskScheduleObj *task_obj);
private:
	virtual int startStream(taskScheduleObj *task_obj);
	virtual int stopStream(taskScheduleObj *task_obj);
	virtual int notify_stoped();
	void getFps();
	//发送线程;
	static void sendThread(void *lparam);
	//发送一帧;
	int send_frame();
	//根据帧数据生成可以发送的帧，如果是TCP模式，后需加上TCP头;
	
	int generate_sendable_frame(std::list<DataPacket> &frames, int  frame_size);
	//增长seq;
	unsigned short incrementSeq();
	//填充RTP_header到一个buffer里;
	void fillRtpHeader2Buf(RTP_header &rtp_header, char *buf,unsigned int timeFrame);

	static const int s_base_frame_size = 1024 * 1024;
	void shutdown();
	int  get_frame();

	/////////////////////////////////////////////////////////////////////////////////////
	taskJsonStruct	m_taskStruct;
	//std::list<DataPacket>	m_dataPacketList;

	volatile bool m_sendFirstPacket;
	unsigned int m_curPacketTimeMS;
	int			m_firstPacketTimeMS;
	std::mutex	m_mutexDataPacket;
	static const int s_maxCachePacket = 5;

	//中间缓存，添加包时添加到这里，取包从这里取
	std::list<timedPacket>	m_dataPacketCache;
	//从中间缓存取包，发送
	std::list<timedPacket>	m_dataPacketSend;
	std::string m_streamName;

	timedPacket m_pktForNoPkt;

	int	m_RTSP_socket_id;

	char *m_tmp_buf_in;
	int m_tmp_size;
	char *m_sps;
	int m_sps_len;
	char *m_pps;
	int m_pps_len;
	std::mutex m_spsLock;
	std::mutex m_ppsLock;


	in_addr m_sock_addr;
	socket_t	m_rtp_socket;
	socket_t	m_rtcp_socket;

	//264文件只有一个session;
	LP_RTSP_session m_session;
	unsigned int m_rtcp_ssrc;
	unsigned short m_seq;
	unsigned short m_seq_num;
	unsigned int	m_rtp_pkg_counts;
	unsigned int	m_rtp_data_size;
	timeval	m_rtcp_time;

	volatile bool m_send_end;
	std::mutex	m_mutex_send;
	std::thread m_thread_send;

	std::vector<std::string>	m_vectorRtspSendData;
	std::mutex					m_mutexRtspSend;
	std::condition_variable		m_sendCondition;

	bool m_hasCalledDelete;
	int m_fps;
};

