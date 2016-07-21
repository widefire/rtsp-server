#include "RTSP_server.h"

int is_supported_file_stuffix(char *p)
{
	if (stricmp(p,".264")==0||
		stricmp(p,".h264")==0||
		stricmp(p,".flv")==0)
	{
		return 0;
	}
	return -1;
}