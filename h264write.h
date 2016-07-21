#ifndef _H264_WRITE_H_
#define _H264_WRITE_H_
#include <stdio.h>
#include <stdlib.h>

class h264FileWriter
{
public:
	h264FileWriter(const char *filename);
	~h264FileWriter();
	void	set_sps_data(unsigned char* data,int len);
	void	set_pps_data(unsigned char* data,int len);
	void	add_frame(unsigned char* data,int len);
	void	append_data(unsigned char* data,int len);
private:
	h264FileWriter(h264FileWriter&);
	void	write_sps_pps();
	void	write_split();
	FILE	*m_fp;
	unsigned char	*m_sps;
	int m_sps_len;
	unsigned char	*m_pps;
	int m_pps_len;
	bool	m_sps_pps_seted;
};

#endif
