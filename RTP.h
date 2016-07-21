#pragma once

#include <list>

typedef struct _RTP_header
{
#if (BYTE_ORDER==LITTLE_ENDIAN)
	unsigned int cc : 4;
	unsigned int extension : 1;
	unsigned int padding : 1;
	unsigned int version : 2;
#else
	unsigned int version : 2;
	unsigned int padding : 1;
	unsigned int extension : 1;
	unsigned int cc : 4;
#endif 
#if (BYTE_ORDER==LITTLE_ENDIAN)
	unsigned int payload_type : 7;
	unsigned int marker : 1;
#else 
	unsigned int marker : 1;
	unsigned int payload_type : 7;
#endif
	unsigned short seq;
	unsigned int ts;
	unsigned int ssrc;
	//unsigned int csrc[16];
}RTP_header;

typedef struct _RTP_header_extension
{
	unsigned short profile : 16;
	unsigned short length : 16;
	unsigned char*	header_extension;
}RTP_header_extension;


struct DataPacket
{
	DataPacket();
	~DataPacket();
	int  size;
	char *data;
};

enum flvPacketType
{

	flv_pkt_audio,
	flv_pkg_video,
	raw_h264_frame,
};


struct timedPacket
{
	unsigned long timestamp;
	flvPacketType packetType;
	std::list<DataPacket>	data;
};

