#pragma once
#include "streamSource.h"
#include "localFileReader.h"
#include "RTSP.h"
#include "RTP.h"

#include <mutex>
#include <vector>
#include <thread>




class H264VideoFileStreamSource:public streamSource
{
public:
	H264VideoFileStreamSource(const char *file_name, LP_RTSP_session session);
	~H264VideoFileStreamSource();
	virtual int handTask(taskScheduleObj *task_obj);
protected:
	virtual int startStream(taskScheduleObj *task_obj);
	virtual int stopStream(taskScheduleObj *task_obj);
	virtual int notify_stoped();
private:
	H264VideoFileStreamSource(H264VideoFileStreamSource&);
	//发送线程;
	static void sendThread(void *lparam);
	//发送一帧;
	int send_frame();
	//根据帧数据生成可以发送的帧，如果是TCP模式，后需加上TCP头;
	int generate_sendable_frame(std::list<DataPacket> &frames, int  frame_size);
	//增长seq;
	unsigned short incrementSeq();
	//填充RTP_header到一个buffer里;
	void fillRtpHeader2Buf(RTP_header &rtp_header,  char *buf);

	static const int s_base_frame_size=1024*1024;
	void shutdown();
	//读取sps,pps,分析宽高和fps;
	void initStreamFile();
	bool get_sps_pps();
	bool anasys_sps_pps();
	int  get_frame();

	int	m_RTSP_socket_id;
	localFileReader *m_file_reader;

	char *m_tmp_buf_in;
	int m_tmp_size;
	char *m_sps;
	int m_sps_len;
	char *m_pps;
	char *m_frame_data;
	int m_frame_size;
	int m_frame_container_size;
	int m_pps_len;
	int m_video_width;
	int m_video_height;
	int m_fps;
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

	bool m_hasCalledDelete;
};
