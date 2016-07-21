#pragma once
#include <stdio.h>
#include "fileParser.h"
#include "base64.h"
#include "BitReader.h"
#include "flvParser.h"

enum h264_nal_type
{
	h264_nal_slice = 1,
	h264_nal_slice_dpa = 2,
	h264_nal_slice_dpb = 3,
	h264_nal_slice_dpc = 4,
	h264_nal_slice_idr = 5,
	h264_nal_sei = 6,
	h264_nal_sps = 7,
	h264_nal_pps = 8,
	h264_nal_aud = 9,
	h264_nal_filter= 12,
};

class h264FileSdpGenerator:public fileSdpGenerator
{
public:
	static h264FileSdpGenerator* createNew(const char *file_name);
	virtual ~h264FileSdpGenerator(void);
	virtual int getSdpLines(char *sdp_lines,size_t sdp_buf_size);
	virtual const char *sdpMediaType();
	virtual int rtpPlyloadType();
	virtual const char *rtpmapLine();
	virtual const char *rangeLine();
private:
	h264FileSdpGenerator();
	virtual int initializeFile(const char *file_name);
	h264FileSdpGenerator(h264FileSdpGenerator&);
	char* get_frame(char* ptr_data,int buf_size,char **ptr_frame,int &frame_len);

	FILE *m_fp;
	char *m_sps;
	int	 m_sps_len;
	char *m_pps;
	int m_pps_len;
	int m_payload_type;
	//CBitReader	m_bitReader;
};

class flvFileSdpGenerator:public fileSdpGenerator
{
public:
	static flvFileSdpGenerator* createNew(const char *file_name);
	virtual ~flvFileSdpGenerator();
	virtual int getSdpLines(char *sdp_lines, size_t sdp_buf_size);
	virtual const char *sdpMediaType();
	virtual int rtpPlyloadType();
	virtual const char *rtpmapLine();
	virtual const char *rangeLine();
private:
	flvFileSdpGenerator();
	virtual int initializeFile(const char *file_name);
	flvParser *m_flvParser;

	std::string m_rangeLine;
	double m_duration;
	int m_payload_type;
	DataPacket m_sps;
	DataPacket m_pps;
};


void	emulation_prevention(unsigned char *pData,int &len);