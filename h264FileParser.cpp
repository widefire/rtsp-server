#include "h264FileParser.h"
#include "log4z.h"
#include "configData.h"

h264FileSdpGenerator* h264FileSdpGenerator::createNew(const char *file_name)
{
	h264FileSdpGenerator *parser=new h264FileSdpGenerator();
	int iRet=parser->initializeFile(file_name);
	if (iRet!=0)
	{
		delete parser;
		parser=0;
	}
	return parser;
}

h264FileSdpGenerator::h264FileSdpGenerator():m_fp(0),m_sps(0),m_pps(0),m_sps_len(0),m_pps_len(0),m_payload_type(96)
{

}

h264FileSdpGenerator::~h264FileSdpGenerator(void)
{
	if (m_fp)
	{
		fclose(m_fp);
		LOGT("fclose" << m_fp);
		m_fp=0;
	}
	if (m_sps)
	{
		delete []m_sps;
		m_sps=0;
	}
	if (m_pps)
	{
		delete []m_pps;
		m_pps=0;
	}
}


int h264FileSdpGenerator::initializeFile(const char *file_name)
{
	int iRet=0;
	char *file_buffer=0;
	do 
	{
		m_fp=fopen(file_name,"rb");
		LOGT("fopen" << file_name);
		if (!m_fp)
		{
			iRet=-1;
			break;
		}
		//read total file length,if file too large,read 1024*1024byte only;just for sps pps;
		const int max_size_sps_pps=1024*1024;
		fseek(m_fp,0,SEEK_END);
		int fileLength=ftell(m_fp);
		fseek(m_fp,0,SEEK_SET);
		int buf_size=0;
		buf_size=(fileLength>max_size_sps_pps)?max_size_sps_pps:fileLength;
		if (buf_size<=0)
		{
			iRet=-1;
			break;
		}
		file_buffer=new char[buf_size];
		if (!file_buffer)
		{
			printf("out of memory in h264FileParser::initializeFile.size=%d\n",buf_size);
			iRet=-1;
			break;
		}
		if(fread(file_buffer,buf_size,1,m_fp)!=1)
		{
			printf("fread failed \n");
			iRet=-1;
			break;
		}

		char *ptr_end=get_frame(file_buffer,buf_size,&m_sps,m_sps_len);
		if (ptr_end==0||!m_sps||m_sps_len<=0)
		{
			iRet=-1;
			break;
		}
		if ((m_sps[0]&0x1f)!=h264_nal_sps)
		{
			iRet=-1;
			break;
		}

		get_frame(ptr_end,buf_size-(ptr_end-file_buffer),&m_pps,m_pps_len);
		if (iRet!=0||!m_pps||m_pps_len<=0)
		{
			iRet=-1;
			break;
		}
		if ((m_pps[0]&0x1f)!=h264_nal_pps)
		{
			iRet=-1;
			break;
		}

	} while (0);
	if (m_fp!=0)
	{
		fclose(m_fp);
		LOGT("fclose" << m_fp);
		m_fp=0;
	}
	if (file_buffer)
	{
		delete []file_buffer;
		file_buffer=0;
	}
	return iRet;
}

char* h264FileSdpGenerator::get_frame(char *ptr_data,int buf_size,char **ptr_frame,int &frame_len)
{
	int iRet=0;
	int nal_start=0;
	int nal_end=0;
	bool	get_nal=false;
	while(true)
	{
		if (nal_start+4>=buf_size)
		{
			return 0;
		}
		if (ptr_data[nal_start]==0x00&&
			ptr_data[nal_start+1]==0x00)
		{
			if (ptr_data[nal_start+2]==0x01)
			{
				nal_start+=3;
				get_nal=true;
				break;
			}
			else if (ptr_data[nal_start+2]==0x00)
			{
				if (ptr_data[nal_start+3]==0x01)
				{
					nal_start+=4;
					get_nal=true;
					break;
				}
			}
			else
			{
				nal_start+=2;
			}
		}
		nal_start++;
	}
	if (!get_nal)
	{
		return	0;
	}
	get_nal=false;
	nal_end=nal_start;
	while (nal_end<buf_size)
	{
		if (nal_end+4>=buf_size)
		{
			nal_end=buf_size;
			break;
		}
		if (ptr_data[nal_end]==0x00&&
			ptr_data[nal_end+1]==0x00)
		{
			if (ptr_data[nal_end+2]==0x01)
			{
				get_nal=true;
				break;
			}
			else if (ptr_data[nal_end+2]==0x00)
			{
				if (ptr_data[nal_end+3]==0x01)
				{
					get_nal=true;
					break;
				}
			}else
			{
				nal_end+=2;
			}
		}
		nal_end++;
	}
	if (!get_nal)
	{
		return	0;
	}

	char *ptr_end=ptr_data+nal_end;
	frame_len=nal_end-nal_start;

	if (!ptr_frame)
	{
		return 0;
	}
	*ptr_frame=new char[frame_len];
	memcpy(*ptr_frame,ptr_data+nal_start,frame_len);

	return ptr_end;
}

int	h264FileSdpGenerator::getSdpLines(char *sdp_lines,size_t sdp_buf_size)
{
	int iRet=0;
	unsigned char *sps_no_emulation=0;

	do 
	{
		if (!m_sps||!m_pps)
		{
			iRet=-1;
			break;
		}

		sps_no_emulation=new unsigned char[m_sps_len];
		memcpy(sps_no_emulation,m_sps,m_sps_len);
		int no_emulation_len=m_sps_len;
		emulation_prevention(sps_no_emulation,no_emulation_len);
		unsigned int profile_level_id=((sps_no_emulation[1]<<16)|(sps_no_emulation[2]<<8)|sps_no_emulation[3]);
		delete []sps_no_emulation;
		sps_no_emulation=0;

		size_t sps_len,pps_len;
		char *sps_base64=base64_encode((unsigned char*)m_sps,m_sps_len,&sps_len);
		char *pps_base64=base64_encode((unsigned char*)m_pps,m_pps_len,&pps_len);

		char const * fmtpFmt=
		"a=fmtp:%d packetization-mode=1;"
		"profile-level-id=%06X;"
		"sprop-parameter-sets=%s,%s\r\n";

		int fmtpFmtSize=strlen(fmtpFmt)+9+
			sps_len+pps_len;
		if (fmtpFmtSize>sdp_buf_size)
		{
			iRet=-1;
			delete []sps_base64;
			sps_base64=0;
			delete []pps_base64;
			pps_base64=0;
			break;
		}
		sprintf(sdp_lines,fmtpFmt,
			96,
			profile_level_id,
			sps_base64,
			pps_base64);
		delete []sps_base64;
		sps_base64=0;
		delete []pps_base64;
		pps_base64=0;


	} while (0);
	return iRet;
}

const char *h264FileSdpGenerator::sdpMediaType()
{
	return "video";
}

int h264FileSdpGenerator::rtpPlyloadType()
{
	return m_payload_type;
}

const char *h264FileSdpGenerator::rtpmapLine()
{
	return "a=rtpmap:96 H264/90000\r\n";
}

const char *h264FileSdpGenerator::rangeLine()
{
	return "a=range:npt=0-\r\n";
}

void	emulation_prevention(unsigned char *pData,int &len)
{
	int templen = len;
	for (int i = 0; i < templen - 2; i++)
	{
		int ret = (pData[i] ^ 0x00) + (pData[i + 1] ^ 0x00) + (pData[i + 2] ^ 0x03);
		if (0 == ret)
		{
			for (int j = i + 2; j < templen - 1; j++)
			{
				pData[j] = pData[j + 1];
			}
			len--;
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////

flvFileSdpGenerator * flvFileSdpGenerator::createNew(const char * file_name)
{
	flvFileSdpGenerator *generator = new flvFileSdpGenerator();
	int iRet = generator->initializeFile(file_name);
	if (iRet != 0)
	{
		delete generator;
		generator = 0;
	}
	return generator;
}

flvFileSdpGenerator::~flvFileSdpGenerator()
{
	if (m_flvParser)
	{
		delete m_flvParser;
		m_flvParser = nullptr;
	}
	if (m_sps.data)
	{
		delete[]m_sps.data;
		m_sps.data = nullptr;
	}
	if (m_pps.data)
	{
		delete[]m_pps.data;
		m_pps.data = nullptr;
	}
}

int flvFileSdpGenerator::getSdpLines(char * sdp_lines, size_t sdp_buf_size)
{
	int iRet = 0;
	unsigned char *sps_no_emulation = 0;

	do
	{
		if (!m_sps.data || !m_pps.data)
		{
			iRet = -1;
			break;
		}

		sps_no_emulation = new unsigned char[m_sps.size];
		memcpy(sps_no_emulation, m_sps.data, m_sps.size);
		int no_emulation_len = m_sps.size;
		emulation_prevention(sps_no_emulation, no_emulation_len);
		unsigned int profile_level_id = ((sps_no_emulation[1] << 16) | (sps_no_emulation[2] << 8) | sps_no_emulation[3]);
		delete[]sps_no_emulation;
		sps_no_emulation = 0;

		size_t sps_len, pps_len;
		char *sps_base64 = base64_encode((unsigned char*)m_sps.data, m_sps.size, &sps_len);
		char *pps_base64 = base64_encode((unsigned char*)m_pps.data, m_pps.size, &pps_len);

		char const * fmtpFmt =
			"a=fmtp:%d packetization-mode=1;"
			"profile-level-id=%06X;"
			"sprop-parameter-sets=%s,%s\r\n";

		int fmtpFmtSize = strlen(fmtpFmt) + 9 +
			sps_len + pps_len;
		if (fmtpFmtSize>sdp_buf_size)
		{
			iRet = -1;
			delete[]sps_base64;
			sps_base64 = 0;
			delete[]pps_base64;
			pps_base64 = 0;
			break;
		}
		sprintf(sdp_lines, fmtpFmt,
			96,
			profile_level_id,
			sps_base64,
			pps_base64);
		delete[]sps_base64;
		sps_base64 = 0;
		delete[]pps_base64;
		pps_base64 = 0;


	} while (0);
	return iRet;
}

const char * flvFileSdpGenerator::sdpMediaType()
{
	return "video";
}

int flvFileSdpGenerator::rtpPlyloadType()
{
	return m_payload_type;
}

const char * flvFileSdpGenerator::rtpmapLine()
{
	return "a=rtpmap:96 H264/90000\r\n";
}

const char * flvFileSdpGenerator::rangeLine()
{
	m_duration = m_flvParser->duration();
	if (m_duration<=0.00001)
	{
		m_duration = 1.0;
	}
	char tmpChar[100];
	sprintf(tmpChar, "%f", m_duration);
	std::string tmpStr = tmpChar;
	m_rangeLine = "a=range:npt=0-" + tmpStr + "\r\n";
	return m_rangeLine.c_str();
}

flvFileSdpGenerator::flvFileSdpGenerator() :m_flvParser(nullptr),m_payload_type(96)
{
}

int flvFileSdpGenerator::initializeFile(const char * file_name)
{
	int iRet = 0;
	do
	{
		m_flvParser = new flvParser(file_name);
		if (!m_flvParser->inited())
		{
			LOG_ERROR(configData::getInstance().get_loggerId(), "open filed :" << file_name );
			iRet = -1;
			break;
		}
		auto frame = m_flvParser->getNextFrame();
		bool endloop = false;
		while (frame.data.size()!=0)
		{
			for (auto i:frame.data)
			{
				if (i.size<=0)
				{
					continue;
				}
				if (frame.packetType==raw_h264_frame)
				{
					h264_nal_type nalType;
					nalType = (h264_nal_type)(i.data[0] & 0x1f);

					//读取SPS pps
					if (nalType==h264_nal_sps)
					{
						if (m_sps.data)
						{
							delete[]m_sps.data;
							m_sps.data = nullptr;

						}
						m_sps.size = i.size;
						m_sps.data = new char[i.size];
						memcpy(m_sps.data, i.data, i.size);
					}
					else if (nalType==h264_nal_pps)
					{
						if (m_pps.data)
						{
							delete[]m_pps.data;
							m_pps.data = nullptr;
						}
						m_pps.size = i.size;
						m_pps.data = new char[i.size];
						memcpy(m_pps.data, i.data, i.size);
						if (m_sps.size)
						{
							endloop = true;
							break;
						}
					}
				}

				if (i.data)
				{
					delete[]i.data;
					i.data = nullptr;
				}
				if (endloop)
				{
					break;
				}
			}
			if (endloop)
			{
				break;
			}
			frame = m_flvParser->getNextFrame();
		}

		//如果没有SPS pps则失败
		if (m_sps.size<=0||m_pps.size<=0)
		{
			iRet = -1;
			break;
		}
	} while (0);
	return iRet;
}
