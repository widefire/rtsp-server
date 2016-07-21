#include "RTSP_server.h"
#include "taskSchedule.h"
#include "tcpCacher.h"
#include "task_json.h"

#include <thread>

tcpCacherManager &taskJsonCacher()
{
	static tcpCacherManager cacherManager;
	return cacherManager;
}

void taskJsonCallback(socket_event_t * data)
{
	//套接字关闭，
	if (data->sock_closed)
	{
		taskJsonCloseClient(data->clientId);
		return;
	}
	taskJsonCacher().appendBuffer(data->clientId, (char*)data->dataPtr, data->dataLen);
	//转发给任务队列
	std::list<DataPacket> dataList;
	taskJsonCacher().getValidBuffer(data->clientId, dataList, parseTaskJsonData);
	for (auto it:dataList)
	{
		std::string strJson=it.data + 4;
		//因为传递过来的数据没有结束符，所以手动结束
		strJson.resize(it.size - 4);
		taskScheduleObj *taskObj = new task_json(strJson, data->clientId);
		
		taskSchedule::getInstance().addTaskObj(taskObj);
	}
}

void taskJsonCloseClient(socket_t sockId)
{
	taskJsonCacher().removeCacher(sockId);
	taskJsonClosed *taskClosed = new taskJsonClosed(sockId);
	//taskSchedule::getInstance().addTaskObj(taskClosed);
	taskSchedule::getInstance().executeTaskObj(taskClosed);
	delete taskClosed;
}