#pragma once
#include "RTP.h"
#include <vector>
#include <string>

struct SampleToChunk
{
	unsigned int firstChunkID;
	unsigned int samplesPerChunk;
};

struct OffsetVal
{
	unsigned int count;
	unsigned int val;
};

struct MP4VideoFrameInfo
{
	unsigned long long fileOffset;
	unsigned int size;
	unsigned int timestamp;
	int	compositionOffset;
};

struct MP4AudioFrameInfo
{
	unsigned long long fileOffset;
	unsigned int size;
	unsigned int timestamp;
};

class mp4FileStream
{
public:
	mp4FileStream(std::string fileName);
	virtual ~mp4FileStream();
	virtual void addTimedDataPacket(timedPacket &dataPacket);
	static unsigned int fourCC2NL(std::string stringConv);
	
private:
	void addPacket(timedPacket &packet);
	mp4FileStream(const mp4FileStream&) = delete;
	mp4FileStream operator= (const mp4FileStream&) = delete;
	void emulation_prevention(unsigned char *pData, int &len);
	bool parseSps();

	void getChunkInfo(MP4VideoFrameInfo &data, unsigned int index,
		std::vector<unsigned long long> &chunks, std::vector<SampleToChunk>	&sampleToChunks,
		unsigned long long &curChunkOffset, unsigned long long &connectedSampleOffset,
		unsigned int &numSamples);
	inline void EndChunkInfo(std::vector<unsigned long long> &chunks,
		std::vector<SampleToChunk> &sampleToChunks, unsigned long long &curChunkOffset, unsigned int &numSamples);


	void GetVideoDecodeTime(MP4VideoFrameInfo &videoFrame, bool bLast);
	DataPacket _sps;
	DataPacket _pps;
	int _fps;
	int _widht;
	int _height;
	
	FILE *_fp;

	//用来保存mp4盒子数据，保存完后一次性写入
	std::vector<unsigned char>	_endBuf;
	//记录盒子的起始位置
	std::vector<unsigned int>	_boxOffsets;
	void pushBox(unsigned int boxName);
	void popBox();
	void push8Bytes(unsigned long long data);
	void push4Bytes(unsigned long data);
	void push2Byte(unsigned short data);
	void pushByte(unsigned char data);
	void pushByteArray(void *data, unsigned int size);
	void flushBox();
	//初始化，写入ftype
	void InitFile();
	//最后关闭文件，合并moov和mdat
	void ShutdownFile();
	//mdat
	unsigned int _firstPktTime;
	bool _getIdr;
	//moov
	std::vector<MP4VideoFrameInfo>	_videoFrames;
	std::vector<MP4AudioFrameInfo>	_audioFrames;

	std::vector<unsigned int>	_IFrameIDs;

	unsigned int _lastVideoTimestamp;

	bool	_bStreamOpened;
	bool	_bMP3;

	unsigned long long _connectedAudioSampleOffset;
	unsigned long long _connectedVideoSampleOffset;

	unsigned long long _curAudioChunkOffset;
	unsigned long long _curVideoChunkOffset;

	unsigned int _numVideoSamples;
	unsigned int _numAudioSamples;

	std::vector<unsigned long long> _videoChunks;
	std::vector<unsigned long long> _audioChunks;
	std::vector<SampleToChunk>	_videoSampleToChunk;
	std::vector<SampleToChunk>	_audioSampleToChunk;

	unsigned long long _lastAudioTimeVal;
	unsigned long long _audioFrameSize;

	std::vector<OffsetVal>	_videoDecodeTimes;
	std::vector<OffsetVal>	_audioDecodeTimes;

	std::vector<OffsetVal>	_compositionOffsets;

	unsigned long long _mdatStart;
	unsigned long long _mdatStop;

	bool	_bCancelMP4Build;
	//!moov
};

