#pragma once
#include "RTP.h"
#include "fileHander.h"
#include "flvIdx.h"
#include <string>
//接收h264裸流和时间，一秒一写，而不是一帧一写
//非线程安全
class flvWriter
{
public:
	flvWriter();
	virtual ~flvWriter();
	bool initFile(std::string fileName);
	bool addTimedPacket(timedPacket &packet);
private:
	enum flv_h264_nal_type
	{
		flv_h264_nal_slice = 1,
		flv_h264_nal_slice_dpa = 2,
		flv_h264_nal_slice_dpb = 3,
		flv_h264_nal_slice_dpc = 4,
		flv_h264_nal_slice_idr = 5,
		flv_h264_nal_sei = 6,
		flv_h264_nal_sps = 7,
		flv_h264_nal_pps = 8,
		flv_h264_nal_aud = 9,
		flv_h264_nal_filter = 12,
	};
	bool generateMetaData();
	bool processPacket(timedPacket &packet);
	bool writePacket(timedPacket &packet);
	bool parseSps();
	void updateDuration(double timeDuration);
	void updateFileSize();
	volatile bool m_end;
	flvWriter(flvWriter&) = delete;
	flvWriter&operator=(const flvWriter&) = delete;
	bool m_inited;
	fileHander *m_fileHandler;
	DataPacket m_sps;
	DataPacket m_pps;
	DataPacket m_metaData;
	std::mutex m_mutexSpsPps;
	unsigned long m_firstTimeStamp;

	bool m_firstKeyframeWrited;


	//metadata
	double m_fileSize;
	double m_fps;
	double m_width;
	double m_height;
	double m_duration;
	long long m_fileSizePos;
	long long m_durationPos;
	long long m_fpsPos;
	std::string m_encoder;

	std::string	m_fileName;
	//idx
	flvIdxWriter *m_idxWriter;
	bool	m_hasIdx;
};

