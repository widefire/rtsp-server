#include "flvWriter.h"
#include "configData.h"
#include "BitReader.h"
#include "h264FileParser.h"
#include "socket_type.h"
#include "flvFormat.h"
#include <iterator>
#include <algorithm>

flvWriter::flvWriter() :m_inited(false), m_fileHandler(nullptr), m_durationPos(0), m_end(false),
m_firstKeyframeWrited(false), m_firstTimeStamp(0),
m_fileSize(0.0), m_fps(0.0), m_width(0.0), m_height(0.0), m_duration(0.0)
{
	m_encoder = configData::getInstance().get_server_name();

}


flvWriter::~flvWriter()
{

	
	LOG_INFO(configData::getInstance().get_loggerId(), "flv write thread end");

	if (m_fileHandler)
	{
		//更新文件大小
		updateFileSize();
		updateDuration(m_duration);
		delete m_fileHandler;
		m_fileHandler = nullptr;
	}
	m_mutexSpsPps.lock();
	if (m_sps.data)
	{
		delete[]m_sps.data;
		m_sps.data = nullptr;
	}
	if (m_pps.data)
	{
		delete[]m_pps.data;
		m_pps.data = nullptr;
	}
	if (m_metaData.data)
	{
		delete[]m_metaData.data;
		m_metaData.data = nullptr;
	}
	m_mutexSpsPps.unlock();
	if (m_idxWriter)
	{
		delete m_idxWriter;
		m_idxWriter = nullptr;
	}
}

bool flvWriter::initFile(std::string fileName)
{
	if (m_inited == true)
	{
		return false;
	}
	do
	{
		if (fileName.size() <= 0)
		{
			m_inited = false;
			break;
		}

		m_fileName = fileName;

		//文件是否存在
		int exisit = access(fileName.c_str(), 0);
		if (exisit == 0)
		{
			LOG_WARN(configData::getInstance().get_loggerId(), "file:" << fileName << " is exist");
			m_inited = false;
			break;
		}
		//创建文件
		m_fileHandler = new fileHander(fileName, open_writeBinary);
		m_inited = m_fileHandler->openSuccessed();
		if (!m_inited)
		{
			delete m_fileHandler;
			m_fileHandler = nullptr;
			break;
		}
		//idx
		std::string idxName = fileName + idxStuff;
		exisit = access(idxName.c_str(), 0);
		if (exisit == 0)
		{
			LOG_WARN(configData::getInstance().get_loggerId(), "file:" << fileName << " is exist");
			break;
		}
		m_idxWriter = new flvIdxWriter();
		m_hasIdx = m_idxWriter->initWriter(idxName);
	} while (0);

	m_inited = true;

	return m_inited;
}

bool flvWriter::addTimedPacket(timedPacket & packet)
{
	if (!m_inited)
	{
		return false;
	}
	do
	{
		m_duration = (packet.timestamp - m_firstTimeStamp) / 1000.0;
		//判断帧类型
		std::lock_guard<std::mutex> sps_pps_guard(m_mutexSpsPps);
		if (!m_sps.data || !m_pps.data)
		{

			flv_h264_nal_type nalType;
			for (auto i : packet.data)
			{
				nalType = (flv_h264_nal_type)(i.data[0] & 0x1f);
				switch (nalType)
				{
				case flvWriter::flv_h264_nal_sps:
					if (m_sps.data)
					{
						delete[]m_sps.data;
						m_sps.data = nullptr;
					}
					m_sps.size = i.size;
					m_sps.data = new char[i.size];
					memcpy(m_sps.data, i.data, i.size);
					m_firstTimeStamp = packet.timestamp;
					break;
				case flvWriter::flv_h264_nal_pps:
					if (m_pps.data)
					{
						delete[]m_pps.data;
						m_pps.data = nullptr;
					}
					m_pps.size = i.size;
					m_pps.data = new char[i.size];
					memcpy(m_pps.data, i.data, i.size);
					if (m_sps.data&&m_pps.data)
					{
						if (!generateMetaData())
						{
							LOG_ERROR(configData::getInstance().get_loggerId(), "generate metadata failed");
							m_inited = false;
						}
					}
					m_firstTimeStamp = packet.timestamp;
					break;
				default:
					break;
				}
			}
		}
		//!判断帧类型
		//添加到队列
		if (m_sps.data&&m_pps.data)
		{
			timedPacket tmpPacket;
			tmpPacket.packetType = packet.packetType;
			tmpPacket.timestamp = packet.timestamp - m_firstTimeStamp;
			DataPacket tmpData;
			for (auto i : packet.data)
			{
				tmpData.size = i.size;
				tmpData.data = new char[i.size];
				memcpy(tmpData.data, i.data, i.size);
				tmpPacket.data.push_back(tmpData);
			}
			processPacket(tmpPacket);
		}
		//!添加到队列
	} while (0);
	return true;
}

bool flvWriter::generateMetaData()
{
	bool result = true;
	const int maxMetaDataSize = 8192;
	char *metadataBuff = nullptr;
	unsigned char tmp8;
	unsigned short tmp16;
	unsigned int tmp24;
	unsigned int tmp32;
	unsigned long long tmp64;
	double valueDouble;
	do
	{
		if (!parseSps())
		{
			result = false;
			break;
		}
		metadataBuff = new char[maxMetaDataSize];
		int cur = 0;
		//FLV header 
		metadataBuff[cur] = 'F';
		cur++;
		metadataBuff[cur] = 'L';
		cur++;
		metadataBuff[cur] = 'V';
		cur++;
		//version
		metadataBuff[cur] = 0x1;
		cur++;
		//有音频 
		tmp8 = 0x1;
		metadataBuff[cur] = tmp8;
		cur++;
		//offset
		tmp32 = htonl(9);
		memcpy(metadataBuff + cur, &tmp32, 4);
		cur += 4;
		//first PreviousTagSize0  always 0
		tmp32 = htonl(0);
		memcpy(metadataBuff + cur, &tmp32, 4);
		cur += 4;
		//script tag
		flvTag scriptTag;
		scriptTag.tagType = 18;
		scriptTag.timestamp = scriptTag.timestampExtended = scriptTag.timestampFull = 0;
		scriptTag.streamID = 0;
		//现在不知道dataSize;留下dataSize的位置
		scriptTag.dataSize = 0;
		long long scriptDataSizePos = 0;
		tmp8 = scriptTag.tagType;
		metadataBuff[cur] = tmp8;
		cur++;
		tmp24 = fastHton3(scriptTag.dataSize);
		char *ptrFor24 = (char*)&tmp24;
		scriptDataSizePos = cur;
		memcpy(metadataBuff + cur, ptrFor24, 3);
		cur += 3;
		tmp32 = htonl(scriptTag.timestampFull);
		memcpy(metadataBuff + cur, &tmp32, 4);
		cur += 4;

		tmp24 = scriptTag.streamID;
		ptrFor24 = (char*)&tmp24;
		memcpy(metadataBuff + cur, ptrFor24, 3);
		tmp24 = fastHton3(tmp24);
		cur += 3;
		//!script tag
		scriptDataValue	scriptValue;
		scriptDataString stringValue;
		//onMetaData
		scriptValue.type = 2;
		stringValue.stringData = "onMetaData";
		stringValue.stringLength = 10;
		tmp8 = scriptValue.type;
		metadataBuff[cur] = tmp8;
		cur++;
		tmp16 = htons(stringValue.stringLength);
		memcpy(metadataBuff + cur, &tmp16, 2);
		cur += 2;
		memcpy(metadataBuff + cur, stringValue.stringData.c_str(), stringValue.stringLength);
		cur += stringValue.stringLength;
		//!onMetaData
		//array
		scriptValue.type = 8;
		tmp8 = scriptValue.type;
		metadataBuff[cur] = tmp8;
		cur++;
		int numArray = 6;
		tmp32 = htonl(numArray);
		memcpy(metadataBuff + cur, &tmp32, 4);
		cur += 4;
		stringValue.stringData = "framerate";
		stringValue.stringLength = stringValue.stringData.size();
		tmp16 = htons(stringValue.stringLength);
		memcpy(metadataBuff + cur, &tmp16, 2);
		cur += 2;
		memcpy(metadataBuff + cur, stringValue.stringData.c_str(), stringValue.stringLength);
		cur += stringValue.stringLength;
		tmp8 = 0;
		metadataBuff[cur] = tmp8;
		cur++;
		m_fpsPos = cur;
		tmp64 = *reinterpret_cast<unsigned long long*>(&m_fps);
		tmp64 = fastHtonll(tmp64);
		memcpy(metadataBuff + cur, &tmp64, 8);
		cur += 8;

		stringValue.stringData = "width";
		stringValue.stringLength = stringValue.stringData.size();
		tmp16 = htons(stringValue.stringLength);
		memcpy(metadataBuff + cur, &tmp16, 2);
		cur += 2;
		memcpy(metadataBuff + cur, stringValue.stringData.c_str(), stringValue.stringLength);
		cur += stringValue.stringLength;
		tmp8 = 0;
		metadataBuff[cur] = tmp8;
		cur++;
		tmp64 = *reinterpret_cast<unsigned long long*>(&m_width);
		tmp64 = fastHtonll(tmp64);
		memcpy(metadataBuff + cur, &tmp64, 8);
		cur += 8;

		stringValue.stringData = "height";

		stringValue.stringLength = stringValue.stringData.size();
		tmp16 = htons(stringValue.stringLength);
		memcpy(metadataBuff + cur, &tmp16, 2);
		cur += 2;
		memcpy(metadataBuff + cur, stringValue.stringData.c_str(), stringValue.stringLength);
		cur += stringValue.stringLength;
		tmp8 = 0;
		metadataBuff[cur] = tmp8;
		cur++;
		tmp64 = *reinterpret_cast<unsigned long long*>(&m_height);
		tmp64 = fastHtonll(tmp64);
		memcpy(metadataBuff + cur, &tmp64, 8);
		cur += 8;

		stringValue.stringData = "duration";

		stringValue.stringLength = stringValue.stringData.size();
		tmp16 = htons(stringValue.stringLength);
		memcpy(metadataBuff + cur, &tmp16, 2);
		cur += 2;
		memcpy(metadataBuff + cur, stringValue.stringData.c_str(), stringValue.stringLength);
		cur += stringValue.stringLength;
		tmp8 = 0;
		metadataBuff[cur] = tmp8;
		cur++;
		m_durationPos = cur;
		tmp64 = *reinterpret_cast<unsigned long long*>(&m_duration);
		tmp64 = fastHtonll(tmp64);
		memcpy(metadataBuff + cur, &tmp64, 8);
		cur += 8;

		stringValue.stringData = "fileSize";
		stringValue.stringLength = stringValue.stringData.size();
		tmp16 = htons(stringValue.stringLength);
		memcpy(metadataBuff + cur, &tmp16, 2);
		cur += 2;
		memcpy(metadataBuff + cur, stringValue.stringData.c_str(), stringValue.stringLength);
		cur += stringValue.stringLength;
		tmp8 = 0;
		metadataBuff[cur] = tmp8;
		cur++;
		m_fileSizePos = cur;
		tmp64 = *reinterpret_cast<unsigned long long*>(&m_fileSize);
		tmp64 = fastHtonll(tmp64);
		memcpy(metadataBuff + cur, &tmp64, 8);
		cur += 8;

		stringValue.stringData = "encoder";
		stringValue.stringLength = stringValue.stringData.size();
		tmp16 = htons(stringValue.stringLength);
		memcpy(metadataBuff + cur, &tmp16, 2);
		cur += 2;
		memcpy(metadataBuff + cur, stringValue.stringData.c_str(), stringValue.stringLength);
		cur += stringValue.stringLength;
		tmp8 = 2;
		metadataBuff[cur] = tmp8;
		cur++;
		stringValue.stringData = m_encoder;
		stringValue.stringLength = stringValue.stringData.size();
		tmp16 = htons(stringValue.stringLength);
		memcpy(metadataBuff + cur, &tmp16, 2);
		cur += 2;
		memcpy(metadataBuff + cur, stringValue.stringData.c_str(), stringValue.stringLength);
		cur += stringValue.stringLength;
		//!array
		//end
		tmp24 = fastHton3(9);
		ptrFor24 = (char*)&tmp24;
		memcpy(metadataBuff + cur, ptrFor24, 3);
		cur += 3;
		//更新scriptDataSize
		tmp32 = fastHton3(cur - 11 - 9 - 4);
		memcpy(metadataBuff + scriptDataSizePos, ((char*)(&tmp32)), 3);

		//last frame size 即cur
		tmp32 = htonl(cur - 9 - 4);
		memcpy(metadataBuff + cur, &tmp32, 4);
		cur += 4;
		int ret = m_fileHandler->writeFile(cur, metadataBuff);
	} while (0);
	if (metadataBuff)
	{

		delete[]metadataBuff;
		metadataBuff = nullptr;
		m_metaData.size = 1;

	}
	return true;
}

bool flvWriter::processPacket(timedPacket & packet)
{
	//是否已经有了metadata
	if (m_metaData.size>0)
	{
		//丢弃关键帧之前的数据
		if (!m_firstKeyframeWrited)
		{
			int removedCounts = 0;
			bool bbreak = false;
			for (auto i:packet.data )
			{
				flv_h264_nal_type nalType= (flv_h264_nal_type)(i.data[0] & 0x1f);
				if (nalType==flv_h264_nal_slice_idr)
				{
					m_firstKeyframeWrited = true;
					bbreak = true;
				}
				if (bbreak)
				{
					break;
				}
				else
				{
					removedCounts++;
				}
			}
			for (int i = 0; i < removedCounts; i++)
			{
				delete []packet.data.front().data;
				packet.data.pop_front();
			}
		}
		if (packet.data.size()>0)
		{

			//写数据
			writePacket(packet);
			//更新时长
			updateDuration(m_duration);
		}
	}
	//释放
	for (auto i:packet.data)
	{
		delete[]i.data;
		i.data = nullptr;
	}
	packet.data.clear();
	return false;
}

bool flvWriter::writePacket(timedPacket & packet)
{
	bool result = true;
	if (packet.packetType == raw_h264_frame)
	{
		flv_h264_nal_type nalType;
		flvTag tag;
		videoData videoTag;
		avcVideoPacket avcvideopacket;
		unsigned int lastFrameSize = 0;
		unsigned char tmp8;
		unsigned short tmp16;
		unsigned int tmp24;
		unsigned int tmp32;
		for (auto i : packet.data)
		{
			nalType = (flv_h264_nal_type)(i.data[0] & 0x1f);
			if (nalType == flv_h264_nal_sps)
			{
				if (m_hasIdx)
				{
					flvIdx idx;
					idx.time = packet.timestamp / 1000.0;
					//四个字节的上个包的大小
					idx.offset = m_fileHandler->curFile() - 4;
					m_idxWriter->appendIdx(idx);
				}
			}
			else if (nalType == flv_h264_nal_pps)
			{
				//AVCDecoderConfigurationRecord
				tag.tagType = 9;
				tag.dataSize = 0;
				tag.timestampFull = packet.timestamp;
				tag.timestamp = (packet.timestamp & 0xffffff);
				tag.timestampExtended = ((packet.timestamp & 0xff000000) >> 24);
				tag.streamID = 0;
				videoTag.frameType = 1;
				videoTag.codecID = 7;

				avcvideopacket.avcPacketType = 0;
				avcvideopacket.compositionTime = 0;

				AVCDecoderConfigurationRecord avcDecoderConfigurationRecord;
				avcDecoderConfigurationRecord.configurationVersion = 1;
				avcDecoderConfigurationRecord.AVCProfileIndication = m_sps.data[1];
				avcDecoderConfigurationRecord.profile_compatibility = m_sps.data[2];
				avcDecoderConfigurationRecord.AVCLevelIndication = m_sps.data[3];
				avcDecoderConfigurationRecord.reserved6 = 0x3f;
				avcDecoderConfigurationRecord.lengthSizeMinusOne2 = 3;
				avcDecoderConfigurationRecord.reserved3 = 0x7;
				avcDecoderConfigurationRecord.numOfSequenceParameterSets5 = 1;
				avcDecoderConfigurationRecord.numOfPictureParameterSets = 1;

				//1:videoTag 4:AVCVIDEOPACKET 7:AVCDecoderConfigurationRecord 
				tag.dataSize = 1 + 4 + 7 + 2 + m_sps.size + 2 + m_pps.size;
				//flvTag的size
				lastFrameSize = tag.dataSize + 11;
				//开始写数据
				unsigned int totalSize = lastFrameSize + 4;
				char *bufSend = new char[totalSize];
				char *tmpPtr = nullptr;
				int cur = 0;
				//flv tag
				bufSend[cur] = tag.tagType;
				cur++;
				tmp24 = fastHton3(tag.dataSize);
				tmpPtr = (char*)&tmp24;
				memcpy(bufSend + cur, tmpPtr, 3);
				cur += 3;


				tmp24 = fastHton3(tag.timestamp);
				tmpPtr = (char*)&tmp24;
				memcpy(bufSend + cur, tmpPtr, 3);
				cur += 3;
				tmp8 = fastHton3(tag.timestampExtended);
				bufSend[cur] = tmp8;
				cur++;


				tmp24 = fastHton3(tag.streamID);
				tmpPtr = (char*)&tmp24;
				memcpy(bufSend + cur, tmpPtr, 3);
				cur += 3;
				//!flv tag
				//videodata
				tmp8 = (((videoTag.frameType & 0xf) << 4) | (videoTag.codecID & 0xf));
				bufSend[cur] = tmp8;
				cur++;
				//!videodata
				//avcvideopacket
				tmp8 = avcvideopacket.avcPacketType;
				bufSend[cur] = tmp8;
				cur++;
				tmp24 = fastHton3(avcvideopacket.compositionTime);
				tmpPtr = (char*)&tmp24;
				memcpy(bufSend + cur, tmpPtr, 3);
				cur += 3;
				//!avcvideopacket
				//AVCDecoderConfigurationRecord
				tmp8 = avcDecoderConfigurationRecord.configurationVersion;
				bufSend[cur] = tmp8;
				cur++;
				tmp8 = avcDecoderConfigurationRecord.AVCProfileIndication;
				bufSend[cur] = tmp8;
				cur++;
				tmp8 = avcDecoderConfigurationRecord.profile_compatibility;
				bufSend[cur] = tmp8;
				cur++;
				tmp8 = avcDecoderConfigurationRecord.AVCLevelIndication;
				bufSend[cur] = tmp8;
				cur++;
				tmp8 = (((avcDecoderConfigurationRecord.reserved6 & 0x3f) << 2) | (avcDecoderConfigurationRecord.lengthSizeMinusOne2 & 0x3));
				bufSend[cur] = tmp8;
				cur++;
				tmp8 = (((avcDecoderConfigurationRecord.reserved3 & 0x7) << 5) |
					(avcDecoderConfigurationRecord.numOfSequenceParameterSets5));
				bufSend[cur] = tmp8;
				cur++;

				tmp16 = htons(m_sps.size);
				memcpy(bufSend + cur, &tmp16, 2);
				cur += 2;
				memcpy(bufSend + cur, m_sps.data, m_sps.size);
				cur += m_sps.size;

				tmp8 = avcDecoderConfigurationRecord.numOfPictureParameterSets;
				bufSend[cur] = tmp8;
				cur++;
				tmp16 = htons(m_pps.size);
				memcpy(bufSend + cur, &tmp16, 2);
				cur += 2;
				memcpy(bufSend + cur, m_pps.data, m_pps.size);
				cur += m_pps.size;
				//!AVCDecoderConfigurationRecord
				//lastframe size
				tmp32 = htonl(lastFrameSize);
				memcpy(bufSend + cur, &tmp32, 4);
				cur += 4;
				//!lastframe size
				m_fileHandler->writeFile(totalSize, bufSend);
				delete[]bufSend;
				bufSend = nullptr;
			}
			else
			{
				if (nalType == flv_h264_nal_slice_idr)
				{
					videoTag.frameType = 1;
					if (m_hasIdx)
					{
						flvIdx idx;
						idx.time = packet.timestamp / 1000.0;
						//四个字节的上个包的大小
						idx.offset = m_fileHandler->curFile() - 4;
						m_idxWriter->appendIdx(idx);
					}
				}
				else
				{
					videoTag.frameType = 2;
				}

				tag.tagType = 9;
				tag.dataSize = 0;
				tag.timestampFull = packet.timestamp;
				tag.timestamp = (packet.timestamp & 0xffffff);
				tag.timestampExtended = ((packet.timestamp & 0xff000000) >> 24);
				tag.streamID = 0;

				videoTag.codecID = 7;


				avcvideopacket.avcPacketType = 1;
				avcvideopacket.compositionTime = 0;


				int avcdataSize = i.size;
				//四个字节的大小,
				tag.dataSize = avcdataSize + 4 + 4 + 1;
				//flvTag的size
				lastFrameSize = tag.dataSize + 11;
				//开始写数据
				unsigned int totalSize = lastFrameSize + 4;
				char *bufSend = new char[totalSize];
				char *tmpPtr = nullptr;
				int cur = 0;
				//flv tag
				bufSend[cur] = tag.tagType;
				cur++;
				tmp24 = fastHton3(tag.dataSize);
				tmpPtr = (char*)&tmp24;
				memcpy(bufSend + cur, tmpPtr, 3);
				cur += 3;

				tmp24 = fastHton3(tag.timestamp);
				tmpPtr = (char*)&tmp24;
				memcpy(bufSend + cur, tmpPtr, 3);
				cur += 3;
				tmp8 = fastHton3(tag.timestampExtended);
				bufSend[cur] = tmp8;
				cur++;

				tmp24 = fastHton3(tag.streamID);
				tmpPtr = (char*)&tmp24;
				memcpy(bufSend + cur, tmpPtr, 3);
				cur += 3;
				//!flv tag
				//videodata
				tmp8 = (((videoTag.frameType & 0xf) << 4) | (videoTag.codecID & 0xf));
				bufSend[cur] = tmp8;
				cur++;
				//!videodata
				//avcvideopacket
				tmp8 = avcvideopacket.avcPacketType;
				bufSend[cur] = tmp8;
				cur++;
				tmp24 = fastHton3(avcvideopacket.compositionTime);
				tmpPtr = (char*)&tmp24;
				memcpy(bufSend + cur, tmpPtr, 3);
				cur += 3;
				tmp32 = htonl(avcdataSize);
				memcpy(bufSend + cur, &tmp32, 4);
				cur += 4;
				memcpy(bufSend + cur, i.data, i.size);
				cur += i.size;
				//!avcvideopacket
				//lastframe size
				tmp32 = htonl(lastFrameSize);
				memcpy(bufSend + cur, &tmp32, 4);
				cur += 4;
				//!lastframe size
				m_fileHandler->writeFile(totalSize, bufSend);
				delete[]bufSend;
				bufSend = nullptr;
			}
		}

	}
	return result;
}

bool flvWriter::parseSps()
{
	bool result = true;
	char *tmp_sps = 0;
	int web_sps_len = 0;
	CBitReader	bit_reader;
	do
	{
		if (!m_sps.data || !m_pps.data)
		{
			result = false;
			break;
		}
		tmp_sps = new char[m_sps.size];
		memcpy(tmp_sps, m_sps.data, m_sps.size);
		web_sps_len = m_sps.size;
		emulation_prevention((unsigned char*)tmp_sps, web_sps_len);
		bit_reader.SetBitReader((unsigned char*)tmp_sps, web_sps_len);
		bit_reader.ReadBits(8);
		int frame_crop_left_offset = 0;
		int frame_crop_right_offset = 0;
		int frame_crop_top_offset = 0;
		int frame_crop_bottom_offset = 0;

		int profile_idc = bit_reader.ReadBits(8);
		int constraint_set0_flag = bit_reader.ReadBit();
		int constraint_set1_flag = bit_reader.ReadBit();
		int constraint_set2_flag = bit_reader.ReadBit();
		int constraint_set3_flag = bit_reader.ReadBit();
		int constraint_set4_flag = bit_reader.ReadBit();
		int constraint_set5_flag = bit_reader.ReadBit();
		int reserved_zero_2bits = bit_reader.ReadBits(2);
		int level_idc = bit_reader.ReadBits(8);
		int seq_parameter_set_id = bit_reader.ReadExponentialGolombCode();


		if (profile_idc == 100 || profile_idc == 110 ||
			profile_idc == 122 || profile_idc == 244 ||
			profile_idc == 44 || profile_idc == 83 ||
			profile_idc == 86 || profile_idc == 118)
		{
			int chroma_format_idc = bit_reader.ReadExponentialGolombCode();

			if (chroma_format_idc == 3)
			{
				int residual_colour_transform_flag = bit_reader.ReadBit();
			}
			int bit_depth_luma_minus8 = bit_reader.ReadExponentialGolombCode();
			int bit_depth_chroma_minus8 = bit_reader.ReadExponentialGolombCode();
			int qpprime_y_zero_transform_bypass_flag = bit_reader.ReadBit();
			int seq_scaling_matrix_present_flag = bit_reader.ReadBit();

			if (seq_scaling_matrix_present_flag)
			{
				int i = 0;
				for (i = 0; i < 8; i++)
				{
					int seq_scaling_list_present_flag = bit_reader.ReadBit();
					if (seq_scaling_list_present_flag)
					{
						int sizeOfScalingList = (i < 6) ? 16 : 64;
						int lastScale = 8;
						int nextScale = 8;
						int j = 0;
						for (j = 0; j < sizeOfScalingList; j++)
						{
							if (nextScale != 0)
							{
								int delta_scale = bit_reader.ReadSE();
								nextScale = (lastScale + delta_scale + 256) % 256;
							}
							lastScale = (nextScale == 0) ? lastScale : nextScale;
						}
					}
				}
			}
		}

		int log2_max_frame_num_minus4 = bit_reader.ReadExponentialGolombCode();
		int pic_order_cnt_type = bit_reader.ReadExponentialGolombCode();
		if (pic_order_cnt_type == 0)
		{
			int log2_max_pic_order_cnt_lsb_minus4 = bit_reader.ReadExponentialGolombCode();
		}
		else if (pic_order_cnt_type == 1)
		{
			int delta_pic_order_always_zero_flag = bit_reader.ReadBit();
			int offset_for_non_ref_pic = bit_reader.ReadSE();
			int offset_for_top_to_bottom_field = bit_reader.ReadSE();
			int num_ref_frames_in_pic_order_cnt_cycle = bit_reader.ReadExponentialGolombCode();
			int i;
			for (i = 0; i < num_ref_frames_in_pic_order_cnt_cycle; i++)
			{
				bit_reader.ReadSE();
				//sps->offset_for_ref_frame[ i ] = ReadSE();
			}
		}
		int max_num_ref_frames = bit_reader.ReadExponentialGolombCode();
		int gaps_in_frame_num_value_allowed_flag = bit_reader.ReadBit();
		int pic_width_in_mbs_minus1 = bit_reader.ReadExponentialGolombCode();
		int pic_height_in_map_units_minus1 = bit_reader.ReadExponentialGolombCode();
		int frame_mbs_only_flag = bit_reader.ReadBit();
		if (!frame_mbs_only_flag)
		{
			int mb_adaptive_frame_field_flag = bit_reader.ReadBit();
		}
		int direct_8x8_inference_flag = bit_reader.ReadBit();
		int frame_cropping_flag = bit_reader.ReadBit();
		if (frame_cropping_flag)
		{
			frame_crop_left_offset = bit_reader.ReadExponentialGolombCode();
			frame_crop_right_offset = bit_reader.ReadExponentialGolombCode();
			frame_crop_top_offset = bit_reader.ReadExponentialGolombCode();
			frame_crop_bottom_offset = bit_reader.ReadExponentialGolombCode();
		}
		int Width = ((pic_width_in_mbs_minus1 + 1) * 16) - frame_crop_bottom_offset * 2 - frame_crop_top_offset * 2;
		int Height = ((2 - frame_mbs_only_flag)* (pic_height_in_map_units_minus1 + 1) * 16) - (frame_crop_right_offset * 2) - (frame_crop_left_offset * 2);
		m_width = Width;
		m_height = Height;
		int vui_parameters_present_flag = bit_reader.ReadBit();

		if (vui_parameters_present_flag)
		{
			int aspect_ratio_info_present_flag = bit_reader.ReadBit();
			if (aspect_ratio_info_present_flag)
			{
				int aspect_ratio_idc = bit_reader.ReadBits(8);
				if (aspect_ratio_idc == 255)
				{
					int sar_width = bit_reader.ReadBits(16);
					int sar_height = bit_reader.ReadBits(16);
				}
			}
			int overscan_info_present_flag = bit_reader.ReadBit();
			if (overscan_info_present_flag)
				int overscan_appropriate_flagu = bit_reader.ReadBit();
			int video_signal_type_present_flag = bit_reader.ReadBit();
			if (video_signal_type_present_flag)
			{
				int video_format = bit_reader.ReadBits(3);
				int video_full_range_flag = bit_reader.ReadBit();
				int colour_description_present_flag = bit_reader.ReadBit();
				if (colour_description_present_flag)
				{
					int colour_primaries = bit_reader.ReadBits(8);
					int transfer_characteristics = bit_reader.ReadBits(8);
					int matrix_coefficients = bit_reader.ReadBits(8);
				}
			}
			int chroma_loc_info_present_flag = bit_reader.ReadBit();
			if (chroma_loc_info_present_flag)
			{
				int chroma_sample_loc_type_top_field = bit_reader.ReadExponentialGolombCode();
				int chroma_sample_loc_type_bottom_field = bit_reader.ReadExponentialGolombCode();
			}
			int timing_info_present_flag = bit_reader.ReadBit();
			if (timing_info_present_flag)
			{
				int num_units_in_tick = bit_reader.ReadBits(32);
				int time_scale = bit_reader.ReadBits(32);
				m_fps = time_scale / (2 * num_units_in_tick);
			}
		}
	} while (0);

	if (tmp_sps)
	{
		delete[]tmp_sps;
		tmp_sps = 0;
	}
	return result;
}

void flvWriter::updateDuration(double timeDuration)
{
	if (!m_fileHandler)
	{
		LOG_WARN(configData::getInstance().get_loggerId(), "fileHandler not vailded");
		return;
	}
	long long cur = m_fileHandler->curFile();
	if (0 != m_fileHandler->seek(m_durationPos, SEEK_SET))
	{
		LOG_WARN(configData::getInstance().get_loggerId(), "seek for duration failed");
		return;
	}
	unsigned long long tmp64 = *reinterpret_cast<unsigned long long*>(&timeDuration);
	tmp64 = fastHtonll(tmp64);
	/*fileHander rwHander(m_fileName, open_rewrite);
	rwHander.rewriteFile(8, (char*)&tmp64, m_durationPos);*/

	m_fileHandler->writeFile(8, (char*)&tmp64);

	m_fileHandler->seek(cur, SEEK_SET);
}

void flvWriter::updateFileSize()
{
	if (!m_fileHandler)
	{
		LOG_WARN(configData::getInstance().get_loggerId(), "fileHandler not vailded");
		return;
	}
	long long fileSize;
	if (0 != m_fileHandler->seek(0, SEEK_END))
	{
		LOG_WARN(configData::getInstance().get_loggerId(), "seek for filesize failed");
		return;
	}
	fileSize = m_fileHandler->curFile();
	/*fileHander rwHander(m_fileName, open_rewrite);
	fileSize = rwHander.fileSize();*/
	if (0 != m_fileHandler->seek(m_fileSizePos, SEEK_SET))
	{
		LOG_WARN(configData::getInstance().get_loggerId(), "seek for filesize failed");
		return;
	}
	double dfileSize = fileSize;
	long long tmp64 = *reinterpret_cast< long long*>(&dfileSize);
	tmp64 = fastHtonll(tmp64);
	m_fileHandler->writeFile(8, (char*)&tmp64);
	//rwHander.rewriteFile(8, (char*)&tmp64, m_fileSizePos);
}
