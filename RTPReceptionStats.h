#pragma once
//#include <WinSock2.h>
#include "socket_type.h"

static const int MILLION=1000000;
static const unsigned int UMILLION=1000000;

class RTPReceptionStats
{
public:
	RTPReceptionStats(unsigned int SSRC,unsigned short initialSeq);
	RTPReceptionStats(unsigned int SSRC);
	~RTPReceptionStats(void);

	unsigned int	SSRC();
	unsigned int	numPacketsReceivedSinceLastReset();
	unsigned int	totalNumPacketsReceived();
	unsigned int	totalNumPacketsExpected();
	unsigned int	baseExtSeqNumReceived();
	unsigned int	lastResetExtSeqNumReceived();
	unsigned int	highestExtSeqNumReceived();
	unsigned int	jitter();
	unsigned int	lastReceivedSR_NTPmsw();
	unsigned int	lastReceivedSR_NTPlsw();
	timeval	const&	lastReceivedSR_time();
	unsigned int	minInterPacketGapUS();
	unsigned int	maxInterPacketGapUS();
	timeval const&	totalInterPacketGaps();
	void			noteIncomingPacket(
		unsigned short	seqNum,
		unsigned int	rtpTimestamp,unsigned int	timestampFrequency,
		bool		useForJitterCalculation,
		timeval&	resultPresentationTime,
		bool&		resultHasBeenSyncedUsingRTCP,
		unsigned int	packetSize);
	void			noteIncomingSR(unsigned int ntpTimestampMSW,
		unsigned int ntpTimestampLSW,
		unsigned int rtpTimestamp);
	void			resetStats();
	char*			CNAME(int	&len);
private:
	void			initStats(unsigned int SSRC);
	void			initSeqNum(unsigned short seqNum);
	//判断RTP的序列号是否是S2>S1;
	bool			seqNumLT(unsigned short s1,unsigned short s2);
private:
	char			m_CNAME[100];
	int				m_CNAME_length;

	unsigned int	m_SSRC;
	unsigned int	m_numPacketsReceivedSinceLastReset;
	unsigned int	m_totalNumPacketsReceived;
	unsigned int	m_totalBytesReceived_hi;
	unsigned int	m_totalBytesReceived_lo;
	bool			m_haveSeenInitialSequenceNumber;
	unsigned int	m_baseExtSeqNumReceived;
	unsigned		m_lastResetExtSeqNumReceived;
	unsigned		m_highestExtSeqNumReceived;
	int				m_lastTransit; // used in the jitter calculation
	unsigned int	m_previousPacketRTPTimestamp;
	double			m_jitter;

	unsigned int	m_lastReceivedSR_NTPmsw; // NTP timestamp (from SR), most-signif
	unsigned int	m_lastReceivedSR_NTPlsw; // NTP timestamp (from SR), least-signif
	timeval			m_lastReceivedSR_time;
	timeval			m_lastPacketReceptionTime;
	unsigned int	m_minInterPacketGapUS;
	unsigned int	m_maxInterPacketGapUS;
	timeval			m_totalInterPacketGaps;
	bool			m_hasBeenSynchronized;
	unsigned int	m_syncTimestamp;
	timeval			m_syncTime;
};

