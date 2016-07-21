#include "TCP_ConnectingManager.h"
#include "util.h"


TcpConnectingManager::TcpConnectingManager():m_timeout(300)
{
}


TcpConnectingManager::~TcpConnectingManager()
{
}

void TcpConnectingManager::setTimeoutSeconds(unsigned int timeSeconds)
{
	m_timeout = timeSeconds;
}

void TcpConnectingManager::updateTimeInfo(unsigned int id, unsigned long time)
{
	std::lock_guard<std::mutex> guard(m_mutex);

	//m_mapTimeInfo.find(timeInfo)
	auto it = m_mapTimeInfo.find(id);
	if (it==m_mapTimeInfo.end())
	{
		m_mapTimeInfo.insert(std::pair<unsigned int, unsigned int>(id, time));
		it = m_mapTimeInfo.find(id);
	}
	(*it).second = time;
}

void TcpConnectingManager::deleteSocket(unsigned int id)
{
	auto iterator = m_mapTimeInfo.find(id);
	if (iterator!=m_mapTimeInfo.end())
	{
		m_mapTimeInfo.erase(id);
	}
}

std::vector<unsigned> TcpConnectingManager::getTimeoutSockets()
{
	std::lock_guard<std::mutex> guard(m_mutex);
	std::vector<unsigned int> vecOut;

	timeval time;
	gettimeofday(&time, nullptr);
	for (auto i:m_mapTimeInfo)
	{
		if (time.tv_sec-i.second>m_timeout)
		{
			vecOut.push_back(i.first);
		}
	}

	for (auto i:vecOut )
	{
		m_mapTimeInfo.erase(i);
	}
	return vecOut;
}
