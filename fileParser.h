#pragma once
class fileSdpGenerator
{
public:
	static  fileSdpGenerator* createNew(const char *file_name);
	virtual ~fileSdpGenerator(void);
	//return 0 sucessed;
	virtual int getSdpLines(char *sdp_lines,size_t sdp_buf_size)=0;
	virtual const char *sdpMediaType()=0;
	virtual int rtpPlyloadType()=0;
	virtual const char *rtpmapLine()=0;
	virtual const char *rangeLine()=0;

protected:
	fileSdpGenerator(void);
private:
	//return 0 sucessed;
	virtual int	initializeFile(const char *file_name)=0;
	fileSdpGenerator(fileSdpGenerator&);
};
