#pragma once

#include "util.h"

#include <string>
#include <map>
#include <mutex>
#include <list>
#include <stdio.h>
#ifdef WIN32
#include <io.h>
#include <process.h>
#else
#include <unistd.h>
#endif // WIN32


/*  r 打开只读文件，该文件必须存在。
r + 打开可读写的文件，该文件必须存在。
w 打开只写文件，若文件存在则文件长度清为0，即该文件内容会消失。若文件不存在则建立该文件。
w + 打开可读写文件，若文件存在则文件长度清为零，即该文件内容会消失。若文件不存在则建立该文件。
a 以附加的方式打开只写文件。若文件不存在，则会建立该文件，如果文件存在，写入的数据会被加到文件尾，即文件原先的内容会被保留。
a + 以附加方式打开可读写的文件。若文件不存在，则会建立该文件，如果文件存在，写入的数据会被加到文件尾后，即文件原先的内容会被保留。
*/
//不考虑已经有人在读，然后写的情况，只考虑只读或写时读的情况
//使用w+或wb+方式写文件

//读文件时使用缓存机制，一次读入大量数据，避免频繁读文件

#define USE_CACHE_READ_MODE 1
#define MIN_FILE_CACHE_SIZE 1024*1024

enum fileOpenMode
{
	open_read,
	open_readBinary,
	open_write,
	open_writeBinary
};

class fileHander;

struct fileMutex
{
	fileMutex();
	ReadWriteLock fileLock;
	int m_ref;
};
class fileHander
{
public:
	fileHander(std::string fileName, fileOpenMode openMode);
	bool openSuccessed();
	//由使用者自己申请空间。读取数据，返回读取的数据量
	int readFile(int size, char *destBuf);
	//写入数据，返回写入的数据量
	int writeFile(int size, char *srcBuf);
	//当前位置
	long long curFile();
	//seek 成功返回0
	int seek(long long offset, int origin);
	long long fileSize();
	~fileHander();

private:
	std::string getOpenMode(fileOpenMode openMode);
	fileHander(const fileHander&) = delete;
	fileHander& operator=(const fileHander&) = delete;
	FILE	*m_fp;
	std::string m_fileName;
	fileOpenMode m_openMode;
	bool	m_openSuccessed;
	bool	m_hasWriter;
	//文件位置
	long long	m_curFile;
	static ReadWriteLock	s_rwMapLock;
	static std::map<std::string, fileMutex*>	s_rwMap;
#ifdef USE_CACHE_READ_MODE
	//seek to 绝对位置，重新读取buffer,已经上锁
	int seekToPos(long pos);
	long long	m_fileSize;
	//缓存相对于文件的位置
	long long	m_cacheStartPos;
	//相对于缓存的位置
	long long	m_readStartPos;
	//可能不等于配置文件中的缓存大小，因为可能有关键帧过大或者文件本身过小
	int		m_readCacheSize;
	char	*m_cacheData;
#endif // USE_CACHE_READ_MODE

};