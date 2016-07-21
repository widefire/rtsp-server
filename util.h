#ifndef HX_UTIL_H_
#define HX_UTIL_H_
#include <stdio.h>
#include <time.h>
#include <string>
#include <thread>
#include <map>
#include <list>
#include<mutex>
#include<condition_variable>

#ifdef WIN32
#include <winsock2.h>
#include <Windows.h>
#include <objbase.h>
#include <direct.h>
int	gettimeofday(struct timeval *tp, void *tzp);
#else
#include <uuid/uuid.h>

#include<sys/stat.h>
#include<sys/types.h>
#include <dirent.h>
#define GUID uuid_t
#include<sys/sem.h>
#include<semaphore.h>
#include<pthread.h>

union semun
{
	int val;
	struct semid_ds *buf;
	ushort* seminfo;
	void* __pad;
};
#endif

#include <io.h>
//#include "log4z.h"

unsigned int generate_random32();

void os_sleep(int ms);

void generateGUID64(std::string &strGUID);

//如果文件夹不存在则创建文件夹，成功返回0
int createDirectory(const char *dirName);
//检测文件夹是否存在，存在返回0
int checkDirectoryExist(const char *dirName);

std::string getYmd();
std::string getYmd2();



class ReadWriteLock
{
public:
	ReadWriteLock(void);
	virtual ~ReadWriteLock(void);

public:

	void acquireReader();

	void releaseReader();

	void acquireWriter();

	void releaseWriter();
private:
	int numReaders_;
	std::mutex mtx_numReaders_;
	std::mutex mtx_writer_;
	std::condition_variable_any cv_numReaders_;
};


long long fileSeek(FILE* fp, long long offset, int origin);
long long fileTell(FILE* fp);

#ifndef _WS2_32_WINSOCK_SWAP_LONGLONG

#define _WS2_32_WINSOCK_SWAP_LONGLONG(l)            \
            ( ( ((l) >> 56) & 0x00000000000000FFLL ) |       \
              ( ((l) >> 40) & 0x000000000000FF00LL ) |       \
              ( ((l) >> 24) & 0x0000000000FF0000LL ) |       \
              ( ((l) >>  8) & 0x00000000FF000000LL ) |       \
              ( ((l) <<  8) & 0x000000FF00000000LL ) |       \
              ( ((l) << 24) & 0x0000FF0000000000LL ) |       \
              ( ((l) << 40) & 0x00FF000000000000LL ) |       \
              ( ((l) << 56) & 0xFF00000000000000LL ) )


#ifndef htonll
__inline unsigned long long htonll(unsigned long long Value)
{
	const unsigned long long Retval = _WS2_32_WINSOCK_SWAP_LONGLONG(Value);
	return Retval;
}
#endif /* htonll */
#endif // _WS2_32_WINSOCK_SWAP_LONGLONG
#endif

