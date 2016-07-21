#include "flvParser.h"
#include "configData.h"
#include "socket_type.h"


flvParser::flvParser(std::string fileName) :m_fileHander(nullptr), m_duration(0)
{
	m_fileHander = new fileHander(fileName, open_readBinary);
	m_inited = m_fileHander->openSuccessed();
	auto cur = m_fileHander->curFile();
	parseFlvHeader();
}


flvParser::~flvParser()
{
	if (m_fileHander)
	{
		delete m_fileHander;
		m_fileHander = nullptr;
	}
}

timedPacket flvParser::getNextFrame()
{
	timedPacket pktOut;
	if (m_fileHander->curFile()==0)
	{
		parseFlvHeader();
	}
	do
	{
		//pktOut.data = nullptr;
		int ret = 0;
		unsigned int previousTagSize;
		ret = m_fileHander->readFile(4, (char*)&previousTagSize);
		if (ret != 4)
		{
			LOG_ERROR(configData::getInstance().get_loggerId(), "read file failed");
			break;
		}
		previousTagSize = ntohl(previousTagSize);
		///////////////////////////////////////////////
		//读取tag
		flvTag	tag;
		bool result = readTag(tag);
		if (!result)
		{
			LOG_ERROR(configData::getInstance().get_loggerId(), "read file failed");
			break;
		}
		pktOut.timestamp = tag.timestampFull;
		long long curFilePos = m_fileHander->curFile();
		switch (tag.tagType)
		{
		case FLV_Tag_Audio:
			if (!parserAudioData(tag, pktOut))
			{
				LOG_ERROR(configData::getInstance().get_loggerId(), "invalid flv audio tag");
			}
			break;
		case FLV_Tag_Video:
			if (!parserVideoData(tag, pktOut))
			{
				LOG_ERROR(configData::getInstance().get_loggerId(), "invalid flv video tag");
			}
			break;
		case FLV_Tag_ScriptData:
			if (parseScriptData(tag))
			{
				//读取下一帧
				pktOut = getNextFrame();
			}
			else
			{
				//可能读metaData错误seek到需要的位置
				m_fileHander->seek(curFilePos + tag.dataSize, SEEK_SET);
				pktOut = getNextFrame();
			}
			break;
		default:
			LOG_ERROR(configData::getInstance().get_loggerId(), "invalid flv tag");
			break;
		}
	} while (0);
	return pktOut;
}

timedPacket flvParser::getNextFrame(flvPacketType type)
{
	while (true)
	{
		auto pktGeted = getNextFrame();

		if (pktGeted.data.size()<=0)
		{
			return pktGeted;
		}
		if (pktGeted.packetType != type)
		{
			for (auto i:pktGeted.data)
			{
				delete[]i.data;
				i.data = nullptr;
			}
		}
		else
		{
			return pktGeted;
		}
	}
}

double flvParser::duration()
{
	return m_duration;
}

bool flvParser::inited()
{
	return m_inited;
}

bool flvParser::seekToPos(long long pos)
{
	//m_fileHander->seek()
	return (0 == m_fileHander->seek(pos, SEEK_SET));
	
}

void flvParser::parseFlvHeader()
{
	do
	{
		int ret = 0;
		if (!m_fileHander->openSuccessed())
		{
			LOG_ERROR(configData::getInstance().get_loggerId(), "open file failed");
			return;
		}
		unsigned char tmp8;
		ret = m_fileHander->readFile(1, (char*)&tmp8);
		if (ret != 1)
		{
			LOG_ERROR(configData::getInstance().get_loggerId(), "read file failed");
			return;
		}
		if (tmp8 != FLV_F)
		{
			LOG_ERROR(configData::getInstance().get_loggerId(), "invalid flv file");
			return;
		}
		m_flvHeader.F = tmp8;
		ret = m_fileHander->readFile(1, (char*)&tmp8);
		if (ret != 1)
		{
			LOG_ERROR(configData::getInstance().get_loggerId(), "read file failed");
			return;
		}
		if (tmp8 != FLV_L)
		{
			LOG_ERROR(configData::getInstance().get_loggerId(), "invalid flv file");
			return;
		}
		m_flvHeader.L = tmp8;
		ret = m_fileHander->readFile(1, (char*)&tmp8);
		if (ret != 1)
		{
			LOG_ERROR(configData::getInstance().get_loggerId(), "read file failed");
			return;
		}
		if (tmp8 != FLV_V)
		{
			LOG_ERROR(configData::getInstance().get_loggerId(), "invalid flv file");
			return;
		}
		m_flvHeader.V = tmp8;
		ret = m_fileHander->readFile(1, (char*)&m_flvHeader.version);
		if (ret != 1)
		{
			LOG_ERROR(configData::getInstance().get_loggerId(), "read file failed");
			return;
		}
		ret = m_fileHander->readFile(1, (char*)&tmp8);
		if (ret != 1)
		{
			LOG_ERROR(configData::getInstance().get_loggerId(), "read file failed");
			return;
		}
		m_flvHeader.typeFlagsReservedAudio = (tmp8 & 0x38);
		if (m_flvHeader.typeFlagsReservedAudio != 0)
		{
			LOG_ERROR(configData::getInstance().get_loggerId(), "invalid flv file");
			return;
		}
		m_flvHeader.typeFlagsAudio = ((tmp8 & 4) >> 2);
		if (m_flvHeader.typeFlagsAudio == 0)
		{
			//LOG_INFO(configData::getInstance().get_loggerId(), "no audio");
		}
		else
		{
			//LOG_INFO(configData::getInstance().get_loggerId(), "has audio");
		}
		m_flvHeader.typeFlagsReservedVideo = ((tmp8 & 2) >> 1);
		if (m_flvHeader.typeFlagsReservedVideo != 0)
		{
			LOG_ERROR(configData::getInstance().get_loggerId(), "invalid flv file");
			return;
		}
		m_flvHeader.typeFlagsVideo = (tmp8 & 1);
		if (m_flvHeader.typeFlagsVideo == 0)
		{
			//LOG_INFO(configData::getInstance().get_loggerId(), "no video");
		}
		else
		{
			//LOG_INFO(configData::getInstance().get_loggerId(), "has video");
		}
		unsigned int tmp32 = 0;
		m_fileHander->readFile(4, (char*)&tmp32);
		tmp32 = ntohl(tmp32);
		m_flvHeader.dataOffset = tmp32;
		long long cur = m_fileHander->curFile();
		if (cur != m_flvHeader.dataOffset)
		{
			m_fileHander->seek(m_flvHeader.dataOffset, SEEK_SET);
		}
		/*m_fileHander->readFile(4, (char*)&tmp32);
		tmp32 = ntohl(tmp32);
		unsigned int PreviousTagSize0 = tmp32;
		if (PreviousTagSize0!=0)
		{
		LOG_ERROR(configData::getInstance().get_loggerId(), "invalid flv file");
		return;
		}*/
	} while (0);
}

bool flvParser::readTag(flvTag &tagOut)
{
	bool result = true;
	do
	{
		int ret = m_fileHander->readFile(1, (char*)&tagOut.tagType);
		if (ret != 1)
		{
			result = false;
			LOG_ERROR(configData::getInstance().get_loggerId(), "invalid flv tag");
			break;
		}
		switch (tagOut.tagType)
		{
		case FLV_Tag_Audio:
			break;
		case FLV_Tag_Video:
			break;
		case FLV_Tag_ScriptData:
			break;
		default:
			result = false;
			LOG_ERROR(configData::getInstance().get_loggerId(), "invalid flv tag");
			break;
		}
		tagOut.dataSize = 0;
		ret = m_fileHander->readFile(3, (char*)&tagOut.dataSize);
		if (ret != 3)
		{
			result = false;
			LOG_ERROR(configData::getInstance().get_loggerId(), "invalid flv tag");
			break;
		}

		tagOut.dataSize = fastHton3(tagOut.dataSize);

		ret = m_fileHander->readFile(3, (char*)&tagOut.timestamp);
		if (ret != 3)
		{
			result = false;
			LOG_ERROR(configData::getInstance().get_loggerId(), "invalid flv tag");
			break;
		}
		tagOut.timestamp = fastHton3(tagOut.timestamp);
		ret = m_fileHander->readFile(1, (char*)&tagOut.timestampExtended);
		tagOut.timestampFull = (((tagOut.timestampExtended & 0xff) << 24) | (tagOut.timestamp));
		auto cur = m_fileHander->curFile();
		tagOut.streamID = 0;
		ret = m_fileHander->readFile(3, (char*)&tagOut.streamID);
		if (ret != 3 || tagOut.streamID != 0)
		{
			result = false;
			LOG_ERROR(configData::getInstance().get_loggerId(), "invalid flv tag");
			break;
		}
		tagOut.data = nullptr;
	} while (0);

	return result;
}

bool flvParser::parseScriptData(flvTag &scriptTag)
{
	bool result = true;
	long long cur = m_fileHander->curFile();
	long long startCur = cur;
	scriptDataValue	scriptValue;
	int ret = 0;

	do
	{
		while (cur<startCur + scriptTag.dataSize)
		{
			cur = m_fileHander->curFile();
			ret = m_fileHander->readFile(1, (char*)&scriptValue.type);
			if (ret != 1)
			{
				LOG_ERROR(configData::getInstance().get_loggerId(), "invalid flv tag");
				result = false;
				break;
			}
			scriptDataString stringValue;
			unsigned int tmp32;
			unsigned short tmp16;
			unsigned char tmp8;
			unsigned long long tmp64;
			double	valueDouble = 0.0;
			bool	valueBool = false;
			switch (scriptValue.type)
			{
			case 0:
				ret = m_fileHander->readFile(8, (char*)&tmp64);
				if (ret != 8)
				{
					LOG_ERROR(configData::getInstance().get_loggerId(), "invalid flv tag");
					result = false;
					break;
				}
				tmp64 = fastHtonll(tmp64);
				valueDouble = *reinterpret_cast<double*>(&tmp64);
				LOG_INFO(configData::getInstance().get_loggerId(), valueDouble);
				break;
			case 1:
				ret = m_fileHander->readFile(1, (char*)&tmp8);
				if (ret != 1)
				{
					LOG_ERROR(configData::getInstance().get_loggerId(), "invalid flv tag");
					result = false;
					break;
				}
				valueBool = tmp8;
				LOG_INFO(configData::getInstance().get_loggerId(), valueBool);
				break;
				break;
			case 2:
				ret = m_fileHander->readFile(2, (char*)&stringValue.stringLength);
				if (ret != 2)
				{
					LOG_ERROR(configData::getInstance().get_loggerId(), "invalid flv tag");
					result = false;
					break;
				}
				stringValue.stringLength = ntohs(stringValue.stringLength);
				stringValue.stringData.resize(stringValue.stringLength);
				ret = m_fileHander->readFile(stringValue.stringLength, const_cast<char*>(stringValue.stringData.c_str()));
				if (ret != stringValue.stringLength)
				{
					LOG_ERROR(configData::getInstance().get_loggerId(), "invalid flv tag");
					result = false;
					break;
				}
				//LOG_INFO(configData::getInstance().get_loggerId(), stringValue.stringData);
				break;
			case 3:
				ret = 0;
				break;
			case 4:
				ret = 0;
				break;
			case 5:
				ret = 0;
				break;
			case 6:
				ret = 0;
				break;
			case 7:
				ret = 0;
				break;
			case 8:
				//读取数组个数
				ret = m_fileHander->readFile(4, (char*)&tmp32);
				if (ret != 4)
				{
					LOG_ERROR(configData::getInstance().get_loggerId(), "invalid flv tag");
					result = false;
					break;
				}
				cur = m_fileHander->curFile();
				tmp32 = ntohl(tmp32);
				for (int i = 0; i<tmp32; i++)
				{
					//key 
					ret = m_fileHander->readFile(2, (char*)&tmp16);
					if (ret != 2)
					{
						LOG_ERROR(configData::getInstance().get_loggerId(), "invalid flv tag");
						result = false;
						break;
					}
					tmp16 = ntohs(tmp16);
					stringValue.stringLength = tmp16;
					stringValue.stringData.resize(tmp16);
					ret = m_fileHander->readFile(stringValue.stringLength, const_cast<char*>(stringValue.stringData.c_str()));
					if (ret != stringValue.stringLength)
					{
						LOG_ERROR(configData::getInstance().get_loggerId(), "invalid flv tag");
						result = false;
						break;
					}
					//LOG_INFO(configData::getInstance().get_loggerId(), stringValue.stringData);
					//!key
					//value
					ret = m_fileHander->readFile(1, (char*)&tmp8);
					if (ret != 1)
					{
						LOG_ERROR(configData::getInstance().get_loggerId(), "invalid flv tag");
						result = false;
						break;
					}
					//根据类型取值
					switch (tmp8)
					{
					case 0:
						ret = m_fileHander->readFile(8, (char*)&tmp64);
						if (ret != 8)
						{
							LOG_ERROR(configData::getInstance().get_loggerId(), "invalid flv tag");
							result = false;
							break;
						}
						tmp64 = fastHtonll(tmp64);
						valueDouble = *reinterpret_cast<double*>(&tmp64);
						//LOG_INFO(configData::getInstance().get_loggerId(), valueDouble);
						if (stringValue.stringData == "duration")
						{
							m_duration = valueDouble;
						}
						break;
					case 1:
						ret = m_fileHander->readFile(1, (char*)&tmp8);
						if (ret != 1)
						{
							LOG_ERROR(configData::getInstance().get_loggerId(), "invalid flv tag");
							result = false;
							break;
						}
						valueBool = tmp8;
						LOG_INFO(configData::getInstance().get_loggerId(), valueBool);
						break;
					case 2:
						ret = m_fileHander->readFile(2, (char*)&stringValue.stringLength);
						if (ret != 2)
						{
							LOG_ERROR(configData::getInstance().get_loggerId(), "invalid flv tag");
							result = false;
							break;
						}
						stringValue.stringLength = ntohs(stringValue.stringLength);
						stringValue.stringData.resize(stringValue.stringLength);
						ret = m_fileHander->readFile(stringValue.stringLength, const_cast<char*>(stringValue.stringData.c_str()));
						if (ret != stringValue.stringLength)
						{
							LOG_ERROR(configData::getInstance().get_loggerId(), "invalid flv tag");
							result = false;
							break;
						}
						//LOG_INFO(configData::getInstance().get_loggerId(), stringValue.stringData);
						break;
					case 3:
						ret = 0;
						break;
					case 4:
						ret = 0;
						break;
					case 5:
						ret = 0;
						break;
					case 6:
						ret = 0;
						break;
					case 7:
						ret = 0;
						break;
					case 8:
						break;
					case 10:
						break;
					case 11:
						break;
					case 12:
						break;
					default:
						break;
					}
					//double
					//!value
				}
				//read array end
				ret = m_fileHander->readFile(3, (char*)&tmp32);
				if (ret != 3)
				{
					LOG_ERROR(configData::getInstance().get_loggerId(), "invalid flv tag");
					result = false;
					break;
				}
				tmp32 = fastHton3(tmp32);
				if (tmp32 != 9)
				{
					LOG_ERROR(configData::getInstance().get_loggerId(), "invalid flv tag");
					result = false;
					break;
				}
				//!array end
				break;
			case 10:
				ret = 0;
				break;
			case 11:
				ret = 0;
				break;
			case 12:
				ret = 0;
				break;
			default:
				LOG_ERROR(configData::getInstance().get_loggerId(), "invalid flv tag");
				cur = m_fileHander->curFile();
				result = false;
				break;
			}
			cur = m_fileHander->curFile();
			if (!result)
			{
				break;
			}
		}
	} while (0);
	return result;
}

bool flvParser::parserAudioData(flvTag & audioTag, timedPacket & outPkt)
{
	bool result = true;
	int ret = 0;
	do
	{
		unsigned char tmp8;
		ret = m_fileHander->readFile(1, (char*)&tmp8);
		if (ret != 1)
		{
			LOG_ERROR(configData::getInstance().get_loggerId(), "read audio data failed");
			result = false;
			break;
		}
		int soundFormat = 0;
		int soundRate = 0;
		int soundSize = 0;
		int soundType = 0;
		//parse audio 
		soundFormat = ((tmp8 & 0xf0) >> 4);
		soundRate = ((tmp8 & 0xc) >> 2);
		soundSize = ((tmp8 & 0x2) >> 1);
		soundType = ((tmp8 & 0x1));
		//aac
		if (soundFormat == 10)
		{
			ret = m_fileHander->readFile(1, (char*)&tmp8);
			if (ret != 1)
			{
				LOG_ERROR(configData::getInstance().get_loggerId(), "read audio data failed");
				result = false;
				break;
			}
			aacAudioData aacData;
			aacData.aacPacketType = tmp8;
			outPkt.timestamp = audioTag.timestamp;
			outPkt.packetType = flv_pkt_audio;
			DataPacket tmpData;
			tmpData.size = audioTag.dataSize - 2;
			tmpData.data = new  char[tmpData.size];
			ret = m_fileHander->readFile(tmpData.size, (char*)tmpData.data);
			if (ret != tmpData.size)
			{
				LOG_ERROR(configData::getInstance().get_loggerId(), "read audio data failed");
				result = false;
				break;
			}
			outPkt.data.push_back(tmpData);
		}
		else
		{
			long long cur = m_fileHander->curFile();
			//-1 last read one byte
			m_fileHander->seek(cur + audioTag.dataSize - 1, SEEK_SET);
			outPkt.packetType = flv_pkt_audio;

		}
	} while (0);
	return result;
}

bool flvParser::parserVideoData(flvTag & videoTag, timedPacket & outPkt)
{
	bool result = true;
	do
	{
		int ret = 0;
		videoData	videodata;
		unsigned char tmp8;
		unsigned int tmp32;
		ret = m_fileHander->readFile(1, (char*)&tmp8);
		if (ret != 1)
		{
			LOG_ERROR(configData::getInstance().get_loggerId(), "read video data failed");
			result = false;
			break;
		}
		videodata.frameType = ((tmp8 & 0xf0) >> 4);
		videodata.codecID = (tmp8 & 0xf);
		if (videodata.codecID == 7)
		{
			avcVideoPacket avcvideopacket;
			ret = m_fileHander->readFile(1, (char*)&tmp8);
			if (ret != 1)
			{
				LOG_ERROR(configData::getInstance().get_loggerId(), "read video data failed");
				result = false;
				break;
			}
			avcvideopacket.avcPacketType = tmp8;
			tmp32 = 0;
			ret = m_fileHander->readFile(3, (char*)&tmp32);
			if (ret != 3)
			{
				LOG_ERROR(configData::getInstance().get_loggerId(), "read video data failed");
				result = false;
				break;
			}
			tmp32 = fastHton3(tmp32);
			avcvideopacket.compositionTime = tmp32;
			outPkt.packetType = raw_h264_frame;
			outPkt.timestamp = videoTag.timestampFull;
			/*outPkt.size = videoTag.dataSize - 5;
			outPkt.data = new unsigned char[outPkt.size];
			ret = m_fileHander->readFile(outPkt.size, (char*)outPkt.data);
			if (ret!=outPkt.size)
			{
			LOG_ERROR(configData::getInstance().get_loggerId(), "read video data failed");
			result = false;
			break;
			}*/
			if (avcvideopacket.avcPacketType == 0)
			{
				AVCDecoderConfigurationRecord configurationRecord;
				ret = m_fileHander->readFile(1, (char*)&configurationRecord.configurationVersion);
				if (ret != 1)
				{
					LOG_ERROR(configData::getInstance().get_loggerId(), "read video data failed");
					result = false;
					break;
				}
				ret = m_fileHander->readFile(1, (char*)&configurationRecord.AVCProfileIndication);
				if (ret != 1)
				{
					LOG_ERROR(configData::getInstance().get_loggerId(), "read video data failed");
					result = false;
					break;
				}
				ret = m_fileHander->readFile(1, (char*)&configurationRecord.profile_compatibility);
				if (ret != 1)
				{
					LOG_ERROR(configData::getInstance().get_loggerId(), "read video data failed");
					result = false;
					break;
				}
				ret = m_fileHander->readFile(1, (char*)&configurationRecord.AVCLevelIndication);
				if (ret != 1)
				{
					LOG_ERROR(configData::getInstance().get_loggerId(), "read video data failed");
					result = false;
					break;
				}
				ret = m_fileHander->readFile(1, (char*)&tmp8);
				if (ret != 1)
				{
					LOG_ERROR(configData::getInstance().get_loggerId(), "read video data failed");
					result = false;
					break;
				}
				configurationRecord.reserved6 = ((tmp8 & 0xfc) >> 2);
				configurationRecord.lengthSizeMinusOne2 = (tmp8 & 0x3);
				ret = m_fileHander->readFile(1, (char*)&tmp8);
				if (ret != 1)
				{
					LOG_ERROR(configData::getInstance().get_loggerId(), "read video data failed");
					result = false;
					break;
				}
				configurationRecord.reserved3 = ((tmp8 & 0xE0) >> 5);
				configurationRecord.numOfSequenceParameterSets5 = (tmp8 & 0x1f);

				spsppsRecord tmpsps;
				for (int i = 0; i < configurationRecord.numOfSequenceParameterSets5; i++)
				{
					//sps
					ret = m_fileHander->readFile(2, (char*)&tmpsps.setlength);
					if (ret != 2)
					{
						LOG_ERROR(configData::getInstance().get_loggerId(), "read video data failed");
						result = false;
						break;
					}
					DataPacket spsPacket;
					spsPacket.size = ntohs(tmpsps.setlength);
					spsPacket.data = new char[spsPacket.size];
					ret = m_fileHander->readFile(spsPacket.size, (char*)spsPacket.data);
					if (ret != spsPacket.size)
					{
						LOG_ERROR(configData::getInstance().get_loggerId(), "read video data failed");
						delete[]spsPacket.data;
						result = false;
						break;
					}
					outPkt.data.push_back(spsPacket);
				}
				//pps
				ret = m_fileHander->readFile(1, (char*)&configurationRecord.numOfPictureParameterSets);
				if (ret != 1)
				{
					LOG_ERROR(configData::getInstance().get_loggerId(), "read video data failed");
					result = false;
					break;
				}
				for (int i = 0; i < configurationRecord.numOfPictureParameterSets; i++)
				{

					if (ret != 1)
					{
						LOG_ERROR(configData::getInstance().get_loggerId(), "read video data failed");
						result = false;
						break;
					}
					ret = m_fileHander->readFile(2, (char*)&tmpsps.setlength);
					if (ret != 2)
					{
						LOG_ERROR(configData::getInstance().get_loggerId(), "read video data failed");
						result = false;
						break;
					}
					DataPacket ppsPacket;
					ppsPacket.size = ntohs(tmpsps.setlength);
					ppsPacket.data = new char[ppsPacket.size];
					ret = m_fileHander->readFile(ppsPacket.size, (char*)ppsPacket.data);
					if (ret != ppsPacket.size)
					{
						LOG_ERROR(configData::getInstance().get_loggerId(), "read video data failed");
						delete[]ppsPacket.data;
						result = false;
						break;
					}
					outPkt.data.push_back(ppsPacket);
				}
			}
			else if (avcvideopacket.avcPacketType == 1)
			{
				int totalBufSize = videoTag.dataSize - 5;
				int readedSize = 0;
				while (readedSize<totalBufSize)
				{

					DataPacket dataPacket;
					//四个字节大小
					ret = m_fileHander->readFile(4, (char*)&tmp32);
					readedSize += 4;
					dataPacket.size = htonl(tmp32);
					dataPacket.data = new char[dataPacket.size];
					ret = m_fileHander->readFile(dataPacket.size, (char*)dataPacket.data);
					readedSize += dataPacket.size;
					if (ret != dataPacket.size)
					{
						LOG_ERROR(configData::getInstance().get_loggerId(), "read video data failed");
						delete[]dataPacket.data;
						result = false;
						break;
					}
					outPkt.data.push_back(dataPacket);
				}

			}
			else
			{
				LOG_INFO(configData::getInstance().get_loggerId(), "avcPacketType:" << avcvideopacket.avcPacketType);
				break;
			}
		}
		else
		{
			long long cur = m_fileHander->curFile();
			//-1 last read one byte
			m_fileHander->seek(cur + videoTag.dataSize - 1, SEEK_SET);

		}
	} while (0);
	return result;
}
