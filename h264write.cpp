#include "h264write.h"
#include "socketEvent.h"
#include "log4z.h"

h264FileWriter::h264FileWriter(const char *filename):m_fp(0),m_sps(0),m_sps_len(0),
	m_pps(0),m_pps_len(0),m_sps_pps_seted(false)
{
	m_fp=fopen(filename,"wb");
	//LOG_INFO(configData::getInstance().get_loggerId(),"fopen" << filename);
}

h264FileWriter::~h264FileWriter()
{
	deletePointerArray(m_sps);
	deletePointerArray(m_pps);
}

void h264FileWriter::set_sps_data(unsigned char* data,int len)
{
	deletePointerArray(m_sps);
	m_sps=new unsigned char[len];
	memcpy(m_sps,data,len);
	m_sps_len=len;
	if (m_sps&&m_pps)
	{
		m_sps_pps_seted=true;
		write_sps_pps();
	}
}
void h264FileWriter::set_pps_data(unsigned char* data,int len)
{
	deletePointerArray(m_pps);
	m_pps=new unsigned char[len];
	memcpy(m_pps,data,len);
	m_pps_len=len;
	if (m_sps&&m_pps)
	{
		m_sps_pps_seted=true;
		write_sps_pps();
	}
}

void h264FileWriter::write_split()
{
	if (m_fp)
	{
		unsigned char bit;
		bit=0x00;
		for (int i=0;i<3;i++)
		{
			fwrite(&bit,1,1,m_fp);
		}
		bit=0x01;
		fwrite(&bit,1,1,m_fp);
	}
}

void h264FileWriter::add_frame(unsigned char* data,int len)
{
	if (!m_fp||!m_sps_pps_seted)
	{
		return;
	}
	write_split();
	fwrite(data,len,1,m_fp);
}

void h264FileWriter::append_data(unsigned char* data,int len)
{
	if (!m_fp||!m_sps_pps_seted)
	{
		return;
	}
	fwrite(data,len,1,m_fp);
}

void	h264FileWriter::write_sps_pps()
{
	if (!m_fp)
	{
		return;
	}
	write_split();
	fwrite(m_sps,m_sps_len,1,m_fp);
	write_split();
	fwrite(m_pps,m_pps_len,1,m_fp);
}