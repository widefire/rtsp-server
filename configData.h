#pragma once
#include "log4z.h"
class configData
{
public:
	static configData &getInstance();
	const char *get_server_name();
	const char *get_server_version();
	std::string getFlvSaveDir();
	std::string getMP4SaveDir();
	const int get_loggerId();
	int RTSP_timeout();
	int readFileCacheSize();
	short RTSP_server_port();
	short taskPort();
	short httpFilePort();
	static void setConfigFileName(std::string fileName);
private:
	configData(void);
	configData(configData&);
	virtual ~configData(void);

	void Init();

	std::string m_server_name;
	std::string m_server_version;

	short m_rtspSvrPort;
	short m_taskPort;
	short m_httpFilePort;
	int   m_readFileCache;
	std::string m_flvSaveDir;
	std::string m_mp4SaveDir;
	static std::string s_configFileName;
	static std::string s_rtspServerPort;
	static std::string s_rtspServerName;
	static std::string s_rtspServerVersion;
	static std::string s_taskPort;
	static std::string s_readFileCacheSize;
	static std::string s_flvSaveDir;
	static std::string s_mp4SaveDir;
	static std::string s_logPath;
	static std::string s_httpFilePort;
	LoggerId	m_logger;
	std::string m_logPath;
};

