#include "RTSP_server.h"
#include "fileParser.h"
#include "configData.h"

int generate_SDP_description(char *url,char *descr)
{
	int iRet=0;
	do 
	{
		unsigned int sdp_length=0;
		char sdp_lines[RTSP_DESC_SIZE];
		iRet=generate_SDP_info(url,sdp_lines,RTSP_DESC_SIZE);
		if (iRet!=0)
		{
			break;
		}
		sdp_length+=strlen(sdp_lines);
		//just set duration 0 now;
		//const char *range_line="a=range:npt=0-\r\n";
		const char *range_line = "a=range:npt=0-\r\n";
		
		char *pIp=const_cast<char*>(get_server_ip());
		const char *description_sdp_string="H.264 Video,streamed by RTSP server";
		const char *server_name=configData::getInstance().get_server_name();
		const char *server_version=configData::getInstance().get_server_version();
		const char *source_filter_line="";
		const char *misc_sdp_lines="";
		timeval time_now;
		gettimeofday(&time_now,0);
		const char *urlBad = "can you guess";
		char const* const sdp_prefix_fmt=
			"v=0\r\n"
			"o=- %ld%06ld %d IN IP4 %s\r\n"
			"s=%s\r\n"
			"i=%s\r\n"
			"t=0 0\r\n"
			"a=tool:%s%s\r\n"
			"a=type:broadcast\r\n"
			"a=control:*\r\n"
			"%s"
			"%s"
			"a=x-qt-text-nam:%s\r\n"
			"a=x-qt-text-inf:%s\r\n"
			"%s";
		sdp_length+=strlen(sdp_prefix_fmt)+
			46+strlen(pIp)+
			strlen(range_line)+
			strlen(description_sdp_string)*2+
			strlen(server_name)*2+
			strlen(server_version)+
			strlen(source_filter_line)+
			strlen(misc_sdp_lines);
		sprintf(descr,sdp_prefix_fmt,
			time_now.tv_sec,time_now.tv_usec,1,pIp,
			description_sdp_string,
			urlBad,
			server_name,
			server_version,
			source_filter_line,
			range_line,
			description_sdp_string,
			urlBad,
			//misc_sdp_lines,
			sdp_lines);
	} while (0);
	return	iRet;
}

int generate_SDP_info(char *url,char *sdpLines,size_t sdp_line_size)
{
	int iRet=0;
	fileSdpGenerator *url_file_Parser;
	char sdp_lines[2048];
	do 
	{
		url_file_Parser=fileSdpGenerator::createNew(url);
		if (!url_file_Parser)
		{
			iRet=-1;
			break;
		}
		iRet=url_file_Parser->getSdpLines(sdp_lines,sizeof sdp_lines);
		if (iRet!=0)
		{
			sdp_lines[0]='\0';
			iRet=0;
		}
		const char * media_type=url_file_Parser->sdpMediaType();
		unsigned char rtp_payload_type=url_file_Parser->rtpPlyloadType();
		const char *ip_address="0.0.0.0";
		const char *rtpmap_line=url_file_Parser->rtpmapLine();
		const char *rtcpmux_line="";
		const char *range_line=url_file_Parser->rangeLine();
		//a=control是音视频控制频道，如果只有视频则只有一个，如果音视频都有则有两个，
		//一个连接只能发一个频道的数据，如果要同时收音视频，需要两个连接
		const char *streamId=RTSP_CONTROL_ID_0;
		unsigned int sdp_fmt_leng=0;
		char const* const sdp_fmt=
			"m=%s %u RTP/AVP %d\r\n"
			"c=IN IP4 %s\r\n"
			"b=AS:%u\r\n"
			"%s"
			"%s"
			"%s"
			"%s"
			"a=control:%s\r\n";
		sdp_fmt_leng+=strlen(sdp_fmt)+
			strlen(media_type)+8+
			strlen(ip_address)+
			20+
			strlen(rtpmap_line)+
			strlen(rtcpmux_line)+
			strlen(range_line)+
			strlen(sdp_lines)+
			strlen(streamId);
		if (sdp_fmt_leng>sdp_line_size)
		{
			printf("sdp fmt too larg,\n");
			iRet=-1;
			break;
		}
		sprintf(sdpLines,
			sdp_fmt,
			media_type,
			0,
			rtp_payload_type,
			ip_address,
			500,
			rtpmap_line,
			rtcpmux_line,
			range_line,
			sdp_lines,
			streamId);
	} while (0);
	deletePointer(url_file_Parser);
	return iRet;
}