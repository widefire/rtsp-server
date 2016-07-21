#include "util.h"

extern "C"
{
#include <sys/types.h>
#include <time.h>
};

#ifdef WIN32

int	gettimeofday(struct timeval *tp, void *tzp)
{
	time_t clock;
	struct tm tm;
	SYSTEMTIME wtm;
	GetLocalTime(&wtm);
	tm.tm_year     = wtm.wYear - 1900;
	tm.tm_mon     = wtm.wMonth - 1;
	tm.tm_mday     = wtm.wDay;
	tm.tm_hour     = wtm.wHour;
	tm.tm_min     = wtm.wMinute;
	tm.tm_sec     = wtm.wSecond;
	tm. tm_isdst    = -1;
	clock = mktime(&tm);
	tp->tv_sec = clock;
	tp->tv_usec = wtm.wMilliseconds * 1000;
	return (0);
}
#else
#include <unistd.h>
#endif

unsigned int generate_random32()
{
	static timeval time_last;
	static bool first_call=true;
	timeval time_now;
	gettimeofday(&time_now,0);
	if (first_call)
	{
		gettimeofday(&time_last,0);
		first_call=false;
	}else if (time_now.tv_sec==time_last.tv_sec&&
		time_now.tv_usec==time_last.tv_usec)
	{
		os_sleep(1);
	}
	gettimeofday(&time_now,0);

	time_last=time_now;
	unsigned int high=time_now.tv_sec%1000000;
	high*=1000;
	unsigned int random=((static_cast<unsigned int>((time_now.tv_sec%1000000)*1000))+(static_cast<unsigned int>(time_now.tv_usec)/1000));
	return random;

}

void os_sleep(int ms)
{
#ifdef WIN32
	Sleep(ms);
#else
	usleep(ms*1000);
#endif
}


void generateGUID64(std::string &strGUID)
{
	GUID guid;
	char chGUID[64];
#ifdef WIN32
	CoCreateGuid(&guid);

		sprintf(chGUID, "%08X%04X%04X%02X%02X%02X%02X%02X%02X%02X%02X",
			guid.Data1, guid.Data2, guid.Data3,
			guid.Data4[0], guid.Data4[1],
			guid.Data4[2], guid.Data4[3],
			guid.Data4[4], guid.Data4[5],
			guid.Data4[6], guid.Data4[7]);
		
#else
	uuid_generate(reinterpret_cast<unsigned char *>(&guid));
	uuid_unparse(guid,chGUID);
#endif


	strGUID = chGUID;
}



int createDirectory(const char * dirName)
{
	int iRet = 0;
	if (0 != checkDirectoryExist(dirName))
	{
#ifdef WIN32
		iRet = mkdir(dirName);
#else
		//iRet = mkdir(dirName, S_IRWXU);
		//iRet = mkdir(dirName, S_IRWXO);
		iRet = mkdir(dirName, 0777);
#endif // WIN32
	}
	return iRet;
}

int checkDirectoryExist(const char * dirName)
{
	int iRet = 0;
#ifdef WIN32
	WIN32_FIND_DATAA   wfd;
	BOOL rValue = FALSE;
	HANDLE hFind = FindFirstFileA(dirName, &wfd);
	if ((hFind != INVALID_HANDLE_VALUE) && (wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
	{
		rValue = TRUE;
	}
	FindClose(hFind);
	if (rValue == TRUE)
	{
		iRet = 0;
	}
	else
	{
		iRet = -1;
	}
#else
	auto dirPtr = opendir(dirName);
	if (dirPtr == nullptr)
	{
		iRet = -1;
	}
	else
	{
		closedir(dirPtr);
	}
#endif // WIN32
	return iRet;
}

std::string getYmd()
{
	time_t tick;
	struct tm tm;
	char s[100];

	tick = time(NULL);
	tm = *localtime(&tick);
	//strftime(s, sizeof(s), "%Y-%m-%d %H:%M:%S", &tm);
	strftime(s, sizeof(s), "%Y-%m-%d", &tm);

	return s;
}

std::string getYmd2()
{
	time_t tick;
	struct tm tm;
	char s[100];

	tick = time(NULL);
	tm = *localtime(&tick);
	//strftime(s, sizeof(s), "%Y-%m-%d %H:%M:%S", &tm);
	strftime(s, sizeof(s), "%Y/%m/%d", &tm);

	return s;
}

long long fileSeek(FILE * fp, long long offset, int origin)
{
	long long ret = 0;
#ifdef WIN32
	ret = _fseeki64(fp, offset, origin);
#else
	ret = fseeko(fp, offset, origin);
#endif // WIN32
	return ret;
}

long long fileTell(FILE * fp)
{
	long long ret = 0;
#ifdef WIN32
	ret = _ftelli64(fp);
#else
	ret = ftello(fp);
#endif // WIN32
	return ret;
}
