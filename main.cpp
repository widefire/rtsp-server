
#include "RTSP_server.h"
#include "taskSchedule.h"

#include "taskJsonStructs.h"
#include "rtspClientSource.h"
//#include <vld.h>
using namespace zsummer::log4z;
#ifndef WIN32
#include <signal.h>
#include<execinfo.h> 

void SystemErrorHandler(int signum)
{
	const int len = 4096;
	void *func[len];
	size_t size;
	int i;
	char **funs;

	signal(signum, SIG_DFL);
	size = backtrace(func, len);
	funs = (char**)backtrace_symbols(func, size);
	LOG_FATAL(configData::getInstance().get_loggerId(), "System error, Stack trace:\n");
	for (i = 0; i < size; ++i)
	{
		LOG_FATAL(configData::getInstance().get_loggerId(), i << " " << funs[i]);
		
	}
	free(funs);
}
#endif
void signal_callback_handler(int signum) {

	printf("Caught signal SIGPIPE %d\n", signum);
}
int main(int argc,char **argv)
{
	int iRet=0;
	const char *host_name="localhost";
	auto max_thread=std::thread::hardware_concurrency();
	if (argc>1)
	{
		//config path
		configData::setConfigFileName(argv[1]);
		configData::getInstance();
	}
	else
	{
		//do nothing

		//set_LOG4Z_DEFAULT_PATH("./log/");
		configData::getInstance();
	}
#ifdef WIN32
#else
	signal(SIGPIPE, signal_callback_handler);
#endif
#ifndef WIN32
	sigset_t signal_mask;
	sigemptyset(&signal_mask);
	sigaddset(&signal_mask, SIGPIPE);
	int rc = pthread_sigmask(SIG_BLOCK, &signal_mask, NULL);
	if (rc != 0) {
		printf("block sigpipe error/n");
	}
	sigprocmask(SIG_BLOCK, &signal_mask, NULL);
	signal(SIGSEGV, SystemErrorHandler); //Invaild memory address  
	signal(SIGABRT, SystemErrorHandler); // Abort signal
#endif
	LOG_INFO(configData::getInstance().get_loggerId(), "num thread:" << max_thread);
	taskJson::getInstance();
	do
	{
		iRet=socketInit();
		if (iRet!=0)
		{
			break;
		}
		taskSchedule::getInstance();
		iRet=create_RTP_RTCP_server(RTP_Callback,RTCP_Callback);
		if (iRet!=0)
		{
			break;
		}

		std::thread rtspServerThread(create_RTSP_server, const_cast<char*>(host_name), 
			configData::getInstance().RTSP_server_port(), RTSP_Callback);
		std::thread taskJsonThread(createTaskJsonServer, configData::getInstance().taskPort(), taskJsonCallback);
		if (rtspServerThread.joinable())
		{
			rtspServerThread.join();
		}
		if (taskJsonThread.joinable())
		{
			taskJsonThread.join();
		}
		LOG_INFO(configData::getInstance().get_loggerId(),"hss shutdown");
	} while (0);
	socketDestory();
	return iRet;
}
