#include "tcpCacher.h"


tcpCacherManager::tcpCacherManager()
{
}

tcpCacherManager::~tcpCacherManager()
{
	for (m_Iterator=m_tcpCacherMap.begin();m_Iterator!=m_tcpCacherMap.end();++m_Iterator)
	{
		if (m_Iterator->second)
		{
			delete m_Iterator->second;
			m_Iterator->second = 0;
		}
	}
	m_tcpCacherMap.clear();
}

int tcpCacherManager::appendBuffer(unsigned int sockId, char * dataIn, unsigned int size)
{
	//是否已经存在
	m_Iterator = m_tcpCacherMap.find(sockId);
	if (m_Iterator==m_tcpCacherMap.end())
	{
		//不存在，则添加一个
		tcpCacher *tmpCacher = new tcpCacher();
		m_tcpCacherMap.insert(std::pair<unsigned int, tcpCacher*>(sockId, tmpCacher));
		m_Iterator = m_tcpCacherMap.find(sockId);
	}
	int ret = m_Iterator->second->appendBuffer(dataIn, size);
	return ret;
}

int tcpCacherManager::getBuffer(unsigned int sockId, char * dataOut, unsigned int size, bool remove)
{
	//如果不存在，返回错误
	m_Iterator = m_tcpCacherMap.find(sockId);
	if (m_Iterator == m_tcpCacherMap.end())
	{
		return 404;
	}
	int ret = m_Iterator->second->getBuffer(dataOut, size, remove);
	return ret;
}

int tcpCacherManager::getValidBuffer(unsigned int sockId, std::list<DataPacket>& dataList, tcpBufAnalyzer analyzer)
{
	//如果不存在，返回错误
	m_Iterator = m_tcpCacherMap.find(sockId);
	if (m_Iterator == m_tcpCacherMap.end())
	{
		return 404;
	}
	int ret = m_Iterator->second->getValidBuffer(dataList, analyzer);
	return ret;
}

void tcpCacherManager::removeCacher(unsigned int sockId)
{
	//如果不存在，返回错误
	m_Iterator = m_tcpCacherMap.find(sockId);
	if (m_Iterator == m_tcpCacherMap.end())
	{
		return ;
	}

	delete m_Iterator->second;
	m_Iterator->second = 0;

	m_tcpCacherMap.erase(sockId);
}
