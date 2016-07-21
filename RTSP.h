#ifndef RTSP_H_
#define RTSP_H_

#include "socket_type.h"

#include <string>
#include <list>

enum RTSP_STATE
{
	INIT_STAT,
	READY_STAT,
	PLAY_STAT,
};

#define RTSP_DEFAULT_PORT   554

#define RTSP_VER			"RTSP/1.0"

#define RTSP_EL				"\r\n"
#define RTSP_RTP_AVP		"RTP/AVP"
#define RTSP_RTP_AVP_TCP	"RTP/AVP/TCP"
#define RTSP_RTP_AVP_UDP	"RTP/AVP/UDP"
#define RTSP_RAW_RAW_UDP	"RAW/RAW/UDP"
#define RTSP_CONTROL_ID_0	"track1"
#define RTSP_CONTROL_ID_1	"track2"

#define RTSP_LIVE			"live"

#define HDR_CONTENTLENGTH "Content-Length"
#define HDR_ACCEPT "Accept"
#define HDR_ALLOW "Allow"
#define HDR_BLOCKSIZE "Blocksize"
#define HDR_CONTENTTYPE "Content-Type"
#define HDR_DATE "Date"
#define HDR_REQUIRE "Require"
#define HDR_TRANSPORTREQUIRE "Transport-Require"
#define HDR_SEQUENCENO "SequenceNo"
#define HDR_CSEQ "CSeq"
#define HDR_STREAM "Stream"
#define HDR_SESSION "Session"
#define HDR_TRANSPORT "Transport"
#define HDR_RANGE "Range"	
#define HDR_USER_AGENT "User-Agent"	

#define RTSP_METHOD_MAXLEN 15
#define RTSP_METHOD_DESCRIBE "DESCRIBE"
#define RTSP_METHOD_ANNOUNCE "ANNOUNCE"
#define RTSP_METHOD_GET_PARAMETERS "GET_PARAMETERS"
#define RTSP_METHOD_OPTIONS "OPTIONS"
#define RTSP_METHOD_PAUSE "PAUSE"
#define RTSP_METHOD_PLAY "PLAY"
#define RTSP_METHOD_RECORD "RECORD"
#define RTSP_METHOD_REDIRECT "REDIRECT"
#define RTSP_METHOD_SETUP "SETUP"
#define RTSP_METHOD_SET_PARAMETER "SET_PARAMETER"
#define RTSP_METHOD_TEARDOWN "TEARDOWN"

#define DEFAULT_MTU_2	0xfff

#define RTSP_payload_h264	96


enum RTSP_CMD_STATE
{
	RTSP_ID_BAD_REQ,
	RTSP_ID_DESCRIBE,
	RTSP_ID_ANNOUNCE,
	RTSP_ID_GET_PARAMETERS,
	RTSP_ID_OPTIONS,
	RTSP_ID_PAUSE,
	RTSP_ID_PLAY,
	RTSP_ID_RECORD,
	RTSP_ID_REDIRECT,
	RTSP_ID_SETUP,
	RTSP_ID_SET_PARAMETER,
	RTSP_ID_TEARDOWN,
};

#define SD_STREAM "STREAM"
#define SD_STREAM_END "STREAM_END"
#define SD_FILENAME "FILE_NAME"
#define SD_CLOCK_RATE "CLOCK_RATE"
#define SD_PAYLOAD_TYPE "PAYLOAD_TYPE"
#define SD_AUDIO_CHANNELS "AUDIO_CHANNELS"	
#define SD_ENCODING_NAME "ENCODING_NAME"
#define SD_AGGREGATE "AGGREGATE"
#define SD_BIT_PER_SAMPLE "BIT_PER_SAMPLE"
#define SD_SAMPLE_RATE "SAMPLE_RATE"
#define SD_CODING_TYPE "CODING_TYPE"
#define SD_FRAME_LEN "FRAME_LEN"
#define SD_PKT_LEN "PKT_LEN"	
#define SD_PRIORITY "PRIORITY"	
#define SD_BITRATE "BITRATE"
#define SD_FRAME_RATE "FRAME_RATE"
#define SD_FORCE_FRAME_RATE "FORCE_FRAME_RATE"
#define SD_BYTE_PER_PCKT "BYTE_PER_PCKT"	
#define SD_MEDIA_SOURCE "MEDIA_SOURCE"	
#define SD_TWIN "TWIN"	
#define SD_MULTICAST "MULTICAST"	

#define SD_LICENSE "LICENSE"
#define SD_RDF "VERIFY"  
#define SD_TITLE "TITLE"
#define SD_CREATOR "CREATOR"

const int RTSP_BUFFER_SIZE = 4096;
const int RTSP_DESC_SIZE = 4096;

const int MAX_DIR_PATH = 256;

const int RTP_video_freq = 90000;
const int RTP_audio_freq = 8000;

const int SOCKET_PACKET_SIZE = 1456;

const int RTP_H264_Payload_type = 96;

enum RTSP_TRANSPORT_TYPE
{
	RTSP_TRANSPORT_NO_TYPE,
	RTSP_OVER_TCP,
	RTSP_OVER_UDP,
};

#define USE_STD_STRING 1

typedef struct _RTSP_session
{
	RTSP_STATE			cur_state;
#if USE_STD_STRING
	std::string			session_id;
#else
	char				session_id[64];
#endif // USE_STD_STRING
	unsigned int		ssrc;
	int					RTP_channel;
	int					RTCP_channel;
	unsigned short		RTP_s_port;
	unsigned short		RTCP_s_port;
	unsigned short		RTP_c_port;
	unsigned short		RTCP_c_port;
	RTSP_TRANSPORT_TYPE	transport_type;
	bool				unicast;
	char				control_id[128];
	unsigned short		first_seq;
	unsigned int		start_rtp_time;
	//RTP_session...
}RTSP_session, *LP_RTSP_session;

typedef struct _RTSP_buffer
{
	socket_t			sock_id;
	unsigned int		port;

	char				in_buffer[RTSP_BUFFER_SIZE];
	unsigned int		in_size;
#if USE_STD_STRING
	//std::string in_buffer;
	std::string out_buffer;
#else

	char				out_buffer[RTSP_BUFFER_SIZE + RTSP_DESC_SIZE];
#endif // 
	unsigned int		out_size;

	unsigned int		rtsp_seq;
#if USE_STD_STRING
	std::string			sdp_desc;
#else
	char				sdp_desc[RTSP_DESC_SIZE];
#endif // USE_STD_STRING
	char				source_name[MAX_DIR_PATH];
	in_addr				sock_addr;
	std::list<LP_RTSP_session>	session_list;
	unsigned int		session_time;
}RTSP_buffer, *LP_RTSP_buffer;

//深度复制一个session,需要被释放;
LP_RTSP_session deepcopy_RTSP_session(LP_RTSP_session session_in);
//深度复制一个RTSP buffer，需要被释放;
LP_RTSP_buffer deepcopy_RTSP_buffer(LP_RTSP_buffer buff_in);
//释放一个RTSP buffer;
void	free_RTSP_buffer(LP_RTSP_buffer buff_in);
#endif
