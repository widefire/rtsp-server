#include "RTSP_server.h"

int RTSP_OPTIONS(LP_RTSP_buffer rtsp)
{
	int iRet=0;
	do 
	{
		char *p=0;
		char trash[255];
		char url[255];
		char method[255];
		char version[255];

		//»ñÈ¡CSeq
		if ((p=strstr(rtsp->in_buffer,HDR_CSEQ))==0)
		{
			RTSP_send_err_reply(400,0,rtsp);
			iRet=-1;
			break;
		}
		else
		{
			if (sscanf(p,"%254s %d",trash,&rtsp->rtsp_seq)!=2)
			{
				RTSP_send_err_reply(400,0,rtsp);
				iRet=-1;
				break;
			}
		}

		if(sscanf(rtsp->in_buffer,"%31s %255s %31s",method,url,version)!=3)
		{
			iRet=-1;
			break;
		}

		iRet=RTSP_OPTIONS_reply(rtsp);

	} while (0);
	return iRet;
}

int RTSP_OPTIONS_reply(LP_RTSP_buffer rtsp)
{
	int iRet=0;
	do 
	{
		char reply[1024];
		sprintf(reply,"%s %d %s" RTSP_EL"CSeq: %d" RTSP_EL,RTSP_VER,200,get_stat(200),rtsp->rtsp_seq);
		strcat(reply,"Public: OPTIONS,DESCRIBE,SETUP,PLAY,PAUSE,TEARDOWN" RTSP_EL);
		strcat(reply,RTSP_EL);

		iRet=RTSP_add_out_buffer(reply,strlen(reply),rtsp);
		if (iRet!=0)
		{
			break;
		}
	} while (0);
	return iRet;
}