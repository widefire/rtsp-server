#pragma once

#include <map>
#include <vector>
#include <mutex>

class TcpConnectingManager
{
public:
	TcpConnectingManager();
	virtual ~TcpConnectingManager();
	void setTimeoutSeconds(unsigned int timeSeconds);
	void updateTimeInfo(unsigned int id,unsigned long time);
	void deleteSocket(unsigned int id);
	std::vector<unsigned> getTimeoutSockets(); //获取并移除
private:
	TcpConnectingManager(const TcpConnectingManager&) = delete;
	TcpConnectingManager& operator = (const TcpConnectingManager&) = delete;

	std::map<unsigned, unsigned long> m_mapTimeInfo;
	unsigned int m_timeout;
	std::mutex	m_mutex;	//防止跨线程 update get
};

