
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "configData.h"
#include "json11.hpp"
//extern char LOG4Z_DEFAULT_PATH[256];
using namespace zsummer::log4z;
//extern char LOG4Z_DEFAULT_PATH[256];
std::string configData::s_configFileName = "config.json";
std::string configData::s_rtspServerPort = "rtspServerPort";
std::string configData::s_rtspServerName = "rtspServerName";
std::string configData::s_rtspServerVersion = "rtspServerVersion";
std::string configData::s_taskPort = "taskPort";
std::string configData::s_readFileCacheSize = "readFileCacheSize";
std::string configData::s_flvSaveDir = "flvSaveDir";
std::string configData::s_mp4SaveDir = "mp4SaveDir";
std::string configData::s_logPath = "logPath";
std::string configData::s_httpFilePort = "httpFilePort";

configData::configData(void) :m_logger(0), m_rtspSvrPort(0), m_readFileCache(1024 * 1024)
{
	Init();
}


configData::~configData(void)
{
	//ILog4zManager::getRef().stop();
}

void configData::Init()
{
	FILE *fp = fopen(s_configFileName.c_str(), "rb");
	if (fp)
	{
		size_t fileSize;

		fileSize = ftell(fp);
		fseek(fp, 0, SEEK_END);
		fileSize = ftell(fp);
		fseek(fp, 0, SEEK_SET);
		char *data = new char[fileSize + 1];
		data[fileSize] = '\0';
		fread(data, fileSize, 1, fp);
		std::string err;

		json11::Json jsonObj = json11::Json::parse(data, err);
		m_rtspSvrPort = jsonObj[s_rtspServerPort].int_value();
		m_server_name = jsonObj[s_rtspServerName].string_value();
		m_server_version = jsonObj[s_rtspServerVersion].string_value();
		m_taskPort = jsonObj[s_taskPort].int_value();
		m_readFileCache = jsonObj[s_readFileCacheSize].int_value();
		m_flvSaveDir = jsonObj[s_flvSaveDir].string_value();
		m_logPath = jsonObj[s_logPath].string_value();
		m_mp4SaveDir = jsonObj[s_mp4SaveDir].string_value();
		m_httpFilePort = jsonObj[s_httpFilePort].int_value();
		printf(m_logPath.c_str());
		set_LOG4Z_DEFAULT_PATH(m_logPath.c_str());
		fclose(fp);
		fp = nullptr;

		if (m_mp4SaveDir.size()>0)
		{
			if (m_mp4SaveDir.at(m_mp4SaveDir.size()-1)!='/')
			{
				m_mp4SaveDir = m_mp4SaveDir + "/";
			}
		}
		if (m_flvSaveDir.size()>0)
		{
			if (m_flvSaveDir.at(m_flvSaveDir.size() - 1) != '/')
			{
				m_flvSaveDir = m_flvSaveDir + "/";
			}
		}
		if (m_logPath.size()>0)
		{
			if (m_logPath.at(m_logPath.size() - 1) != '/')
			{
				m_logPath =  + "/";
			}
		}
	}
	else
	{
		m_rtspSvrPort = 6554;
		m_server_name = "hss";
		m_server_version = "1.0";
		m_taskPort = 6555;
		m_httpFilePort = 6556;
		m_readFileCache = 1024 * 1024;
	}

	m_logger = 0;
	ILog4zManager::getRef().start();
}

configData &configData::getInstance()
{
	static configData data;
	return data;
}

const char *configData::get_server_name()
{
	return m_server_name.c_str();
}

const char *configData::get_server_version()
{
	return m_server_version.c_str();
}

std::string configData::getFlvSaveDir()
{
	return m_flvSaveDir;
}

std::string configData::getMP4SaveDir()
{
	return m_mp4SaveDir;
}

const int configData::get_loggerId()
{
	return m_logger;
}

int configData::RTSP_timeout()
{
	return 65;
}

int configData::readFileCacheSize()
{
	return m_readFileCache;
}

short configData::RTSP_server_port()
{
	return m_rtspSvrPort;
}

short configData::taskPort()
{
	return m_taskPort;
}

short configData::httpFilePort()
{
	return m_httpFilePort;
}

void configData::setConfigFileName(std::string fileName)
{
	s_configFileName = fileName;
}
