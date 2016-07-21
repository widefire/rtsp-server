#pragma once

#include "json11.hpp"

enum class em_operationType :unsigned int
{
	opt_type_createTask,
	opt_type_createTaskReturn,
	opt_type_deleteTask,
	opt_type_deleteTaskReturn,
	opt_type_disconnect,
	opt_type_heartbeat,
	opt_type_saveFile,
	opt_type_livingEnd,
	opt_type_livingStart,
	opt_type_getLivingList,
};

enum class em_taskKeyType :unsigned int
{
	value_type_taskType,
	value_type_targetProtocol,
	value_type_targetAddress,
	value_type_saveFLV,
	value_type_saveMP4,
	value_type_liveRTSP,
	value_type_liveRTMP,
	value_type_liveHTTP,
	value_type_taskId,
	value_type_saveType,
	value_type_userdefString,
	value_type_status,
	value_type_heartbeat,
	value_type_timeBegin,
	value_type_timeEnd,
	value_type_relativelyDir,
	value_type_fileUrl,
	value_type_fileNameFLV,
	value_type_fileNameMP4,
	value_type_livingList,
};

#define taskJsonProtocol_RTSP "RTSP"
#define taskJsonProtocol_RTMP "RTMP"
#define taskJsonProtocol_HTTP "HTTP"
#define taskJsonSaveType_CreateNew  "createNew"
#define taskJsonSaveType_Append  "append"
#define taskJsonStatus_Success "success"
#define taskJsonStatus_Failed "failed"
#define taskJsonStatus_Existing "existing"
#define taskJsonStatus_NoHeartbeat "noHeartbeat"

static const int MAX_JSON_SIZE = 0xffffff;

//struct taskJsonStruct
class taskJsonStruct
{
public:
	taskJsonStruct();
	~taskJsonStruct()
	{

	}
	std::string taskType;
	std::string targetProtocol;
	std::string targetAddress;
	bool		saveFlv;
	bool		saveMP4;
	bool		liveRTSP;
	bool		liveRTMP;
	bool		liveHTTP;
	std::string taskId;
	std::string saveType;
	std::string userdefString;
	std::string status;
	int heartbeat;
	int timeBegin;
	int timeEnd;
	std::string relativelyDir;
	std::string fileUrl;
	std::vector<taskJsonStruct> livingList;
	std::string fileNameFlv;
	std::string fileNameMp4;
	json11::Json to_json() const;
	taskJsonStruct(const taskJsonStruct&);
	taskJsonStruct&operator =(const taskJsonStruct&);
};

//根据字符串生成Json结构体
//根据Json结构体生成字符串
//
class taskJson
{
public:
	static taskJson& getInstance();
	~taskJson();
	bool string2Json(const std::string& jsonString, taskJsonStruct &jsonStruct);
	bool json2String(const taskJsonStruct& jsonStruct, std::string &jsonString);
	std::string getOperatorType(em_operationType op);
	std::string getTaskKey(em_taskKeyType taskKeyType);
private:
	bool initTaskJson();
	taskJson();
	taskJson(const taskJson&) = delete;
	taskJson& operator =(const taskJson&) = delete;

	bool fillCreateTaskJsonStruct(json11::Json jsonObj,taskJsonStruct &jsonStruct);
	bool fillCreateTaskReturnJsonStruct(json11::Json jsonObj, taskJsonStruct &jsonStruct);
	bool fillDeleteTaskJsonStruct(json11::Json jsonObj, taskJsonStruct &jsonStruct);
	bool fillDeleteTaskReturnJsonStruct(json11::Json jsonObj, taskJsonStruct &jsonStruct);
	bool fillDisconnectJsonStruct(json11::Json jsonObj, taskJsonStruct &jsonStruct);
	bool fillHeartbeatJsonStruct(json11::Json jsonObj, taskJsonStruct &jsonStruct);
	bool fillSaveFileJsonStruct(json11::Json jsonObj, taskJsonStruct &jsonStruct);
	bool fillLiveEndJsonStruct(json11::Json jsonObj, taskJsonStruct &jsonStruct);
	bool fillLiveStartJsonStruct(json11::Json jsonObj, taskJsonStruct &jsonStruct);
	bool fillGetLivingListJsonStruct(json11::Json jsonObj, taskJsonStruct &jsonStruct);
	

	std::map<em_operationType, std::string> m_mapOperationTypes;
	std::map<em_taskKeyType, std::string> m_maptaskKeyTypes;
};

