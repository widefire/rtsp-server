#include "RTSP_server.h"
#include "RTCP_Process.h"
#include "util.h"

RTCP_process::RTCP_process():m_pRTCP_SDES(0),m_pRTCP_report_block(0),m_count_SDES(0)
{
	memset(&m_RTCP_header, 0, sizeof m_RTCP_header);
	memset(&m_RTCP_header_SR, 0, sizeof m_RTCP_header_SR);
	memset(&m_RTCP_header_RR, 0, sizeof m_RTCP_header_RR);
	memset(&m_RTCP_goodbye, 0, sizeof m_RTCP_goodbye);
}

RTCP_process::RTCP_process(RTCP_process &rp)
{
	this->m_pkt_type = rp.m_pkt_type;
	this->m_RTCP_header = rp.m_RTCP_header;
	this->m_RTCP_header_SR = rp.m_RTCP_header_SR;
	this->m_RTCP_header_RR = rp.m_RTCP_header_RR;
	m_pRTCP_SDES = 0;
	m_pRTCP_report_block = 0;
	if (this->m_RTCP_header.rc>0)
	{
		this->m_pRTCP_report_block = new RTCP_header_SR_report_block[this->m_RTCP_header.rc];
		memcpy(this->m_pRTCP_report_block, rp.m_pRTCP_report_block, sizeof(RTCP_header_SR_report_block)*this->m_RTCP_header.rc);
	}
	this->m_count_SDES = rp.m_count_SDES;
	if (this->m_count_SDES)
	{
		this->m_pRTCP_SDES = new RTCP_header_SDES[this->m_count_SDES];
		memcpy(this->m_pRTCP_SDES, rp.m_pRTCP_SDES, sizeof(RTCP_header_SDES)*this->m_count_SDES);
	}
	
	this->m_RTCP_goodbye = rp.m_RTCP_goodbye;
}

RTCP_process::~RTCP_process()
{
	if (m_pRTCP_report_block)
	{
		delete[]m_pRTCP_report_block;
		m_pRTCP_report_block = 0;
	}
	if (m_pRTCP_SDES)
	{
		delete[]m_pRTCP_SDES;
		m_pRTCP_SDES = 0;
	}
}

int RTCP_process::parse_RTCP_buf(const DataPacket& data_packet)
{
	int iRet = 0;
	do
	{
		if (data_packet.size<sizeof m_RTCP_header)
		{
			iRet = -1;
			break;
		}
		if (data_packet.data[0]=='$')
		{
			m_BitReader.SetBitReader((unsigned char*)data_packet.data+4, data_packet.size-4);
		}
		else
		{
			m_BitReader.SetBitReader((unsigned char*)data_packet.data, data_packet.size);
		}

		m_RTCP_header.version = m_BitReader.ReadBits(2);
		m_RTCP_header.padding = m_BitReader.ReadBit();
		m_RTCP_header.rc = m_BitReader.ReadBits(5);
		m_RTCP_header.pt = m_BitReader.ReadBits(8);
		m_RTCP_header.length = m_BitReader.ReadBits(16);
		if (m_RTCP_header.rc<=0)
		{
			break;
		}
		switch (m_RTCP_header.pt)
		{
		case RTCP_SR:
			iRet = parseSR();
			break;
		case RTCP_RR:
			iRet = parseRR();
			break;
		case RTCP_SDES:
			iRet = parseSDES();
			break;
		case RTCP_BYE:
			iRet = parseBYE();
			break;
		case RTCP_APP:
			iRet = parseAPP();
			break;
		default:
			iRet = -1;
			break;
		}
		if (iRet!=0)
		{
			break;
		}
	} while (0);

	return iRet;
}

int RTCP_process::generate_RTCP_SR_buf(DataPacket & data_packet, unsigned int SSRC, unsigned int packet_count, unsigned int octet_count)
{
	int iRet = 0;

	if (data_packet.data!=0)
	{
		delete[]data_packet.data;
		data_packet.data = 0;
	}
	data_packet.size = 0;
	rtcp_pkt_type packet_type = RTCP_SR;
	unsigned int version = 2;
	unsigned int padding = 1;
	unsigned int reception_report_count = 0;
	unsigned int length = 6;//SSRC:1 NTP_MSW:1 NTP_LSW:1 RTP:1 SPC:1 SOC:1;

	unsigned int RTCP_header = 0;
	RTCP_header = ((version << 30) | (padding << 29) | (reception_report_count << 24) | (packet_type << 16) | (length));

	//RTCP大小头1行 一个负载6行;
	data_packet.size = (length + 1) * (sizeof(unsigned int));
	data_packet.data = new char[data_packet.size];

	unsigned int tmp_u32 = 0;
	int cur = 0;
	
	tmp_u32 = htonl(RTCP_header);
	memcpy(data_packet.data + cur, &tmp_u32, sizeof tmp_u32);
	cur += sizeof tmp_u32;

	tmp_u32 = htonl(SSRC);
	memcpy(data_packet.data + cur, &tmp_u32, sizeof tmp_u32);
	cur += sizeof tmp_u32;

	timeval time_now;
	gettimeofday(&time_now, 0);

	//MSW 秒;LSW 1,000,000,000,000 picoseconds劈成2的32次方份 约232皮秒 RFC 1305;
	unsigned int msw = time_now.tv_sec + 0x83AA7E80;
	tmp_u32 = htonl(msw);
	memcpy(data_packet.data + cur, &tmp_u32, sizeof tmp_u32);
	cur += sizeof tmp_u32;

	double fractionalPart = (time_now.tv_usec / 15625.0) * 0x04000000+0.5;
	unsigned int lsw = static_cast<unsigned int>(fractionalPart);
	tmp_u32 = htonl(lsw);
	memcpy(data_packet.data + cur, &tmp_u32, sizeof tmp_u32);
	cur += sizeof tmp_u32;

	unsigned int rtp_time = convert_to_RTP_timestamp(time_now, RTP_video_freq);
	tmp_u32 = htonl(rtp_time);
	memcpy(data_packet.data + cur, &tmp_u32, sizeof tmp_u32);
	cur += sizeof tmp_u32;

	tmp_u32 = htonl(packet_count);
	memcpy(data_packet.data + cur, &tmp_u32, sizeof tmp_u32);
	cur += sizeof tmp_u32;

	tmp_u32 = htonl(octet_count);
	memcpy(data_packet.data + cur, &tmp_u32, sizeof tmp_u32);
	cur += sizeof tmp_u32;

	//sdes;

	return iRet;
}

int RTCP_process::generate_RTCP_RR_buf(DataPacket & data_packet, RTCP_RR_generate_param param)
{
	return 0;
}

int RTCP_process::generate_Bye_Buf(DataPacket & data_packet, unsigned int SSRC)
{
	int iRet = 0;
	do
	{
		if (data_packet.data)
		{
			delete[]data_packet.data;
			data_packet.data = 0;
		}

		data_packet.size = 2 * sizeof(unsigned int);
		data_packet.data = new char[data_packet.size];
		rtcp_pkt_type packet_type = RTCP_BYE;
		unsigned int version = 2;
		unsigned int padding = 1;
		unsigned int reception_report_count = 0;
		unsigned int length = 1;//SSRC:1;

		unsigned int RTCP_header = 0;
		RTCP_header = ((version << 30) | (padding << 29) | (reception_report_count << 24) | (packet_type << 16) | (length));

		unsigned int tmp_u32 = 0;
		int cur = 0;
		tmp_u32 = htonl(RTCP_header);
		memcpy(data_packet.data + cur, &tmp_u32, sizeof tmp_u32);
		cur += sizeof tmp_u32;

		tmp_u32 = htonl(SSRC);
		memcpy(data_packet.data + cur, &tmp_u32, sizeof tmp_u32);
		cur += sizeof tmp_u32;

	} while (0);
	return iRet;
}

int RTCP_process::generate_SDES_Buf(DataPacket & data_packet, unsigned int SSRC)
{
	int iRet = 0;
	do
	{
		if (data_packet.data)
		{
			delete[]data_packet.data;
			data_packet.data = 0;
		}

		int CNAME_length = 0;
		const char *CNNAME = get_CNAME(CNAME_length);
		int CNAME_4bytes = (4 + CNAME_length)/4;
		rtcp_pkt_type packet_type = RTCP_SDES;
		unsigned int version = 2;
		unsigned int padding = 1;
		unsigned int reception_report_count = 0;
		unsigned int length = 1+CNAME_4bytes;//SSRC:1;
		unsigned int RTCP_header = 0;
		RTCP_header = ((version << 30) | (padding << 29) | (reception_report_count << 24) | (packet_type << 16) | (length));


		data_packet.size = (length + 1) * sizeof(unsigned int);
		data_packet.data = new char[data_packet.size];
		memset(data_packet.data, 0, data_packet.size);
		unsigned int tmp_u32 = 0;
		int cur = 0;
		tmp_u32 = htonl(RTCP_header);
		memcpy(data_packet.data + cur, &tmp_u32, sizeof tmp_u32);
		cur += sizeof tmp_u32;

		tmp_u32 = htonl(SSRC);
		memcpy(data_packet.data + cur, &tmp_u32, sizeof tmp_u32);
		cur += sizeof tmp_u32;

		memcpy(data_packet.data + cur, CNNAME, CNAME_length);
		cur += CNAME_length;



	} while (0);
	return iRet;
}

unsigned int RTCP_process::RTCP_SSRC()
{
	return m_RTCP_header_SR.ssrc;
}

int RTCP_process::report_block_count()
{
	return m_RTCP_header.rc;
}

unsigned int RTCP_process::RTP_ssrc(int index)
{
	if (index<0||index>=m_RTCP_header.rc)
	{
		return 0;
	}
	return m_pRTCP_report_block[index].ssrc;
}

int RTCP_process::parseSR()
{
	int iRet = 0;
	do
	{
		m_RTCP_header_SR.ssrc = m_BitReader.ReadBits(32);
		m_RTCP_header_SR.ntp_timestamp_MSW = m_BitReader.Read32Bits();
		m_RTCP_header_SR.ntp_timestamp_LSW = m_BitReader.Read32Bits();
		m_RTCP_header_SR.rtp_timestamp = m_BitReader.Read32Bits();
		m_RTCP_header_SR.pkt_count = m_BitReader.Read32Bits();
		m_RTCP_header_SR.octet_count = m_BitReader.Read32Bits();
		if (m_RTCP_header.rc>0)
		{
			if (m_RTCP_header.rc>16)
			{
				LOGW("RTCP header rc may be too big:" << m_RTCP_header.rc);
			}
			if (m_pRTCP_report_block)
			{
				delete[]m_pRTCP_report_block;
				m_pRTCP_report_block = 0;
			}
			m_pRTCP_report_block = new RTCP_header_SR_report_block[m_RTCP_header.rc];
			if (!m_pRTCP_report_block)
			{
				LOG_INFO(configData::getInstance().get_loggerId(),"out of memory in new RTCP_header_report_block");
				iRet = -1;
				break;
			}
			for (unsigned int i = 0; i<m_RTCP_header.rc; i++)
			{
				m_pRTCP_report_block[i].ssrc = m_BitReader.Read32Bits();
				m_pRTCP_report_block[i].fract_lost = m_BitReader.ReadBits(8);
				m_pRTCP_report_block[i].pck_lost[0] = m_BitReader.ReadBits(8);
				m_pRTCP_report_block[i].pck_lost[1] = m_BitReader.ReadBits(8);
				m_pRTCP_report_block[i].pck_lost[2] = m_BitReader.ReadBits(8);
				m_pRTCP_report_block[i].h_seq_no = m_BitReader.Read32Bits();
				m_pRTCP_report_block[i].jitter = m_BitReader.Read32Bits();
				m_pRTCP_report_block[i].last_SR = m_BitReader.Read32Bits();
				m_pRTCP_report_block[i].delay_last_SR = m_BitReader.Read32Bits();
			}
		}
	} while (0);
	return iRet;
}

int RTCP_process::parseRR()
{
	int iRet = 0;
	do
	{
		m_RTCP_header_SR.ssrc = m_BitReader.ReadBits(32);
		if (m_RTCP_header.rc>0)
		{
			if (m_RTCP_header.rc>16)
			{
				LOGW("RTCP header rc may be too big:" << m_RTCP_header.rc);
			}
			if (m_pRTCP_report_block)
			{
				delete[]m_pRTCP_report_block;
				m_pRTCP_report_block = 0;
			}
			
			m_pRTCP_report_block = new RTCP_header_SR_report_block[m_RTCP_header.rc];
			if (!m_pRTCP_report_block)
			{
				LOG_INFO(configData::getInstance().get_loggerId(),"out of memory in new RTCP_header_report_block");
				iRet = -1;
				break;
			}
			for (unsigned int i = 0; i<m_RTCP_header.rc; i++)
			{
				m_pRTCP_report_block[i].ssrc = m_BitReader.Read32Bits();
				m_pRTCP_report_block[i].fract_lost = m_BitReader.ReadBits(8);
				m_pRTCP_report_block[i].pck_lost[0] = m_BitReader.ReadBits(8);
				m_pRTCP_report_block[i].pck_lost[1] = m_BitReader.ReadBits(8);
				m_pRTCP_report_block[i].pck_lost[2] = m_BitReader.ReadBits(8);
				m_pRTCP_report_block[i].h_seq_no = m_BitReader.Read32Bits();
				m_pRTCP_report_block[i].jitter = m_BitReader.Read32Bits();
				m_pRTCP_report_block[i].last_SR = m_BitReader.Read32Bits();
				m_pRTCP_report_block[i].delay_last_SR = m_BitReader.Read32Bits();
			}
		}
	} while (0);

	return iRet;
}

int RTCP_process::parseSDES()
{
	LOGW("not processed");
	return 0;
}

int RTCP_process::parseBYE()
{
	LOGW("not processed");
	return 0;
}

int RTCP_process::parseAPP()
{
	LOGW("not processed");
	return 0;
}

const char * RTCP_process::get_CNAME(int & len)
{
	static bool geted_name = false;
	static char CNAME[100];
	static int cname_len = 0;
	if (!geted_name)
	{
		memset(CNAME, 0, sizeof(char) * 100);
		geted_name = true;
		gethostname(CNAME, 100);
		cname_len = strlen(CNAME);
		memmove(CNAME + 2, CNAME, strlen(CNAME));
		CNAME[0] = 1;
		CNAME[1] = cname_len;
	}
	len = cname_len;
	return CNAME;
}

