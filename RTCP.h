#pragma once

typedef enum
{
	RTCP_SR = 200,		//sender report
	RTCP_RR = 201,		//receiver report
	RTCP_SDES = 202,	//source description
	RTCP_BYE = 203,	//goodbye
	RTCP_APP = 204,	//application defined
}rtcp_pkt_type;

typedef enum
{
	CNAME = 1,
	NAME = 2,
	EMAIL = 3,
	PHONE = 4,
	LOC = 5,
	TOOL = 6,
	NOTE = 7,
	PRIV = 8
}rtcp_info;

typedef struct _RTCP_header
{
#if (BYTE_ORDER==LITTLE_ENDIAN)
	unsigned int rc : 5;		//reception report count
	unsigned int padding : 1;
	unsigned int version : 2;
#elif (BYTE_ORDER==BIG_ENDIAN)
	unsigned int version : 2;
	unsigned int padding : 1;
	unsigned int rc : 5;		//reception report count
#else
#endif
	unsigned int pt : 8;			//packet type
	unsigned int length : 16;
}RTCP_header;

typedef struct _RTCP_header_SR
{
	unsigned int ssrc;	//RTCP SSRC;
	unsigned int ntp_timestamp_MSW;	//MSW
	unsigned int ntp_timestamp_LSW;	//LSW
	unsigned int rtp_timestamp;
	unsigned int pkt_count;
	unsigned int octet_count;
}RTCP_header_SR;

typedef struct _RTCP_header_RR
{
	unsigned int ssrc;
}RTCP_header_RR;

typedef struct _RTCP_header_SR_report_block
{
	unsigned int	ssrc;		//SSRC of RTP session;
	unsigned char	fract_lost;//RR 0;
	unsigned char	pck_lost[3];//24bit lost rtp;
	unsigned int	h_seq_no;//low 16bit max rtp ssrc,16 bit extern;
	unsigned int	jitter;//RTP 数据报文interarrival 时间的统计方差的估值
	unsigned int	last_SR;//最近的SR时间戳
	unsigned int	delay_last_SR;//从接收到从源SSRC_n 来的上一个SR 到发送本接收报告块的间隔，表示为1/65536 秒一个单元。如果尚未收到SR，DLSR 域设置为零
}RTCP_header_SR_report_block;

typedef struct _RTCP_header_SDES
{
	unsigned int	ssrc;
	unsigned char	attr_name;
	unsigned char	len;
}RTCP_header_SDES;

typedef struct _RTCP_header_BYE
{
	unsigned int	ssrc;
	unsigned char	length;
}RTCP_header_BYE;

//生成RR包时需要的参数,仅仅为了减少形参列表;
typedef struct _RTCP_RR_generate_param
{

}RTCP_RR_generate_param;