#include "RTSP_server.h"

int RTSP_add_out_buffer(char *buffer,unsigned short len,LP_RTSP_buffer rtsp)
{
#if USE_STD_STRING
	rtsp->out_buffer = buffer;
#else
	if((rtsp->out_size + len)>(int)sizeof rtsp->out_buffer)
{
	return -1;
}
memcpy(&rtsp->out_buffer[rtsp->out_size], buffer, len);
rtsp->out_buffer[rtsp->out_size + len] = '\0';
#endif // USE_STD_STRING
	
	rtsp->out_size+=len;
	return 0;
}