#include "RTSP_server.h"

unsigned int convert_to_RTP_timestamp(timeval tv, int freq)
{
	unsigned int rtptimestamp = freq*tv.tv_sec;
	rtptimestamp += (unsigned int)(freq*(tv.tv_usec / 1000000.0) + 0.5);
	return rtptimestamp;
}