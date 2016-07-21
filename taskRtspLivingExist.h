#pragma once
#include "task_schedule_obj.h"
#include <string>
class taskRtspLivingExist :
	public taskScheduleObj
{
public:
	taskRtspLivingExist(std::string streamName);
	virtual task_schedule_type getType();
	std::string getStreamName();
	void setLivingExist(bool exist);
	bool getLivingExist();
	virtual ~taskRtspLivingExist();
private:
	std::string	m_streamName;
	bool m_livingExist;
};


class taskRtspLivingMediaType :public taskScheduleObj
{
public:
	taskRtspLivingMediaType(std::string streamName);
	std::string getStreamName();
	virtual task_schedule_type getType();
	void setMediaType(int mediaType);
	int  getMediaType();
	virtual ~taskRtspLivingMediaType();
private:
	std::string m_streamName;
	int m_mediaType;
};


class taskRtspLivingSdp :
	public taskScheduleObj
{
public:
	taskRtspLivingSdp(std::string streamName);
	std::string getStreamName();
	virtual task_schedule_type getType();
	void setSdp(std::string strSdp);
	bool getSdp(std::string &strSdp);
	virtual ~taskRtspLivingSdp();
private:
	bool m_sdpSeted;
	std::string m_streamName;
	std::string m_sdp;
};


