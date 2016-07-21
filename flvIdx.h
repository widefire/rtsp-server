#pragma once
#include <string>
#include "fileHander.h"
//以文件开头为偏移量起始位置
//不一定每个关键帧前都有sps pps 以关键帧位置为点
//64位double秒为单位,64位long long为偏移量
//网络字节序

static const std::string idxStuff = ".Idx";

struct flvIdx
{
	double time;
	unsigned long long offset;
};

class flvIdxWriter
{
public:
	flvIdxWriter();
	virtual ~flvIdxWriter();
	bool initWriter(std::string fileName);
	bool appendIdx(flvIdx &idx);
private:
	fileHander *m_fileHander;
	bool m_inited;
};

class flvIdxReader
{
public:
	flvIdxReader();
	~flvIdxReader();
	bool initReader(std::string fileName);
	bool getNearestIdx(double time, flvIdx &idx);
private:
	void resetCur();
	fileHander *m_fileHander;
	bool m_inited;
};

