#pragma once

#include "RTCP.h"
#include "RTP.h"
#include "BitReader.h"



class RTCP_process
{
public:
	RTCP_process();
	RTCP_process(RTCP_process&);
	~RTCP_process();

	//解析RTCP,成功返回0;
	int parse_RTCP_buf(const DataPacket &data_packet);
	//生成RTCP SR,成功返回0;
	static int generate_RTCP_SR_buf(DataPacket &data_packet,unsigned int SSRC,unsigned int packet_count,unsigned int octet_count);
	//生成RTCP RR,成功返回0;
	static int generate_RTCP_RR_buf(DataPacket &data_packet,RTCP_RR_generate_param param);
	//生成RTCP bye,成功返回0;
	static int generate_Bye_Buf(DataPacket &data_packet, unsigned int SSRC);
	//生成RTCP SDES，成功返回0;
	static int generate_SDES_Buf(DataPacket &data_packet, unsigned int SSRC);
	//返回该RTCP的SSRC;
	unsigned int RTCP_SSRC();
	//有效的report block个数;
	int report_block_count();
	//返回客户端发送发送者报告RTP的SSRC;
	unsigned int RTP_ssrc(int index);
private:
	int	parseSR();
	int parseRR();
	int parseSDES();
	int	parseBYE();
	int parseAPP();
	static const char* get_CNAME(int &len);

	rtcp_pkt_type	m_pkt_type;
	RTCP_header		m_RTCP_header;
	RTCP_header_SR	m_RTCP_header_SR;
	RTCP_header_RR	m_RTCP_header_RR;
	RTCP_header_SR_report_block*	m_pRTCP_report_block;
	RTCP_header_SDES*	m_pRTCP_SDES;
	int				m_count_SDES;
	RTCP_header_BYE	m_RTCP_goodbye;

	CBitReader	m_BitReader;
};

