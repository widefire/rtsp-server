#pragma once
#include <list>
#include<map>
#include <mutex>
#include "RTP.h"	//for datapacket

const unsigned int TCP_DEFAULT_CACHER_SIZE = 4096;

//输入需分析的buffer，输出是否产生需要的buffer和需丢弃的字节数
typedef void(*tcpBufAnalyzer)(char* dataIn,unsigned int inSize,bool &getValid,
	unsigned int &outStart, unsigned int &outEnd, unsigned int& invalidBytes);
//解析RTP RTCP RTSP 数据
void parseRtpRtcpRtspData(char* dataIn, unsigned int inSize, bool &getValid,
	unsigned int &outStart, unsigned int &outEnd,unsigned int& invalidBytes);
//解析size+data的taskJson
void parseTaskJsonData(char* dataIn, unsigned int inSize, bool &getValid,
	unsigned int &outStart, unsigned int &outEnd, unsigned int&invalidBytes);

//缓存TCP的流式套接字带来的需缓存的buffer

class tcpCacher
{
public:
	tcpCacher(unsigned int maxBufSize= TCP_DEFAULT_CACHER_SIZE);
	virtual ~tcpCacher();
	//添加buffer,成功返回0;
	int appendBuffer(char *dataIn, unsigned int size);
	//读取buffer,成功返回0;
	int getBuffer(char *dataOut, unsigned int size, bool remove=true);
	//读取有效buffer，移除无效字节,成功返回0;
	int getValidBuffer(std::list<DataPacket>& dataList,tcpBufAnalyzer analyzer);
private:
	tcpCacher(tcpCacher&)=delete;
	tcpCacher& operator=(tcpCacher)=delete;

	//缓存大小
	unsigned int m_MaxBufSize;
	//缓存数据
	char *m_dataBuf;
	//有效数据大小
	unsigned int m_dataSize;
	//有效数据起点
	unsigned int m_cur;
	std::mutex m_dataMutex;
};

typedef std::map<unsigned int, tcpCacher*> tcpCacherMap;

class tcpCacherManager
{
public:
	tcpCacherManager();
	virtual ~tcpCacherManager();
	//添加buffer,成功返回0;
	int appendBuffer(unsigned int sockId, char *dataIn, unsigned int size);
	//读取buffer,成功返回0;
	int getBuffer(unsigned int sockId, char *dataOut, unsigned int size, bool remove = true);
	//读取有效buffer，移除无效字节,成功返回0;
	int getValidBuffer(unsigned int sockId, std::list<DataPacket>& dataList, tcpBufAnalyzer analyzer);
	//移除关闭了的tcp,成功返回0;
	void removeCacher(unsigned int sockId);
private:
	tcpCacherManager(tcpCacherManager&);
	tcpCacherManager& operator=(tcpCacherManager)=delete;

	tcpCacherMap	m_tcpCacherMap;
	tcpCacherMap::iterator m_Iterator;
};
