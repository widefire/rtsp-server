#ifndef _TASK_SCHEDULE_OBJ_H_
#define _TASK_SCHEDULE_OBJ_H_

enum task_schedule_type
{
	//直接任务
	schedule_start_rtsp_file,
	schedule_start_rtsp_living,
	schedule_stop_rtsp_by_socket,
	schedule_stop_rtsp_by_name,
	schedule_pause_rtsp_file,
	schedule_stop_rtsp_file_sinker,
	schedule_rtp_data,
	schedule_rtcp_data,
	schedule_stop_json_task,
	schedule_rtsp_living_exist,
	schedule_rtsp_living_sdp,
	schedule_rtsp_living_mediaType,
	schedule_send_rtsp_data,
	schedule_manager_sinker,
	schedele_json_send_notify,
	schedule_json_closed,
	//需异步任务
	schedule_start_json_task_async,
};
class taskScheduleObj
{
public:
	taskScheduleObj();
	virtual ~taskScheduleObj();
	virtual task_schedule_type getType()=0;
protected:
private:
	taskScheduleObj(const taskScheduleObj&)=delete;
	taskScheduleObj&operator =(const taskScheduleObj&) = delete;
};

#endif
