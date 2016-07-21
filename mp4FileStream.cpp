#include "mp4FileStream.h"
#include "BitReader.h"
#include "socket_type.h"
#include "util.h"


mp4FileStream::mp4FileStream(std::string fileName) :_fps(0), _widht(0), _height(0), _fp(nullptr), _firstPktTime(-1),
_getIdr(false), _lastVideoTimestamp(-1), _bStreamOpened(false), _bMP3(false),
_connectedAudioSampleOffset(0), _connectedVideoSampleOffset(0),
_curAudioChunkOffset(0), _curVideoChunkOffset(0),
_numVideoSamples(0), _numAudioSamples(0), _lastAudioTimeVal(0),
_audioFrameSize(0), _mdatStart(0), _mdatStop(0)
{
	_fp = fopen(fileName.c_str(), "wb+");
	InitFile();
}

mp4FileStream::~mp4FileStream()
{

	ShutdownFile();
	if (_sps.data)
	{
		delete[]_sps.data;
		_sps.data = nullptr;
	}
	if (_pps.data)
	{
		delete[]_pps.data;
		_pps.data = nullptr;
	}
	if (_fp)
	{
		fclose(_fp);
		_fp = nullptr;
	}
}

//使用后清除datapacket
void mp4FileStream::addTimedDataPacket(timedPacket & dataPacket)
{
	if (!_fp)
	{
		return;
	}
	//sps 7 ,pps 8 ,idr 5
	if (dataPacket.data.size() <= 0)
	{
		return;
	}
	int nalType = 0;

	switch (dataPacket.packetType)
	{
	case raw_h264_frame:
		for (auto i : dataPacket.data)
		{
			nalType = (i.data[0] & 0x1f);
			if (_sps.size == 0 && nalType == 7)
			{
				_sps.size = i.size;
				_sps.data = new char[i.size];
				memcpy(_sps.data, i.data, i.size);
				parseSps();
			}
			if (nalType == 8 && _pps.size == 0)
			{
				_pps.size = i.size;
				_pps.data = new char[i.size];
				memcpy(_pps.data, i.data, i.size);
			}
			if (_firstPktTime == -1 && (nalType == 5 || nalType == 5))
			{
				_firstPktTime = dataPacket.timestamp;
				_getIdr = true;
			}
			if (!_getIdr)
			{
				break;
			}
			addPacket(dataPacket);
		}
		break;
	default:
		break;
	}
	//for (auto i:dataPacket.data)
	//{
	//	if (i.data)
	//	{
	//		delete[]i.data;
	//		i.data = nullptr;
	//	}
	//}
	//dataPacket.data.clear();
}

unsigned int mp4FileStream::fourCC2NL(std::string stringConv)
{
	if (stringConv.size()<4)
	{
		return 0;
	}
	unsigned int out;
	out = ((stringConv.at(0) << 24) | (stringConv.at(1) << 16) | (stringConv.at(2) << 8) | (stringConv.at(3)));
	return out;
}

//这里不用清除Packet外层已经清除
void mp4FileStream::addPacket(timedPacket & packet)
{
	unsigned long long offset = fileTell(_fp);
	if (packet.packetType == raw_h264_frame)
	{
		for (auto i : packet.data)
		{
			//抛弃sps pps
			{
				if ((i.data[0] & 0x1f) == 7 ||
					(i.data[0] & 0x1f) == 8)
				{
					continue;
				}
			}
			//!抛弃sps pps
			unsigned int totalCopied = 0;

			//写入长度和数据
			//没有写长度，仅仅写了数据*******四个字节的长度，然后才是数据

			unsigned int dataSize = htonl(i.size);
			fwrite(&dataSize, 4, 1, _fp);
			char *dataWrite = new char[i.size];
			memcpy(dataWrite, i.data, i.size);
			if (1 != fwrite(dataWrite, i.size, 1, _fp))
			{
				delete[]dataWrite;
				break;
			}
			delete[]dataWrite;
			dataWrite = nullptr;
			totalCopied += i.size + 4;
			//!写入长度和数据

			if (!_videoFrames.size() || packet.timestamp != _lastVideoTimestamp)
			{
				MP4VideoFrameInfo frameInfo;
				frameInfo.fileOffset = offset;
				frameInfo.size = totalCopied;
				frameInfo.timestamp = packet.timestamp;
				frameInfo.compositionOffset = 0;

				if (i.data[0] == 0x17)
				{
					_IFrameIDs.push_back(_videoFrames.size() + 1);
				}

				getChunkInfo(frameInfo, _videoFrames.size(), _videoChunks,
					_videoSampleToChunk, _curVideoChunkOffset, _connectedVideoSampleOffset,
					_numVideoSamples);

				if (_videoFrames.size())
				{
					GetVideoDecodeTime(frameInfo, false);
				}

				_videoFrames.push_back(frameInfo);
			}
			else
			{
				//还是原来那帧?
				_videoFrames.back().size += totalCopied;
			}
			//由于rtsp可能发来同一时间的帧，会导致帧数据出错，改变时间
			packet.timestamp++;
			//！由于rtsp可能发来同一时间的帧，会导致帧数据出错，改变时间
			_lastVideoTimestamp = packet.timestamp;
		}
	}
}

bool mp4FileStream::parseSps()
{
	bool bok = true;
	char *tmp_sps = 0;
	int web_sps_len = 0;
	CBitReader	bit_reader;
	do
	{
		tmp_sps = new char[_sps.size];
		memcpy(tmp_sps, _sps.data, _sps.size);
		web_sps_len = _sps.size;
		emulation_prevention((unsigned char*)tmp_sps, web_sps_len);
		bit_reader.SetBitReader((unsigned char*)tmp_sps, web_sps_len, true);
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
		_widht = Width;
		_height = Height;
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
				_fps = time_scale / (2 * num_units_in_tick);
			}
		}
	} while (0);
	if (tmp_sps)
	{
		delete[]tmp_sps;
		tmp_sps = 0;
	}

	return true;
}

//chunks
void mp4FileStream::getChunkInfo(MP4VideoFrameInfo & data, unsigned int index, std::vector<unsigned long long>& chunks, std::vector<SampleToChunk>& sampleToChunks, unsigned long long & curChunkOffset, unsigned long long & connectedSampleOffset, unsigned int & numSamples)
{
	unsigned long long curOffset = data.fileOffset;
	if (index == 0)
	{
		//第一帧，肯定没有对应的chunk
		curChunkOffset = curOffset;
	}
	else
	{
		//如果不连续，则加入新的chunk
		if (curOffset != connectedSampleOffset)
		{
			chunks.push_back(curChunkOffset);
			if (!sampleToChunks.size() || sampleToChunks.back().samplesPerChunk != numSamples)
			{
				SampleToChunk	stc;
				stc.firstChunkID = chunks.size();
				stc.samplesPerChunk = numSamples;
				sampleToChunks.push_back(stc);
			}
			curChunkOffset = curOffset;
			numSamples = 0;
		}
	}
	numSamples++;
	//加上这个data的大小
	connectedSampleOffset = curOffset + data.size;
}

inline void mp4FileStream::EndChunkInfo(std::vector<unsigned long long>& chunks,
	std::vector<SampleToChunk>& sampleToChunks, unsigned long long & curChunkOffset, unsigned int & numSamples)
{
	chunks.push_back(curChunkOffset);
	if (!sampleToChunks.size() || sampleToChunks.back().samplesPerChunk != numSamples)
	{
		SampleToChunk	stc;
		stc.firstChunkID = chunks.size();
		stc.samplesPerChunk = numSamples;
		sampleToChunks.push_back(stc);
	}
}

void mp4FileStream::GetVideoDecodeTime(MP4VideoFrameInfo & videoFrame, bool bLast)
{
	unsigned int frameTime;
	if (_videoFrames.size() <= 0)
	{
		frameTime = 0;
	}
	else
	{
		if (bLast)
		{
			if (_videoDecodeTimes.size()>0)
			{
				frameTime = _videoDecodeTimes.back().val;
			}
			else
			{
				frameTime = 0;
			}
		}
		else
		{
			frameTime = videoFrame.timestamp - _videoFrames.back().timestamp;
			if (videoFrame.timestamp<_videoFrames.back().timestamp)
			{
				frameTime = 0;
			}
		}
	}
	//for stts
	if (!_videoDecodeTimes.size() || _videoDecodeTimes.back().val != frameTime)
	{
		OffsetVal newVal;
		newVal.count = 1;
		newVal.val = frameTime;
		_videoDecodeTimes.push_back(newVal);
	}
	else
	{
		_videoDecodeTimes.back().count++;
	}

	//for ctts
	int compositionOffset;
	if (_videoFrames.size()>0)
	{
		compositionOffset = _videoFrames.back().compositionOffset;
	}
	else
	{
		compositionOffset = 0;
	}

	if (!_compositionOffsets.size() || _compositionOffsets.back().val != compositionOffset)
	{
		OffsetVal newVal;
		newVal.count;
		newVal.val = compositionOffset;
		_compositionOffsets.push_back(newVal);
	}
	else
	{
		_compositionOffsets.back().count++;
	}
}

void mp4FileStream::pushBox(unsigned int boxName)
{
	_boxOffsets.push_back(_endBuf.size());

	//预留长度
	push4Bytes(0);
	//写入名称
	push4Bytes(boxName);
}

void mp4FileStream::popBox()
{
	//修改长度
	unsigned int boxSize = _endBuf.size() - _boxOffsets.back();
	unsigned int cur = _boxOffsets.back();
	_endBuf.at(cur++) = ((boxSize >> 24) & 0xff);
	_endBuf.at(cur++) = ((boxSize >> 16) & 0xff);
	_endBuf.at(cur++) = ((boxSize >> 8) & 0xff);
	_endBuf.at(cur++) = ((boxSize >> 0) & 0xff);
	_boxOffsets.pop_back();
}

void mp4FileStream::push8Bytes(unsigned long long data)
{
	_endBuf.push_back((unsigned char)(data >> 56) & 0xff);
	_endBuf.push_back((unsigned char)(data >> 48) & 0xff);
	_endBuf.push_back((unsigned char)(data >> 40) & 0xff);
	_endBuf.push_back((unsigned char)(data >> 32) & 0xff);
	_endBuf.push_back((unsigned char)(data >> 24) & 0xff);
	_endBuf.push_back((unsigned char)(data >> 16) & 0xff);
	_endBuf.push_back((unsigned char)(data >> 8) & 0xff);
	_endBuf.push_back((unsigned char)(data) & 0xff);
}

void mp4FileStream::push4Bytes(unsigned long data)
{
	_endBuf.push_back((unsigned char)(data >> 24) & 0xff);
	_endBuf.push_back((unsigned char)(data >> 16) & 0xff);
	_endBuf.push_back((unsigned char)(data >> 8) & 0xff);
	_endBuf.push_back((unsigned char)(data) & 0xff);
}

void mp4FileStream::push2Byte(unsigned short data)
{
	_endBuf.push_back((unsigned char)(data >> 8) & 0xff);
	_endBuf.push_back((unsigned char)(data) & 0xff);
}

void mp4FileStream::pushByte(unsigned char data)
{
	_endBuf.push_back(data);
}

void mp4FileStream::pushByteArray(void * data, unsigned int size)
{
	for (unsigned i = 0; i < size; i++)
	{
		_endBuf.push_back(((unsigned char*)data)[i]);
	}
}

void mp4FileStream::flushBox()
{
	if (!_fp)
	{
		return;
	}
	fwrite(_endBuf.data(), _endBuf.size(), 1, _fp);
	_endBuf.clear();
	_boxOffsets.clear();
}

void mp4FileStream::InitFile()
{
	if (!_fp)
	{
		return;
	}

	pushBox('ftyp');
	push4Bytes('isom');
	push4Bytes(0x200);
	push4Bytes('isom');
	push4Bytes('iso2');
	push4Bytes('avc1');
	push4Bytes('mp41');

	popBox();
	flushBox();


	unsigned int freeSize = htonl(8);
	fwrite(&freeSize, 4, 1, _fp);
	fwrite("free", 4, 1, _fp);
	//写入mdat头


	//mdat的起始位置，用来更新mdat的大小
	_mdatStart = fileTell(_fp);
	//64bit mp4? 在mdat后8个字节的大小，否则mdat前输入大小
	unsigned int mdatSize = htonl(0x01);
	fwrite(&mdatSize, 4, 1, _fp);
	fwrite("mdat", 4, 1, _fp);


	long long MP4_64bit = 0;
	fwrite(&MP4_64bit, 8, 1, _fp);

	_bStreamOpened = true;
}

void mp4FileStream::ShutdownFile()
{
	if (!_bStreamOpened)
	{
		return;
	}

	//修改大小
	_mdatStop = fileTell(_fp);

	unsigned long long mdatSize = htonll(_mdatStop - _mdatStart);
	fileSeek(_fp, _mdatStart + 8, SEEK_SET);
	fwrite(&mdatSize, 8, 1, _fp);
	fileSeek(_fp, 0, SEEK_END);
	//!修改大小

	//1970 和1904秒数差
	//unsigned int macTime = htonl(time(0)+2082844800);
	//unsigned int videoDuration = htonl(_lastVideoTimestamp);
	unsigned int macTime = time(0) + 2082844800;
	unsigned int videoDuration = _lastVideoTimestamp;

	std::string lpVideoTrack = "Video Media Handler";
	std::string lpAudioTrack = "Audio Media Handler";

	const char videoCompressionName[31] = "AVC Coding";

	EndChunkInfo(_videoChunks, _videoSampleToChunk, _curVideoChunkOffset, _numVideoSamples);
	if (_numVideoSamples>1)
	{
		GetVideoDecodeTime(_videoFrames.back(), true);
	}

	//moov
	pushBox('moov');
	//mvhd
	pushBox('mvhd');
	//整体信息
	push4Bytes(0);//version flag
	push4Bytes(macTime);//creation time
	push4Bytes(macTime);//modification time
	push4Bytes(1000);//timescale
	push4Bytes(videoDuration);//duration
	push4Bytes(0x00010000);//rate
	push2Byte(0x0100);//volume
					  //reserved
	push2Byte(0);
	//reserved
	push4Bytes(0);
	push4Bytes(0);
	//matrix
	push4Bytes(0x00010000); push4Bytes(0x00000000); push4Bytes(0x00000000); //window matrix row 1 (1.0, 0.0, 0.0)
	push4Bytes(0x00000000); push4Bytes(0x00010000); push4Bytes(0x00000000); //window matrix row 2 (0.0, 1.0, 0.0)
	push4Bytes(0x00000000); push4Bytes(0x00000000); push4Bytes(0x40000000); //window matrix row 3 (0.0, 0.0, 16384.0)
																			//pre defined
	push4Bytes(0);
	push4Bytes(0);
	push4Bytes(0);
	push4Bytes(0);
	push4Bytes(0);
	push4Bytes(0);
	//next track id,这个ID是1为起点，并且这个值要大于track id。比如有一个track，这个值为2,。两个则为3
	push4Bytes(2);
	//!mvhd
	popBox();
	//audio track
	//!audio track
	//video track
	pushBox('trak');
	//tkhd trak的基本描述
	pushBox('tkhd');
	push4Bytes(0x00000007); //version (0) and flags (0x7)
	push4Bytes(macTime); //creation time
	push4Bytes(macTime); //modified time
	push4Bytes(1); //track ID
	push4Bytes(0); //reserved
	push4Bytes(videoDuration); //duration (in time base units)
	push8Bytes(0); //reserved
	push2Byte(0); //video layer (0)
	push2Byte(0); //quicktime alternate track id (0)
	push2Byte(0); //track audio volume (this is video, so 0)
	push2Byte(0); //reserved
	push4Bytes(0x00010000); push4Bytes(0x00000000); push4Bytes(0x00000000); //window matrix row 1 (1.0, 0.0, 0.0)
	push4Bytes(0x00000000); push4Bytes(0x00010000); push4Bytes(0x00000000); //window matrix row 2 (0.0, 1.0, 0.0)
	push4Bytes(0x00000000); push4Bytes(0x00000000); push4Bytes(0x40000000); //window matrix row 3 (0.0, 0.0, 16384.0)
	push4Bytes((_widht << 16));  //width (fixed point)
	push4Bytes((_height << 16)); //height (fixed point)
								 //!tkhd
	popBox();
	//media
	pushBox('mdia');
	//mdhd
	pushBox('mdhd');
	push4Bytes(0);//version and flage
	push4Bytes(macTime);//creation time
	push4Bytes(macTime);//modification time
	push4Bytes(1000);//timescale
	push4Bytes(videoDuration);//duration
	push4Bytes(0x55c40000);//language
						   //!mdhd
	popBox();
	//hdlr
	pushBox('hdlr');
	push4Bytes(0);//version and flags
	push4Bytes(0);//pre defined
	push4Bytes('vide');//handler type
	push4Bytes(0); push4Bytes(0); push4Bytes(0);//reserved
												//name
	pushByteArray((void*)lpVideoTrack.data(), lpVideoTrack.size());
	pushByte('\0');
	//!hdlr
	popBox();
	//minf
	pushBox('minf');
	//vmhd
	pushBox('vmhd');
	push4Bytes(1);//version 0 flags 1
	push2Byte(0);
	push2Byte(0);
	push2Byte(0);
	push2Byte(0);
	//!vmhd
	popBox();
	//dinf
	pushBox('dinf');
	//dref
	pushBox('dref');
	push4Bytes(0);
	push4Bytes(1);
	//url
	pushBox('url ');
	push4Bytes(1);
	//!url
	popBox();
	//!dref
	popBox();
	//!dinf
	popBox();
	//stbl
	pushBox('stbl');
	//stsd
	pushBox('stsd');
	push4Bytes(0);
	push4Bytes(1);
	//avc1
	pushBox('avc1');
	push4Bytes(0);//reserved
	push2Byte(0);
	push2Byte(1);//index
	push2Byte(0);//encoding version
	push2Byte(0);; //encoding revision level
	push4Bytes(0); //encoding vendor
	push4Bytes(0); //temporal quality
	push4Bytes(0); //spatial quality
	push2Byte(_widht);; //width
	push2Byte(_height); //height
	push4Bytes(0x00480000); //fixed point width pixel resolution (72.0)
	push4Bytes(0x00480000); //fixed point height pixel resolution (72.0)
	push4Bytes(0); //quicktime video data size 
	push2Byte(1); //frame count(?)
	pushByte((unsigned char)strlen(videoCompressionName)); //compression name length
	pushByteArray((void*)videoCompressionName, 31); //31 bytes for the name
	push2Byte(24); //bit depth
	push2Byte(0xFFFF); //quicktime video color table id (none = -1)
					   //avcc
	pushBox('avcC');
	pushByte(1);//version
	pushByte(100);//profile id
	pushByte(0);
	pushByte(0x1f);
	pushByte(0xff);
	pushByte(0xe1);
	push2Byte(_sps.size);
	pushByteArray((void*)_sps.data, _sps.size);
	pushByte(1);
	push2Byte(_pps.size);
	pushByteArray((void*)_pps.data, _pps.size);
	//!avcc
	popBox();
	//!avc1
	popBox();
	//!stsd
	popBox();
	//stts
	pushBox('stts');
	push4Bytes(0);//flag and version
	push4Bytes(_videoDecodeTimes.size());//entry count
	for (auto i : _videoDecodeTimes)
	{
		//sample count
		//sample delta
		push4Bytes(i.count);
		push4Bytes(i.val);
	}
	//!stts
	popBox();
	if (_IFrameIDs.size())
	{
		pushBox('stts');
		push4Bytes(0);
		push4Bytes(_IFrameIDs.size());
		pushByteArray((void*)_IFrameIDs.data(), _IFrameIDs.size() * 4);
		popBox();
	}
	//ctts
	pushBox('ctts');
	push4Bytes(0);
	push4Bytes(_compositionOffsets.size());
	for (auto i : _compositionOffsets)
	{
		push4Bytes(i.count);
		push4Bytes(i.val);
	}
	//!ctts
	popBox();
	//stsc
	pushBox('stsc');
	push4Bytes(0);
	push4Bytes(_videoSampleToChunk.size());
	for (auto i : _videoSampleToChunk)
	{
		push4Bytes(i.firstChunkID);
		push4Bytes(i.samplesPerChunk);
		push4Bytes(1);
	}
	//!stsc
	popBox();
	//stsz
	pushBox('stsz');
	push4Bytes(0);
	push4Bytes(0);
	push4Bytes(_videoFrames.size());
	for (auto i : _videoFrames)
	{
		push4Bytes(i.size);
	}
	//!stsz
	popBox();
	//stco
	if (_videoChunks.size())
	{
		pushBox('co64');
		push4Bytes(0);
		push4Bytes(_videoChunks.size());
		for (auto i : _videoChunks)
		{
			push8Bytes(i);
		}
		popBox();
	}
	else
	{
		pushBox('stco');
		push4Bytes(0);
		push4Bytes(_videoChunks.size());
		for (auto i : _videoChunks)
		{
			push4Bytes(i);
		}
		popBox();
	}
	//!stco
	//!stbl
	popBox();
	//!minf
	popBox();
	//!media
	popBox();
	//!video track
	popBox();
	//!moov
	popBox();
	flushBox();
}


void	mp4FileStream::emulation_prevention(unsigned char *pData, int &len)
{
	int templen = len;
	for (int i = 0; i < templen - 2; i++)
	{
		int ret = (pData[i] ^ 0x00) + (pData[i + 1] ^ 0x00) + (pData[i + 2] ^ 0x03);
		if (0 == ret)
		{
			for (int j = i + 2; j < templen - 1; j++)
			{
				pData[j] = pData[j + 1];
			}
			len--;
		}
	}
}
