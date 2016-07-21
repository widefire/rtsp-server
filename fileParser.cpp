#include <string.h>
#include <stdio.h>
#include "fileParser.h"
#include "h264FileParser.h"
#include "socket_type.h"


fileSdpGenerator::fileSdpGenerator(void)
{
}


fileSdpGenerator::~fileSdpGenerator(void)
{
}

fileSdpGenerator* fileSdpGenerator::createNew(const char *file_name)
{
	fileSdpGenerator *parser=0;
	/*char prefix[256],suffix[256];
	if(2!=sscanf(file_name,"%[^.].%s",prefix,suffix))
	{
		return 0;
	}*/
	const char *pStuffix = nullptr;
	for (int i = strlen(file_name); i>1 ; i--)
	{
		if (file_name[i]=='.')
		{
			pStuffix = file_name + i + 1;
			break;
		}
	}
	if (pStuffix==nullptr)
	{
		return nullptr;
	}
	if (stricmp(pStuffix,"264")==0||stricmp(pStuffix,"h264")==0)
	{
		parser=h264FileSdpGenerator::createNew(file_name);
	}
	else if (stricmp(pStuffix, "flv")==0)
	{
		//parser=
		parser = flvFileSdpGenerator::createNew(file_name);
	}
	return parser;
}
