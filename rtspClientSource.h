#pragma once
#include "task_executor.h"
#include "taskJsonStructs.h"
#include "RTCP_Process.h"
#include "RTSP.h"
#include "socket_type.h"
#include "tcpCacher.h"
#include "realtimeSinker.h"
#include "RTPReceptionStats.h"

#include <thread>
#include <condition_variable>

class rtspSession
{
public:
	rtspSession();
	~rtspSession();
	bool generateSdp();
	rtspSession(const rtspSession&);
	rtspSession operator = (const rtspSession&) = delete;
	std::string m_strCodecName;
	unsigned int m_uRtpTimestampFrequency;
	int		m_packetization_mode;//0:单一Nalu单元模式 1非交错模式 2交错模式
	int		m_payloadFmt;
	std::string	m_profile_level_id;// profile 类型和级别
	std::string	m_sps_base64;
	std::string	m_pps_base64;
	std::string m_control;
	bool m_RAWRAWUDP;
	unsigned char*	m_sps;
	unsigned char*	m_pps;
	size_t		m_sps_len;
	size_t		m_pps_len;
	std::string m_sdp;
};

typedef struct _RTP_stats
{
	unsigned short	first_seq;
	unsigned short	highest_seq;
	unsigned short	rtp_received;
	unsigned int	rtp_identifier;
	unsigned int	rtp_ts;
	unsigned int	rtp_cum_lost;
	unsigned int	rtp_expected_prior;
	unsigned int	rtp_received_prior;
	unsigned short	rtp_cycles;
	unsigned int	transit;
	unsigned int	jitter;
	unsigned int	lst;
	unsigned int	last_dlsr;
	unsigned int	last_rcv_SR_ts;
	unsigned int	delay_snc_last_SR;
	timeval			last_recv_SR_time;
	timeval			last_recv_time;
	double			rtt_frac;
}RTP_stats;

//auto reconnect=false
//获取帧后使用推模式
//流结束后产生流结束通知，并异步发送删除任务
//析构时不发送任务和通知
class rtspClientSource :
	public streamSinker
{
public:
	rtspClientSource(taskJsonStruct &stTask,int jsonSocket);
	std::string getTaskId();
	bool taskValid();
	virtual ~rtspClientSource();
	virtual taskJsonStruct& getTaskJson();
	virtual void sourceRemoved();
	virtual int handTask(taskScheduleObj *task_obj);
private:
	rtspClientSource(const rtspClientSource&) = delete;
	rtspClientSource& operator= (const rtspClientSource&) = delete;
	static const int s_tcpTimeoutSec = 60;	//TCP超时
	bool parseTaskParam(); //解析struct，判断是否为有效任务，有效任务才尝试重连

	taskJsonStruct m_taskStruct;
	bool m_validTask;
	short m_port;

	std::string m_username;
	std::string m_password;
	std::string m_svrAddr;
	std::string m_streamName;

	bool m_createNewFile;

	std::thread m_streamThread;	//启动线程
	std::thread m_recvThread;	//接收线程
	std::condition_variable m_streamCondition;
	volatile bool m_end;

	void rtspStreamStart();
	void tcpRecv();
	//成功后断开返回true，无法接收udp数据返回false
	bool rtspUdp();
	void rtspTcp();
	char*	getLine(char*	startOfLine);
	int		getLineCount(char const* pString);
	bool	checkHeader(char const* line, char const* headerName, char const*& headerParams);
	bool	parseSdpLine(char const* inputLine, char const*& nextLine);
	bool	parseSDPLine_s(char const* sdpLine);
	bool	parseSDPLine_i(char const* sdpLine);
	bool	parseSDPLine_c(char const* sdpLine);
	bool	parseSDPAttribute_type(char const* sdpLine);
	bool	parseSDPAttribute_control(char const* sdpLine);
	bool	parseSDPAttribute_range(char const* sdpLine);
	bool	parseSDPAttribute_source_filter(char const* sdpLine);
	bool	parseSdpAttribute_rtpmap(const char* sdpDescription, rtspSession &subSession);
	bool	parseSdpAttribute_control(const char* sdpDescription, rtspSession &subSession);
	bool	parseSdpAttribute_fmtp(const char* sdpDescription, rtspSession &subSession);
	bool	parseTransport(const char* pTransport);
	bool	parseSession(const char* pSession);
	bool parseRtspStatus(std::string rtspBuf);
	bool sendOptions();
	bool sendOptionsWithSession();
	bool parseOptions(std::string rtspBuf);
	bool sendDescribe();
	bool parseDescribe(std::string rtspBuf);
	bool sendSetup(bool tcp = false, bool rawrawudp = false);
	bool creaetRtpRtcpSocket();
	bool parseSetup(std::string rtspBuf);
	bool sendPlay();
	bool parsePlay(std::string rtspBuf);
	void netPenetrate();

	socket_t m_rtspSocket;
	socket_t m_rtpSocket;
	socket_t m_rtcpSocket;
	socket_t m_jsonSocket;
	unsigned short	m_rtpPort;
	unsigned short	m_rtcpPort;
	unsigned short	m_rtpSvrPort;
	unsigned short	m_rtcpSvrPort;

	tcpCacher *m_tcpCacher;
	std::mutex m_mutexCacher;

	int m_cseq;
	std::string m_strDate;
	std::list<std::string> m_publicFuncList;
	std::string	m_sessionName;
	std::string m_sessionDescription;
	std::string m_connectionEndpointName;
	std::string m_mediaSessionType;
	std::string m_controlPath;
	double m_startTime;
	double m_endTime;
	bool	m_bClock;
	std::string m_startClock;
	std::string m_endClock;
	std::vector<rtspSession> m_sessionList;
	int m_timeout;
	std::string m_sessionId;
	unsigned short m_rtpMulticastPort;
	unsigned short m_rtcpMulticastPort;
	bool m_tcpProtocol;
	bool m_multicast;
	std::string m_destination;
	std::string m_source;

	//sink
	std::list<taskExecutor*>	m_sinkerList;
	std::mutex					m_sinkerMutex;

	DataPacket				m_keyframe;
	std::mutex				m_keyframeMutex;
	//rtsp rtp rtcp valid data.
	std::mutex				m_rtspDataMutex;
	std::mutex				m_rtpDataMutex;
	std::mutex				m_rtcpDataMutex;
	std::list<DataPacket>	m_rtspDataList;
	std::list<DataPacket>	m_rtpDataList;
	std::list<DataPacket>   m_rtcpDataList;
	long					m_lastRtpRecvedTime;	//s

	//RTSP接收线程
	void rtspProcessThread();
	bool rtspRecvPacket(char *buff,int bufSize, int &outSize,int &channel);
	//RTP
	void rtpProcessThread();
	//RTCP
	void rtcpProcessThread();
	//RTSP心跳线程,并处理rtp包的时间问题
	void rtspHeartbeatThread();
	//处理各种类型的rtp数据，整合成完整帧，发送给sinker，sinker自行处置
	void rtpStatsUpdate(RTP_header &rtpHeader);
	std::mutex				m_processerMutex;
	RTP_header				m_rtpHeader;
	RTCP_header				m_rtcpHeader;
	RTCP_header_SR			m_RTCP_header_SR;
	RTP_stats				m_rtpStats;
	RTPReceptionStats		*m_rtpReceptionStats;
	void sendRR();
	unsigned int m_ssrcRR;
	void addRtpDataToProcesser(const char *data,int size,int type);
	void processRtcpData(const char *data, int size);
	std::vector<DataPacket>	m_rtpDataForProcesser;
	std::vector<int>	m_rtpDataForProcesserTimeOffset;
	//可能有复合包
	void parseH264Payload(std::vector<DataPacket>& rtpData, const char *data, int size, bool &getFrameEnd);
	//有几种封包格式未测试
	void warnOnce(std::string logData);
	//关闭所有RTSP rtp rtcp套接字
	void closeRtspRtpRtcpSocket();
	//通知任务开始结束，槽
	bool m_notifyStarted;
	void notifyLiveStarted();
	void notifyLiveEnded();

	volatile bool m_hasCalledDeleteSelf;
};

