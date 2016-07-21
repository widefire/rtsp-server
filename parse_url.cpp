#include "RTSP_server.h"

int parse_url(const char *url,char *server,size_t server_len,unsigned short &port,char *filename,size_t filename_len)
{
	int iRet=-1;
	char *full=new char[strlen(url)+1];
	strcpy(full,url);
	do 
	{
		if (strncmp(full,"rtsp://",7)==0)
		{
			char *token=0;
			int has_port=0;
			char *aSubStr=new char[strlen(url)+1];
			strcpy(aSubStr,&full[7]);
			//'rtsp://'后，下一个'/'之前的string，即IP和端口（如果有）;
			if (strchr(aSubStr,'/'))
			{
				int len=0;
				unsigned short i=0;
				char *end_ptr=0;
				end_ptr=strchr(aSubStr,'/');
				len=end_ptr-aSubStr;
				for (;i<strlen(url);i++)
				{
					aSubStr[i]=0;
				}
				strncpy(aSubStr,&full[7],len);
			}
			if (strchr(aSubStr,':'))
			{
				has_port=1;
			}
			deletePointerArray(aSubStr);

			token=strtok(&full[7]," :/\t\n");
			if (token)
			{
				strncpy(server,token,server_len);
				if (server[server_len-1])
				{
					iRet=-1;
					break;
				}
				//获取端口号;
				if (has_port)
				{
					char *port_str=strtok(&full[strlen(server)+7+1]," /\t\n");
					if (port_str)
					{
						port=(unsigned short)atoi(port_str);
					}
					else
					{
						port=RTSP_DEFAULT_PORT;
					}
				}else
				{
					port=RTSP_DEFAULT_PORT;
				}
				iRet=0;
				token=strtok(0," ");
				if (token)
				{
					strncpy(filename,token,filename_len);
					if (filename[filename_len-1])
					{
						iRet=-1;
						break;
					}
				}else
				{
					filename[0]='\0';
				}
			}
		}
		else
		{
			//尝试把它作为一个文件名;
			char *token=strtok(full," \t\n");
			if (token)
			{
				strncpy(filename,token,filename_len);
				if (filename[filename_len-1])
				{
					iRet=-1;
					break;
				}
				server[0]='\0';
				iRet=0;
			}
		}
	} while (0);
	deletePointerArray(full);
	return iRet;
}