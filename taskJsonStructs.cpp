#include "taskJsonStructs.h"
#include "configData.h"



bool taskJson::initTaskJson()
{
	bool result = true;

	std::string strOperationTypes[] =
	{
		"createTask",
		"createTaskReturn",
		"deleteTask",
		"deleteTaskReturn",
		"disconnect",
		"heartbeat",
		"saveFile",
		"liveEnd",
		"liveStart",
		"getLivingList"
	};

	std::string strTaskValueTypes[]=
	{
		"taskType",
		"targetProtocol",
		"targetAddress",
		"saveFLV",
		"saveMP4",
		"liveRTSP",
		"liveRTMP",
		"liveHTTP",
		"taskId",
		"saveType",
		"userDefineString",
		"status",
		"heartbeat",
		"timeBegin",
		"timeEnd",
		"relativelyDir",
		"fileUrl",
		"fileNameFlv",
		"fileNameMp4",
		"livingList",
	};
	do
	{
		int i = 0;
		for (std::string opt:strOperationTypes)
		{
			m_mapOperationTypes.insert(std::pair<em_operationType, std::string>((em_operationType)i++, opt));
		}
		i = 0;
		for (std::string valueTypes:strTaskValueTypes)
		{
			m_maptaskKeyTypes.insert(std::pair<em_taskKeyType, std::string>((em_taskKeyType)i++, valueTypes));
		}
	} while (0);
	return true;
}

taskJson::taskJson()
{
	if (!initTaskJson())
	{
		printf("init task json failed\n");
	}
}


taskJson & taskJson::getInstance()
{
	// TODO: 在此处插入 return 语句
	static taskJson s_task;
	return s_task;
}

taskJson::~taskJson()
{
}

bool taskJson::string2Json(const std::string& jsonString, taskJsonStruct & jsonStruct)
{
	bool result = true;
	std::string err;
	std::string tmpString;
	do
	{
		json11::Json jsonGenerated = json11::Json::parse(jsonString, err);
		if (err.size()!=0)
		{
			result = false;
			LOG_WARN(configData::getInstance().get_loggerId(), err.c_str());
			break;
		}
		//类型
		auto iterator = m_maptaskKeyTypes.find(em_taskKeyType::value_type_taskType);

		result = jsonGenerated[iterator->second].is_string();
		if (result==false)
		{
			LOG_WARN(configData::getInstance().get_loggerId(), "invalid jsonString:"<<jsonString.c_str());
			break;
		}
		tmpString = jsonGenerated[iterator->second].string_value();
		if (tmpString.size()<=0)
		{
			LOG_WARN(configData::getInstance().get_loggerId(), "invalid jsonString:"<< jsonString.c_str());
			result = false;
			break;
		}

		em_operationType optType;
		result = false;
		for (auto itType:m_mapOperationTypes )
		{
			if (itType.second==tmpString)
			{
				optType = itType.first;
				result = true;
				break;
			}
		}
		if (false==result)
		{
			LOG_WARN(configData::getInstance().get_loggerId(), "invalid jsonString:"<< jsonString.c_str());
			result = false;
			break;
		}
		jsonStruct.taskType = tmpString;
		switch (optType)
		{
		case em_operationType::opt_type_createTask:
			result = fillCreateTaskJsonStruct(jsonGenerated, jsonStruct);
			break;
		case em_operationType::opt_type_createTaskReturn:
			result = fillCreateTaskReturnJsonStruct(jsonGenerated, jsonStruct);
			break;
		case em_operationType::opt_type_deleteTask:
			result = fillDeleteTaskJsonStruct(jsonGenerated, jsonStruct);
			break;
		case em_operationType::opt_type_deleteTaskReturn:
			result = fillDeleteTaskReturnJsonStruct(jsonGenerated, jsonStruct);
			break;
		case em_operationType::opt_type_disconnect:
			result = fillDisconnectJsonStruct(jsonGenerated, jsonStruct);
			break;
		case em_operationType::opt_type_heartbeat:
			result = fillHeartbeatJsonStruct(jsonGenerated, jsonStruct);
			break;
		case em_operationType::opt_type_saveFile:
			result = fillSaveFileJsonStruct(jsonGenerated, jsonStruct);
			break;
		case em_operationType::opt_type_livingEnd:
			result = fillLiveEndJsonStruct(jsonGenerated, jsonStruct);
			break;
		case em_operationType::opt_type_livingStart:
			result = fillLiveStartJsonStruct(jsonGenerated, jsonStruct);
			break;
		case em_operationType::opt_type_getLivingList:
			result = fillGetLivingListJsonStruct(jsonGenerated, jsonStruct);
			break;
		default:
			result = false;
			break;
		}
		if (false == result)
		{
			LOG_WARN(configData::getInstance().get_loggerId(), "invalid jsonString:"<< jsonString.c_str());
			break;
		}
	} while (0);
	return result;
}

bool taskJson::json2String(const taskJsonStruct&  jsonStruct, std::string &jsonString)
{
	bool result = true;
	do
	{
		std::map<em_taskKeyType, std::string>::iterator iterator;
		json11::Json jsonGenerated = json11::Json::object
		{
			{ m_maptaskKeyTypes.find(em_taskKeyType::value_type_taskType)->second,jsonStruct.taskType },
			{ m_maptaskKeyTypes.find(em_taskKeyType::value_type_targetProtocol)->second,jsonStruct.targetProtocol },
			{ m_maptaskKeyTypes.find(em_taskKeyType::value_type_targetAddress)->second,jsonStruct.targetAddress },
			{ m_maptaskKeyTypes.find(em_taskKeyType::value_type_saveFLV)->second,jsonStruct.saveFlv },
			{ m_maptaskKeyTypes.find(em_taskKeyType::value_type_saveMP4)->second,jsonStruct.saveMP4 },
			{ m_maptaskKeyTypes.find(em_taskKeyType::value_type_liveRTSP)->second,jsonStruct.liveRTSP },
			{ m_maptaskKeyTypes.find(em_taskKeyType::value_type_liveRTMP)->second,jsonStruct.liveRTMP },
			{ m_maptaskKeyTypes.find(em_taskKeyType::value_type_liveHTTP)->second,jsonStruct.liveHTTP },
			{ m_maptaskKeyTypes.find(em_taskKeyType::value_type_taskId)->second,jsonStruct.taskId },
			{ m_maptaskKeyTypes.find(em_taskKeyType::value_type_saveType)->second,jsonStruct.saveType },
			{ m_maptaskKeyTypes.find(em_taskKeyType::value_type_userdefString)->second,jsonStruct.userdefString },
			{ m_maptaskKeyTypes.find(em_taskKeyType::value_type_status)->second,jsonStruct.status },
			{ m_maptaskKeyTypes.find(em_taskKeyType::value_type_heartbeat)->second,jsonStruct.heartbeat },
			{ m_maptaskKeyTypes.find(em_taskKeyType::value_type_timeBegin)->second,jsonStruct.timeBegin },
			{ m_maptaskKeyTypes.find(em_taskKeyType::value_type_timeEnd)->second,jsonStruct.timeEnd },
			{ m_maptaskKeyTypes.find(em_taskKeyType::value_type_relativelyDir)->second,jsonStruct.relativelyDir },
			{ m_maptaskKeyTypes.find(em_taskKeyType::value_type_fileUrl)->second,jsonStruct.fileUrl },
			{ m_maptaskKeyTypes.find(em_taskKeyType::value_type_livingList)->second,json11::Json( jsonStruct.livingList )},
			{ m_maptaskKeyTypes.find(em_taskKeyType::value_type_fileNameFLV)->second,json11::Json(jsonStruct.fileNameFlv) },
			{ m_maptaskKeyTypes.find(em_taskKeyType::value_type_fileNameMP4)->second,json11::Json(jsonStruct.fileNameMp4) },
		};
		jsonString = jsonGenerated.dump();
	} while (0);
	return result;
}

std::string taskJson::getOperatorType(em_operationType op)
{
	return m_mapOperationTypes.find(op)->second;
}

std::string taskJson::getTaskKey(em_taskKeyType taskKeyType)
{
	return m_maptaskKeyTypes.find(taskKeyType)->second;
}

bool taskJson::fillCreateTaskJsonStruct(json11::Json jsonObj, taskJsonStruct & jsonStruct)
{
	bool result = true;
	std::string tmpString;
	do
	{
		//协议类型
		auto iterator = m_maptaskKeyTypes.find(em_taskKeyType::value_type_targetProtocol);
		tmpString = jsonObj[iterator->second].string_value();
		if (tmpString!=taskJsonProtocol_HTTP&&
			tmpString!=taskJsonProtocol_RTSP&&
			tmpString!=taskJsonProtocol_RTMP)
		{
			LOG_WARN(configData::getInstance().get_loggerId(), "invalid target protocol:"<<tmpString.c_str());
			result = false;
			break;
		}
		jsonStruct.targetProtocol = tmpString;
		//协议地址
		iterator = m_maptaskKeyTypes.find(em_taskKeyType::value_type_targetAddress);
		tmpString = jsonObj[iterator->second].string_value();
		if (tmpString.size()==0)
		{
			LOG_WARN(configData::getInstance().get_loggerId(), "no target address");
			result = false;
			break;
		}
		jsonStruct.targetAddress = tmpString;
		//flv
		iterator = m_maptaskKeyTypes.find(em_taskKeyType::value_type_saveFLV);
		jsonStruct.saveFlv = jsonObj[iterator->second].bool_value();
		//mp4
		iterator = m_maptaskKeyTypes.find(em_taskKeyType::value_type_saveMP4);
		jsonStruct.saveMP4 = jsonObj[iterator->second].bool_value();
		//rtsp
		iterator = m_maptaskKeyTypes.find(em_taskKeyType::value_type_liveRTSP);
		jsonStruct.liveRTSP = jsonObj[iterator->second].bool_value();
		//rtmp
		iterator = m_maptaskKeyTypes.find(em_taskKeyType::value_type_liveRTMP);
		jsonStruct.liveRTMP = jsonObj[iterator->second].bool_value();
		//http
		iterator = m_maptaskKeyTypes.find(em_taskKeyType::value_type_liveHTTP);
		jsonStruct.liveHTTP = jsonObj[iterator->second].bool_value();
		//taskid
		iterator = m_maptaskKeyTypes.find(em_taskKeyType::value_type_taskId);
		jsonStruct.taskId = jsonObj[iterator->second].string_value();
		//saveType
		iterator = m_maptaskKeyTypes.find(em_taskKeyType::value_type_saveType);
		tmpString = jsonObj[iterator->second].string_value();
		if (tmpString!=taskJsonSaveType_Append&&tmpString!=taskJsonSaveType_CreateNew)
		{
			LOG_WARN(configData::getInstance().get_loggerId(), "invalid save type" << tmpString);
			/*result = false;
			break;*/
		}
		jsonStruct.saveType = tmpString;
		//userDefString
		iterator = m_maptaskKeyTypes.find(em_taskKeyType::value_type_userdefString);
		jsonStruct.userdefString = jsonObj[iterator->second].string_value();
		//flvName
		iterator = m_maptaskKeyTypes.find(em_taskKeyType::value_type_fileNameFLV);
		jsonStruct.fileNameFlv = jsonObj[iterator->second].string_value();
		//Mp4Name
		iterator = m_maptaskKeyTypes.find(em_taskKeyType::value_type_fileNameFLV);
		jsonStruct.fileNameMp4 = jsonObj[iterator->second].string_value();
	} while (0);
	return result;
}

bool taskJson::fillCreateTaskReturnJsonStruct(json11::Json jsonObj, taskJsonStruct & jsonStruct)
{
	bool result = true;
	std::string tmpString;
	do
	{
		//status
		auto iterator = m_maptaskKeyTypes.find(em_taskKeyType::value_type_status);
		tmpString = jsonObj[iterator->second].string_value();
		if (tmpString!= taskJsonStatus_Success&&
			tmpString!= taskJsonStatus_Failed&&
			tmpString!= taskJsonStatus_Existing)
		{
			LOG_WARN(configData::getInstance().get_loggerId(), "invalid status:" << tmpString);
			result = false;
			break;
		}
		jsonStruct.status = tmpString;
		//taskId
		iterator = m_maptaskKeyTypes.find(em_taskKeyType::value_type_taskId);
		jsonStruct.userdefString = jsonObj[iterator->second].string_value();
		////address
		//iterator = m_maptaskValueTypes.find(em_taskValueType::value_type_targetAddress);
		//jsonStruct.userdefString = jsonObj[iterator->second].string_value();
		//userDefString
		iterator = m_maptaskKeyTypes.find(em_taskKeyType::value_type_userdefString);
		jsonStruct.userdefString = jsonObj[iterator->second].string_value();
	} while (0);
	return result;
}

bool taskJson::fillDeleteTaskJsonStruct(json11::Json jsonObj, taskJsonStruct & jsonStruct)
{
	bool result = true;
	std::string tmpString;
	do
	{
		//taskId
		auto iterator = m_maptaskKeyTypes.find(em_taskKeyType::value_type_taskId);
		jsonStruct.taskId = jsonObj[iterator->second].string_value();
		//userDefString
		iterator = m_maptaskKeyTypes.find(em_taskKeyType::value_type_userdefString);
		jsonStruct.userdefString = jsonObj[iterator->second].string_value();
	} while (0);
	return result;
}

bool taskJson::fillDeleteTaskReturnJsonStruct(json11::Json jsonObj, taskJsonStruct & jsonStruct)
{
	bool result = true;
	std::string tmpString;
	do
	{
		//status
		auto iterator = m_maptaskKeyTypes.find(em_taskKeyType::value_type_status);
		tmpString = jsonObj[iterator->second].string_value();
		if (tmpString != taskJsonStatus_Success&&
			tmpString != taskJsonStatus_Failed&&
			tmpString != taskJsonStatus_Existing)
		{
			LOG_WARN(configData::getInstance().get_loggerId(), "invalid status:" << tmpString);
			result = false;
			break;
		}
		jsonStruct.status = tmpString;
		//taskId
		iterator = m_maptaskKeyTypes.find(em_taskKeyType::value_type_taskId);
		jsonStruct.userdefString = jsonObj[iterator->second].string_value();
		//userDefString
		iterator = m_maptaskKeyTypes.find(em_taskKeyType::value_type_userdefString);
		jsonStruct.userdefString = jsonObj[iterator->second].string_value();
	} while (0);
	return result;
}

bool taskJson::fillDisconnectJsonStruct(json11::Json jsonObj, taskJsonStruct & jsonStruct)
{
	bool result = true;
	std::string tmpString;
	do
	{
		//status
		auto iterator = m_maptaskKeyTypes.find(em_taskKeyType::value_type_status);
		tmpString = jsonObj[iterator->second].string_value();
		if (tmpString != taskJsonStatus_NoHeartbeat)
		{
			LOG_WARN(configData::getInstance().get_loggerId(), "invalid status:" << tmpString);
			result = false;
			break;
		}
		jsonStruct.status = tmpString;
	} while (0);
	return result;
}

bool taskJson::fillHeartbeatJsonStruct(json11::Json jsonObj, taskJsonStruct & jsonStruct)
{
	bool result = true;
	do
	{
		auto iterator = m_maptaskKeyTypes.find(em_taskKeyType::value_type_heartbeat);
		jsonStruct.heartbeat = jsonObj[iterator->second].int_value();
	} while (0);
	return result;
}

bool taskJson::fillSaveFileJsonStruct(json11::Json jsonObj, taskJsonStruct & jsonStruct)
{
	bool result = true;
	std::string tmpString;
	do
	{
		//task id
		auto iterator = m_maptaskKeyTypes.find(em_taskKeyType::value_type_taskId);
		jsonStruct.taskId = jsonObj[iterator->second].string_value();
		//realtiveDir
		iterator = m_maptaskKeyTypes.find(em_taskKeyType::value_type_relativelyDir);
		jsonStruct.relativelyDir = jsonObj[iterator->second].string_value();
		//fileUrl
		iterator = m_maptaskKeyTypes.find(em_taskKeyType::value_type_fileUrl);
		jsonStruct.fileUrl = jsonObj[iterator->second].string_value();
		//timeBegin
		iterator = m_maptaskKeyTypes.find(em_taskKeyType::value_type_timeBegin);
		jsonStruct.timeBegin = jsonObj[iterator->second].int_value();
		//timeEnd
		iterator = m_maptaskKeyTypes.find(em_taskKeyType::value_type_timeEnd);
		jsonStruct.timeEnd = jsonObj[iterator->second].int_value();
		//userDefineString
		iterator = m_maptaskKeyTypes.find(em_taskKeyType::value_type_userdefString);
		jsonStruct.userdefString = jsonObj[iterator->second].string_value();
		//flvName
		iterator = m_maptaskKeyTypes.find(em_taskKeyType::value_type_fileNameFLV);
		jsonStruct.fileNameFlv = jsonObj[iterator->second].string_value();
		//Mp4Name
		iterator = m_maptaskKeyTypes.find(em_taskKeyType::value_type_fileNameFLV);
		jsonStruct.fileNameMp4 = jsonObj[iterator->second].string_value();
	} while (0);
	return result;
}

bool taskJson::fillLiveEndJsonStruct(json11::Json jsonObj, taskJsonStruct & jsonStruct)
{
	bool result = true;
	do
	{
		//task id
		auto iterator = m_maptaskKeyTypes.find(em_taskKeyType::value_type_taskId);
		jsonStruct.taskId = jsonObj[iterator->second].string_value();
		//targetProtocol
		iterator = m_maptaskKeyTypes.find(em_taskKeyType::value_type_targetProtocol);
		jsonStruct.targetAddress = jsonObj[iterator->second].string_value();
		//timeBegin
		iterator = m_maptaskKeyTypes.find(em_taskKeyType::value_type_timeBegin);
		jsonStruct.timeBegin = jsonObj[iterator->second].int_value();
		//timeEnd
		iterator = m_maptaskKeyTypes.find(em_taskKeyType::value_type_timeEnd);
		jsonStruct.timeEnd = jsonObj[iterator->second].int_value();
		//userDefineString
		iterator = m_maptaskKeyTypes.find(em_taskKeyType::value_type_userdefString);
		jsonStruct.userdefString = jsonObj[iterator->second].string_value();
	} while (0);
	return result;
}

bool taskJson::fillLiveStartJsonStruct(json11::Json jsonObj, taskJsonStruct & jsonStruct)
{
	bool result = true;
	std::string tmpString;
	do
	{
		//targetProtocol
		auto iterator = m_maptaskKeyTypes.find(em_taskKeyType::value_type_targetProtocol);
		tmpString = jsonObj[iterator->second].string_value();
		if (tmpString != taskJsonProtocol_HTTP&&
			tmpString != taskJsonProtocol_RTSP&&
			tmpString != taskJsonProtocol_RTMP)
		{
			LOG_WARN(configData::getInstance().get_loggerId(), "invalid target protocol:" << tmpString.c_str());
			result = false;
			break;
		}
		jsonStruct.targetProtocol = tmpString;
		//targetProtocol
		iterator = m_maptaskKeyTypes.find(em_taskKeyType::value_type_targetProtocol);
		jsonStruct.targetAddress = jsonObj[iterator->second].string_value();
		//targetAddress
		iterator = m_maptaskKeyTypes.find(em_taskKeyType::value_type_targetAddress);
		jsonStruct.targetAddress = jsonObj[iterator->second].string_value();
		//timeBegin
		iterator = m_maptaskKeyTypes.find(em_taskKeyType::value_type_timeBegin);
		jsonStruct.timeBegin = jsonObj[iterator->second].int_value();
		//fileUrl
		iterator = m_maptaskKeyTypes.find(em_taskKeyType::value_type_fileUrl);
		jsonStruct.fileUrl = jsonObj[iterator->second].string_value();
		//taskId
		iterator = m_maptaskKeyTypes.find(em_taskKeyType::value_type_taskId);
		jsonStruct.taskId = jsonObj[iterator->second].string_value();
		//userDefineString
		iterator = m_maptaskKeyTypes.find(em_taskKeyType::value_type_userdefString);
		jsonStruct.userdefString = jsonObj[iterator->second].string_value();
	} while (0);
	return result;
}

bool taskJson::fillGetLivingListJsonStruct(json11::Json jsonObj, taskJsonStruct & jsonStruct)
{
	bool result = true;
	do
	{
		auto iterator = m_maptaskKeyTypes.find(em_taskKeyType::value_type_livingList);
		auto arrayItems = jsonObj[iterator->second].array_items();
		for (auto i:arrayItems )
		{
			taskJsonStruct livingJsonStruct;
			result = fillLiveStartJsonStruct(i, livingJsonStruct);
			if (!result)
			{
				break;
			}
			jsonStruct.livingList.push_back(livingJsonStruct);
		}
	} while (0);
	return result;
}

taskJsonStruct::taskJsonStruct():saveFlv(false),saveMP4(false),liveRTSP(false),liveRTMP(false),liveHTTP(false),
heartbeat(0),timeBegin(0),timeEnd(0)
{
}

json11::Json taskJsonStruct::to_json() const
{
	std::map<em_taskKeyType, std::string>::iterator iterator;
	static std::string strTaskValueTypes[] =
	{
		"taskType",
		"targetProtocol",
		"targetAddress",
		"saveFLV",
		"saveMP4",
		"liveRTSP",
		"liveRTMP",
		"liveHTTP",
		"taskId",
		"saveType",
		"userDefineString",
		"status",
		"heartbeat",
		"timeBegin",
		"timeEnd",
		"relativelyDir",
		"fileUrl",
		"fileNameFlv",
		"fileNameMp4",
		"livingList",
	};
	int index = 0;
	return json11::Json::object
	{
		{ strTaskValueTypes[index++],taskType},
		{ strTaskValueTypes[index++], targetProtocol},
		{ strTaskValueTypes[index++], targetAddress},
		{ strTaskValueTypes[index++], saveFlv},
		{ strTaskValueTypes[index++], saveMP4},
		{ strTaskValueTypes[index++], liveRTSP },
		{ strTaskValueTypes[index++], liveRTMP },
		{ strTaskValueTypes[index++], liveHTTP },
		{ strTaskValueTypes[index++], taskId},
		{ strTaskValueTypes[index++], saveType},
		{ strTaskValueTypes[index++], userdefString},
		{ strTaskValueTypes[index++], status},
		{ strTaskValueTypes[index++], heartbeat},
		{ strTaskValueTypes[index++], timeBegin},
		{ strTaskValueTypes[index++], timeEnd},
		{ strTaskValueTypes[index++], relativelyDir},
		{ strTaskValueTypes[index++], fileUrl},
		{ strTaskValueTypes[index++], fileNameFlv },
		{ strTaskValueTypes[index++], fileNameMp4 }
	};
}

taskJsonStruct::taskJsonStruct(const taskJsonStruct &r)
{
	this->taskType=r.taskType;
	this->targetProtocol=r.targetProtocol;
	this->targetAddress=r.targetAddress;
	this->saveFlv=r.saveFlv;
	this->saveMP4=r.saveMP4;
	this->liveRTSP=r.liveRTSP;
	this->liveRTMP=r.liveRTMP;
	this->liveHTTP=r.liveHTTP;
	this->taskId=r.taskId;
	this->saveType=r.saveType;
	this->userdefString=r.userdefString;
	this->status=r.status;
	this->heartbeat=r.heartbeat;
	this->timeBegin=r.timeBegin;
	this->timeEnd=r.timeEnd;
	this->relativelyDir=r.relativelyDir;
	this->fileUrl=r.relativelyDir;
	this->fileNameFlv = r.fileNameFlv;
	this->fileNameMp4 = r.fileNameMp4;
	for (auto i:r.livingList)
	{
		this->livingList.push_back(i);
	}
}

taskJsonStruct & taskJsonStruct::operator=(const taskJsonStruct &r)
{
	// TODO: 在此处插入 return 语句
	this->taskType = r.taskType;
	this->targetProtocol = r.targetProtocol;
	this->targetAddress = r.targetAddress;
	this->saveFlv = r.saveFlv;
	this->saveMP4 = r.saveMP4;
	this->liveRTSP = r.liveRTSP;
	this->liveRTMP = r.liveRTMP;
	this->liveHTTP = r.liveHTTP;
	this->taskId = r.taskId;
	this->saveType = r.saveType;
	this->userdefString = r.userdefString;
	this->status = r.status;
	this->heartbeat = r.heartbeat;
	this->timeBegin = r.timeBegin;
	this->timeEnd = r.timeEnd;
	this->relativelyDir = r.relativelyDir;
	this->fileUrl = r.relativelyDir;
	this->fileNameFlv = r.fileNameFlv;
	this->fileNameMp4 = r.fileNameMp4;
	for (auto i : r.livingList)
	{
		this->livingList.push_back(i);
	}
	return *this;
}
