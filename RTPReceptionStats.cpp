#include "RTPReceptionStats.h"
#include "util.h"
RTPReceptionStats::RTPReceptionStats(unsigned int SSRC):m_SSRC(0),m_numPacketsReceivedSinceLastReset(0),
	m_totalNumPacketsReceived(0),m_totalBytesReceived_hi(0),m_totalBytesReceived_lo(0),
	m_haveSeenInitialSequenceNumber(0),m_baseExtSeqNumReceived(0),m_lastResetExtSeqNumReceived(0),
	m_highestExtSeqNumReceived(0),m_lastTransit(0),m_previousPacketRTPTimestamp(0),
	m_jitter(0),m_lastReceivedSR_NTPmsw(0),m_lastReceivedSR_NTPlsw(0),
	m_minInterPacketGapUS(0),m_maxInterPacketGapUS(0),m_hasBeenSynchronized(0)
{
	gethostname(m_CNAME,100);
	m_CNAME_length=strlen(m_CNAME);
	initStats(SSRC);
}

RTPReceptionStats::RTPReceptionStats(unsigned int SSRC,unsigned short initialSeq):m_SSRC(0),m_numPacketsReceivedSinceLastReset(0),
	m_totalNumPacketsReceived(0),m_totalBytesReceived_hi(0),m_totalBytesReceived_lo(0),
	m_haveSeenInitialSequenceNumber(0),m_baseExtSeqNumReceived(0),m_lastResetExtSeqNumReceived(0),
	m_highestExtSeqNumReceived(0),m_lastTransit(0),m_previousPacketRTPTimestamp(0),
	m_jitter(0),m_lastReceivedSR_NTPmsw(0),m_lastReceivedSR_NTPlsw(0),
	m_minInterPacketGapUS(0),m_maxInterPacketGapUS(0),m_hasBeenSynchronized(0)
{
	gethostname(m_CNAME,100);
	m_CNAME_length=strlen(m_CNAME);
	initSeqNum(initialSeq);
	initStats(SSRC);
}

RTPReceptionStats::~RTPReceptionStats(void)
{
}

unsigned int	RTPReceptionStats::SSRC()
{
	return m_SSRC;
}

unsigned int	RTPReceptionStats::numPacketsReceivedSinceLastReset()
{
	return	m_numPacketsReceivedSinceLastReset;
}

unsigned int	RTPReceptionStats::totalNumPacketsReceived()
{
	return	m_totalNumPacketsReceived;
}

unsigned int	RTPReceptionStats::totalNumPacketsExpected()
{
	return	m_highestExtSeqNumReceived-m_baseExtSeqNumReceived;
}
unsigned int	RTPReceptionStats::baseExtSeqNumReceived()
{
	return	m_baseExtSeqNumReceived;
}

unsigned int	RTPReceptionStats::lastResetExtSeqNumReceived()
{
	return	m_lastResetExtSeqNumReceived;
}

unsigned int	RTPReceptionStats::highestExtSeqNumReceived()
{
	return m_highestExtSeqNumReceived;
}

unsigned int	RTPReceptionStats::jitter()
{
	return (unsigned int)m_jitter;
}

unsigned int	RTPReceptionStats::lastReceivedSR_NTPmsw()
{
	return	m_lastReceivedSR_NTPmsw;
}

unsigned int	RTPReceptionStats::lastReceivedSR_NTPlsw()
{
	return m_lastReceivedSR_NTPlsw;
}

timeval	const&	RTPReceptionStats::lastReceivedSR_time()
{
	return	m_lastReceivedSR_time;
}

unsigned int	RTPReceptionStats::minInterPacketGapUS()
{
	return	m_minInterPacketGapUS;
}

unsigned int	RTPReceptionStats::maxInterPacketGapUS()
{
	return m_maxInterPacketGapUS;
}

timeval const&	RTPReceptionStats::totalInterPacketGaps()
{
	return	m_totalInterPacketGaps;
}

void			RTPReceptionStats::noteIncomingPacket(
	unsigned short	seqNum,
	unsigned int	rtpTimestamp,unsigned int	timestampFrequency,
	bool		useForJitterCalculation,
	timeval&	resultPresentationTime,
	bool&		resultHasBeenSyncedUsingRTCP,
	unsigned int	packetSize)
{
	if (!m_haveSeenInitialSequenceNumber)
	{
		initSeqNum(seqNum);
	}
	m_numPacketsReceivedSinceLastReset++;
	m_totalNumPacketsReceived++;
	unsigned int lastTotalBytesReceived_Io=m_totalBytesReceived_lo;
	m_totalBytesReceived_lo+=packetSize;
	if (m_totalBytesReceived_lo<lastTotalBytesReceived_Io)
	{
		m_totalBytesReceived_hi++;
	}
	unsigned int oldSeqNum=(m_highestExtSeqNumReceived&0xffff);
	unsigned int seqNumCycle=(m_highestExtSeqNumReceived&0xffff0000);
	unsigned int seqNumDifference=(unsigned int)((int)seqNum-(int)oldSeqNum);
	//32bit contains cycle seqNum;
	unsigned int newSeqNum=0;

	if (seqNumLT((unsigned short)oldSeqNum,seqNum))
	{
		//newSeq<oldSeq,uint<0,seqNumDifference>>0,进入新的循环;
		if (seqNumDifference>0x8000)
		{
			seqNumCycle+=0x10000;
		}

		newSeqNum=seqNumCycle|seqNum;
		if (newSeqNum>m_highestExtSeqNumReceived)
		{
			m_highestExtSeqNumReceived=newSeqNum;
		}
	}else if(m_totalNumPacketsReceived>1)
	{
		//newSeq>>oldSeq回到上一个循环;
		if ((int)seqNumDifference>0x8000)
		{
			seqNumCycle-=0x10000;
		}
		newSeqNum=seqNumCycle|seqNum;
		if (m_baseExtSeqNumReceived>newSeqNum)
		{
			m_baseExtSeqNumReceived=newSeqNum;
		}
	}
	//记录包延时;
	timeval timeNow;
	gettimeofday(&timeNow,0);
	if (m_lastPacketReceptionTime.tv_sec!=0&&
		m_lastPacketReceptionTime.tv_usec!=0)
	{
		unsigned int gapUsec=(timeNow.tv_sec-m_lastPacketReceptionTime.tv_sec)*1000000+
			(timeNow.tv_usec-m_lastPacketReceptionTime.tv_usec);
		if (gapUsec>m_maxInterPacketGapUS)
		{
			m_maxInterPacketGapUS=gapUsec;
		}
		if (gapUsec<m_minInterPacketGapUS)
		{
			m_minInterPacketGapUS=gapUsec;
		}
		m_totalInterPacketGaps.tv_usec+=gapUsec;
		while (m_totalInterPacketGaps.tv_usec>1000000)
		{
			m_totalInterPacketGaps.tv_sec++;
			m_totalInterPacketGaps.tv_usec-=1000000;
		}
	}
	m_lastPacketReceptionTime=timeNow;
	//rtp jitter：时间抖动;
	//Si:RTP 时间戳;
	//Ri:RTP 接收到时的时间戳;
	//D(i,j)=(Rj-Ri)-(Sj-Si)=(Rj-Sj)-(Ri-Si);
	//J=J+(|D(i-1,i)|-J)/16;
	//RFC 1889 A.8
	if (useForJitterCalculation&&
		rtpTimestamp!=m_previousPacketRTPTimestamp)
	{
		unsigned int arrival=(timestampFrequency*timeNow.tv_sec);
		arrival+=(unsigned int)((2.0*timestampFrequency*timeNow.tv_usec+1000000.0)/2000000.0);

		int	transit=arrival-rtpTimestamp;
		if (m_lastTransit==~0)
		{
			m_lastTransit=transit;
		}
		int d=transit-m_lastTransit;
		m_lastTransit=transit;
		if (d<0)
		{
			d=-d;
		}
		m_jitter+=(1.0/16.0)*((double)d-m_jitter);
	}
	if (m_syncTime.tv_usec==0&&m_syncTime.tv_sec==0)
	{
		m_syncTime=timeNow;
		m_syncTimestamp=rtpTimestamp;
	}
	//时间戳间距;
	int timestampDiff=rtpTimestamp-m_syncTimestamp;
	//乘以采样率，获得真实时间差;
	double	timeDiff=timestampDiff/(double)timestampFrequency;

	unsigned int seconds=0,useconds=0;
	if (timeDiff>=0.0)
	{
		seconds=m_syncTime.tv_sec+(unsigned int)timeDiff;
		useconds=m_syncTime.tv_usec+
			(unsigned int)((timeDiff-(unsigned int)timeDiff)*UMILLION);
		if (useconds>=UMILLION)
		{
			seconds++;
			useconds-=UMILLION;
		}
	}else
	{
		timeDiff=-timeDiff;
		seconds=m_syncTime.tv_sec-(unsigned int)timeDiff;
		useconds=m_syncTime.tv_usec-
			(unsigned int)((timeDiff-(unsigned int)timeDiff)*UMILLION);
		if ((int)useconds<0)
		{
			useconds+=UMILLION;
			--seconds;
		}
	}
	resultPresentationTime.tv_sec=seconds;
	resultPresentationTime.tv_usec=useconds;
	resultHasBeenSyncedUsingRTCP=m_hasBeenSynchronized;
	//保存这一包的信息;
	m_syncTimestamp=rtpTimestamp;
	m_syncTime=resultPresentationTime;
	m_previousPacketRTPTimestamp=rtpTimestamp;
}

void			RTPReceptionStats::noteIncomingSR(
	unsigned int ntpTimestampMSW,
	unsigned int ntpTimestampLSW,
	unsigned int rtpTimestamp)
{
	m_lastReceivedSR_NTPmsw=ntpTimestampMSW;
	m_lastReceivedSR_NTPlsw=ntpTimestampLSW;
	gettimeofday(&m_lastReceivedSR_time,0);

	m_syncTimestamp=rtpTimestamp;
	m_syncTime.tv_sec=ntpTimestampMSW-0x83aa7e80;
	double microseconds = (ntpTimestampLSW*15625.0)/0x04000000; // 10^6/2^32
	m_syncTime.tv_usec = (unsigned)(microseconds+0.5);
	m_hasBeenSynchronized = true;
}

void			RTPReceptionStats::resetStats()
{
	m_numPacketsReceivedSinceLastReset=0;
	m_lastResetExtSeqNumReceived=m_highestExtSeqNumReceived;
}

void			RTPReceptionStats::initStats(unsigned int SSRC)
{
	m_SSRC=SSRC;
	m_lastTransit=~0;
	m_lastReceivedSR_time.tv_sec=0;
	m_lastReceivedSR_time.tv_usec=0;
	m_lastPacketReceptionTime.tv_sec=0;
	m_lastPacketReceptionTime.tv_usec=0;
	m_minInterPacketGapUS=0x7fffffff;
	m_maxInterPacketGapUS=0;
	m_totalInterPacketGaps.tv_sec=0;
	m_totalInterPacketGaps.tv_usec=0;
	m_syncTime.tv_sec=0;
	m_syncTime.tv_usec=0;
	resetStats();
}

void			RTPReceptionStats::initSeqNum(unsigned short seqNum)
{
	m_haveSeenInitialSequenceNumber=true;
	m_baseExtSeqNumReceived=0x10000|seqNum;
	m_highestExtSeqNumReceived=0x10000|seqNum;
}

bool			RTPReceptionStats::seqNumLT(unsigned short s1,unsigned short s2)
{
	int diff=s2-s1;
	if (diff>0)
	{
		return (diff<0x8000);
	}else if (diff<0)
	{
		return (diff<-0x8000);
	}else 
	{
		return false;
	}
}


char*			RTPReceptionStats::CNAME(int &len)
{
	len = m_CNAME_length;
	return	m_CNAME;
};