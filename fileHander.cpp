#include "fileHander.h"
#include "configData.h"
#include <utility>
#include <string.h>

ReadWriteLock	fileHander::s_rwMapLock;
std::map<std::string, fileMutex*>	fileHander::s_rwMap;

fileHander::fileHander(std::string fileName, fileOpenMode openMode) :m_fp(0),
m_fileName(fileName), m_openMode(openMode), m_openSuccessed(false), m_hasWriter(false),
m_curFile(0), m_readCacheSize(0), m_fileSize(0), m_cacheStartPos(0), m_readStartPos(0), m_cacheData(0)
{
#ifdef USE_CACHE_READ_MODE
	m_readCacheSize = configData::getInstance().readFileCacheSize();
	if (m_readCacheSize < MIN_FILE_CACHE_SIZE)
	{
		m_readCacheSize = MIN_FILE_CACHE_SIZE;
	}
#endif

	//文件名和模式长度够不够
	if (m_fileName.size() <= 0)
	{
		return;
	}

	
	s_rwMapLock.acquireWriter();
	if (m_openMode == open_read || m_openMode == open_readBinary )
	{
		std::string flag = getOpenMode(m_openMode);
		//m_openSuccessed = (0 == access(m_fileName.c_str(), 0));
		m_fp = fopen(m_fileName.c_str(), flag.c_str());
		if (m_fp==nullptr)
		{
			m_openSuccessed = false;
		}
		else 
		{
			m_openSuccessed = true;
		}
	}
	else
	{

		//m_openSuccessed = (0 != access(m_fileName.c_str(), 0));
		if (0!=access(m_fileName.c_str(),0))
		{
			//创一个空文件
			FILE *fp = fopen(m_fileName.c_str(), "w");
			if (fp)
			{
				fclose(fp);
				fp = nullptr;
			}
		}
		m_openSuccessed = true;
		std::string flag = getOpenMode(m_openMode);
		m_fp = fopen(m_fileName.c_str(), flag.c_str());
	}
	auto it = s_rwMap.find(m_fileName);
	if (it != s_rwMap.end())
	{
		it->second->m_ref++;
	}
	else
	{
		fileMutex *tmpFileMutex = new fileMutex();
		tmpFileMutex->m_ref++;
		s_rwMap.insert(std::pair<std::string, fileMutex*>(m_fileName, tmpFileMutex));
	}

	it = s_rwMap.find(m_fileName);
	it->second->fileLock.acquireReader();
	if (m_openMode==open_read||m_openMode==open_readBinary)
	{
		seekToPos(0);
	}
	it->second->fileLock.releaseReader();
	s_rwMapLock.releaseWriter();

}

bool fileHander::openSuccessed()
{
	return m_openSuccessed;
}

int fileHander::readFile(int size, char * destBuf)
{
	if (m_openMode == open_write || m_openMode == open_writeBinary)
	{
		LOG_ERROR(configData::getInstance().get_loggerId(), "write mode file,should not read");
		return -1;
	}
	//LOG_WARN(configData::getInstance().get_loggerId(), "enter");
	s_rwMapLock.acquireReader();
	auto it = s_rwMap.find(m_fileName);
	do
	{
		if (it==s_rwMap.end())
		{
			LOG_FATAL(configData::getInstance().get_loggerId(), "it it impossible");
			size = -1;
			break;
		}
		it->second->fileLock.acquireReader();
	
		if (size > m_readCacheSize)
		{
			//重新分配缓存空间
			m_readCacheSize = size;
			//重新读取buffer，跳转到指定位置
			if (0 != seekToPos(m_cacheStartPos + m_readStartPos))
			{
				LOG_ERROR(configData::getInstance().get_loggerId(), "change cache size bigger failed");
				size = -1;
				break;
			}
		}
#if USE_CACHE_READ_MODE
		//没有足够的缓存
		if (m_readCacheSize - m_readStartPos<size)
		{
			if (0 != seekToPos(m_cacheStartPos + m_readStartPos))
			{
				LOG_ERROR(configData::getInstance().get_loggerId(), "seek failed");
				size = -1;
				break;
			}
		}
		if (!m_readCacheSize || !m_cacheData)
		{
			size = -1;
			break;
		}
		memcpy(destBuf, m_cacheData + m_readStartPos, size);
		m_readStartPos += size;
#endif // USE_CACHE_READ_MODE
	} while (0);
	if (it != s_rwMap.end())
	{
		it->second->fileLock.releaseReader();
	}
	//LOG_WARN(configData::getInstance().get_loggerId(), "leave");
	s_rwMapLock.releaseReader();
	return size;
}

int fileHander::writeFile(int size, char * srcBuf)
{
	if (m_openMode != open_write && m_openMode != open_writeBinary)
	{
		LOG_ERROR(configData::getInstance().get_loggerId(), "read mode file,should not write");
		return -1;
	}
	int iRet = -1;
	s_rwMapLock.acquireReader();
	auto it = s_rwMap.find(m_fileName);
	do
	{
		if (it==s_rwMap.end())
		{
			LOG_FATAL(configData::getInstance().get_loggerId(), "it it impossible");
			iRet = -1;
			break;
		}
		it->second->fileLock.acquireReader();
		std::string mode;
		if (m_openMode==open_write)
		{
			mode = "r+";
		}
		else if(m_openMode==open_writeBinary)
		{
			mode = "rb+";
		}
		else
		{
			iRet = -1;
			break;
		}
		//m_fp = fopen(m_fileName.c_str(), mode.c_str());
		if (m_fp==nullptr)
		{
			LOG_ERROR(configData::getInstance().get_loggerId(), "open file:" << m_fileName << " failed");
			iRet = -1;
			break;
		}
		if (0 != fileSeek(m_fp, m_curFile, SEEK_SET))
		{
			LOG_ERROR(configData::getInstance().get_loggerId(), "seek file:" << m_fileName << " failed");
			iRet = -1;
			break;
		}
		iRet = fwrite(srcBuf, 1, size, m_fp);
		if (iRet!=size)
		{
			LOG_ERROR(configData::getInstance().get_loggerId(), "write file:" << m_fileName << " failed");
			iRet = -1;
			break;
		}
		//fflush(m_fp);
		m_curFile = fileTell(m_fp);
		/*fclose(m_fp);
		m_fp = nullptr;*/
	} while (0);
	if (it != s_rwMap.end())
	{
		it->second->fileLock.releaseReader();
	}
	s_rwMapLock.releaseReader();
	return iRet;
}


long long fileHander::curFile()
{
#if USE_CACHE_READ_MODE
	if (m_openMode == open_write || m_openMode == open_writeBinary)
	{
		return m_curFile;
	}
	return m_cacheStartPos + m_readStartPos;

#endif
}
//成功返回0
int fileHander::seek(long long offset, int origin)
{
	int ret = 0;
	std::mutex *pMutex = nullptr;
	//如果是写模式，上锁，seek即完
	if (m_openMode == open_write || m_openMode == open_writeBinary)
	{
		s_rwMapLock.acquireReader();
		auto it = s_rwMap.find(m_fileName);

		do
		{
			if (it==s_rwMap.end())
			{
				LOG_FATAL(configData::getInstance().get_loggerId(), "seek :" << m_fileName << " failed");
				ret = -1;
				break;
			}
			//LOG_WARN(configData::getInstance().get_loggerId(), "enter");
			it->second->fileLock.acquireReader();
			/*std::string mode;
			if (m_openMode==open_write)
			{
				mode = "a";
			}
			else
			{
				mode = "ab";
			}
			m_fp = fopen(m_fileName.c_str(), mode.c_str());
			if (m_fp==nullptr)
			{
				LOG_ERROR(configData::getInstance().get_loggerId(), "open file:" << m_fileName << " failed");
				ret = -1;
				break;
			}
			ret = fileSeek(m_fp, offset, origin);
			if (ret!=0)
			{
				LOG_ERROR(configData::getInstance().get_loggerId(), "seek file:" << m_fileName << " failed");
				ret = -1;
				break;
			}
			m_curFile = fileTell(m_fp);
			fclose(m_fp);
			m_fp = nullptr;*/
			
			m_curFile = offset;
		} while (0);
		if (it!=s_rwMap.end())
		{
			it->second->fileLock.releaseReader();
		}
		s_rwMapLock.releaseReader();
	}
	else
	{
		//LOG_WARN(configData::getInstance().get_loggerId(), "enter");
		s_rwMapLock.acquireReader();
		auto it = s_rwMap.find(m_fileName);
		do
		{
			if (it==s_rwMap.end())
			{
				LOG_FATAL(configData::getInstance().get_loggerId(), "seek :" << m_fileName << " failed");
				ret = -1;
				break;
			}
			it->second->fileLock.acquireReader();
			int	newPos = 0;
			switch (origin)
			{
			case SEEK_CUR:
				newPos = offset + m_cacheStartPos;
				break;
			case SEEK_SET:
				newPos = offset;
				break;
			case SEEK_END:
				newPos = m_fileSize - offset;
				break;
			default:
				ret = -1;
				break;
			}
			if (ret != 0)
			{
				ret = -1;
				break;
			}
			//当前的缓存范围
			int curRange = m_cacheStartPos + m_readCacheSize;
			if (newPos<m_cacheStartPos || newPos>curRange)
			{
				//重新seek
				if (0 != seekToPos(newPos))
				{
					LOG_ERROR(configData::getInstance().get_loggerId(), "seek to pos failed");
					ret = -1;
					break;
				}
			}
			else
			{
				//更新readStartPos即可
				m_readStartPos = newPos - m_cacheStartPos;
			}
		} while (0);
		if (it!=s_rwMap.end())
		{
			it->second->fileLock.releaseReader();
		}
		//LOG_WARN(configData::getInstance().get_loggerId(), "leave");
		s_rwMapLock.releaseReader();
	}

	return ret;
}

long long fileHander::fileSize()
{
	long long size = 0;
	s_rwMapLock.acquireReader();
	auto it = s_rwMap.find(m_fileName);
	do
	{
		if (it == s_rwMap.end())
		{
			LOG_FATAL(configData::getInstance().get_loggerId(), "it it impossible");
			size = -1;
			break;
		}
		it->second->fileLock.acquireReader();
		FILE *fp = fopen(m_fileName.c_str(), "rb");
		if (fp==nullptr)
		{
			size = -1;
			break;
		}
		fileSeek(fp, 0, SEEK_END);

		size = fileTell(fp);
		if (fp)
		{
			fclose(fp);
			fp = nullptr;
		}
	} while (0);
	if (it != s_rwMap.end())
	{
		it->second->fileLock.releaseReader();
	}
	LOG_WARN(configData::getInstance().get_loggerId(), "leave");
	s_rwMapLock.releaseReader();
	return size;
}

fileHander::~fileHander()
{
	if (m_cacheData)
	{
		delete[]m_cacheData;
		m_cacheData = nullptr;
	}
	//LOG_WARN(configData::getInstance().get_loggerId(), "enter");
	if (m_fp)
	{
		fclose(m_fp);
		m_fp = nullptr;
	}
	s_rwMapLock.acquireWriter();
	auto it = s_rwMap.find(m_fileName);
	if (it!=s_rwMap.end())
	{
		it->second->m_ref--;
		if (it->second->m_ref==0)
		{
			delete it->second;
			s_rwMap.erase(it);
		}
	}
	//LOG_WARN(configData::getInstance().get_loggerId(), "leave");
	s_rwMapLock.releaseWriter();
}

std::string fileHander::getOpenMode(fileOpenMode openMode)
{
	std::string strMode;
	switch (openMode)
	{
	case open_read:
		strMode = "r";
		break;
	case open_readBinary:
		strMode = "rb";
		break;
	case open_write:
		strMode = "r+";
		break;
	case open_writeBinary:
		strMode = "rb+";
		break;
	default:
		break;
	}
	return strMode;
}

int fileHander::seekToPos(long pos)
{
	//么有锁，read和seek有锁 
	m_readStartPos = 0;
	m_cacheStartPos = 0;
	int ret = 0;
	do
	{
		std::string mode;
		/*if (m_openMode==open_read)
		{
			mode = "r";
		}
		else if(m_openMode==open_readBinary)
		{
			mode = "rb";
		}
		m_fp = fopen(m_fileName.c_str(), mode.c_str());*/
		if (nullptr==m_fp)
		{
			ret = -1;
			break;
		}
		FILE *fp = m_fp;
		//更新文件大小
		if (0 != fileSeek(fp, 0, SEEK_END))
		{
			ret = -1;
			LOG_ERROR(configData::getInstance().get_loggerId(), "seek to pos failed");
			break;
		}
		m_fileSize = fileTell(fp);
		if (pos>m_fileSize)
		{
			ret = -1;
			LOG_ERROR(configData::getInstance().get_loggerId(), "seek to pos failed out range");
			break;
		}
		if (0 != fileSeek(fp, pos, SEEK_SET))
		{
			ret = -1;
			LOG_ERROR(configData::getInstance().get_loggerId(), "seek to pos failed");
			break;
		}
		m_curFile = fileTell(fp);
		if (m_cacheData)
		{
			delete[]m_cacheData;
			m_cacheData = nullptr;
		}
		//文件剩余大小和缓存大小做对比
		int leakFileSize = m_fileSize - m_curFile;
		if (leakFileSize <= 0)
		{
			ret = -1;
			LOG_ERROR(configData::getInstance().get_loggerId(), "no more data to seek");
			m_readCacheSize = 0;
			break;
		}
		if (m_readCacheSize<configData::getInstance().readFileCacheSize())
		{
			m_readCacheSize = configData::getInstance().readFileCacheSize();
		}
		m_readCacheSize = m_readCacheSize < leakFileSize ? m_readCacheSize : leakFileSize;
		m_cacheData = new char[m_readCacheSize];
		m_cacheStartPos = m_curFile;
		if (1 != fread(m_cacheData, m_readCacheSize, 1, fp))
		{
			ret = -1;
			LOG_ERROR(configData::getInstance().get_loggerId(), "fread failed");
			break;
		}
	} while (0);
	/*if (m_fp)
	{
		fclose(m_fp);
		m_fp = nullptr;
	}*/
	return ret;
}

fileMutex::fileMutex():m_ref(0)
{
}
