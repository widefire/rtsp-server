#include "flvIdx.h"
#include "flvFormat.h"
#include "configData.h"


flvIdxWriter::flvIdxWriter():m_fileHander(nullptr),m_inited(false)
{
}


flvIdxWriter::~flvIdxWriter()
{
	if (m_fileHander)
	{
		delete m_fileHander;
		m_fileHander = nullptr;
	}
}

bool flvIdxWriter::initWriter(std::string fileName)
{
	if (m_fileHander)
	{
		delete m_fileHander;
		m_fileHander = nullptr;
	}
	m_fileHander = new fileHander(fileName, open_writeBinary);
	m_inited = m_fileHander->openSuccessed();
	return m_inited;
}

bool flvIdxWriter::appendIdx(flvIdx & idx)
{
	if (!m_inited)
	{
		return false;
	}
	unsigned long long tmp64 = *reinterpret_cast<unsigned long long*>(&idx.time);
	//tmp64 = fastHtonll(tmp64);
	m_fileHander->writeFile(8, (char*)&tmp64);
	//tmp64 = fastHtonll(idx.offset);
	tmp64 = idx.offset;
	m_fileHander->writeFile(8, (char*)&tmp64);
	return true;
}


flvIdxReader::flvIdxReader():m_fileHander(nullptr),m_inited(false)
{
}

flvIdxReader::~flvIdxReader()
{
	if (m_fileHander)
	{
		delete m_fileHander;
		m_fileHander = nullptr;
	}
}

bool flvIdxReader::initReader(std::string fileName)
{
	if (m_fileHander)
	{
		delete m_fileHander;
		m_fileHander = nullptr;
	}
	m_fileHander = new fileHander(fileName, open_readBinary);
	m_inited = m_fileHander->openSuccessed();
	return m_inited;
}

bool flvIdxReader::getNearestIdx(double time, flvIdx & idx)
{
	if (!m_inited)
	{
		return false;
	}
	bool result = true;
	resetCur();
	do
	{
		double tmpDouble;
		unsigned long long tmp64;
		int redRet = 0;
		flvIdx lastIdx;
		lastIdx.time = 0.0;
		lastIdx.offset = 0;

		while (true)
		{
			redRet = m_fileHander->readFile(8, (char*)&tmp64);
			if (redRet!=8)
			{
				LOG_ERROR(configData::getInstance().get_loggerId(), "read file failed");
				result = false;
				break;
			}
			//tmp64 = fastHtonll(tmp64);
			tmpDouble = *reinterpret_cast<double*>(&tmp64);

			redRet = m_fileHander->readFile(8, (char*)&tmp64);
			if (redRet != 8)
			{
				LOG_ERROR(configData::getInstance().get_loggerId(), "read file failed");
				result = false;
				break;
			}

			//tmp64 = fastHtonll(tmp64);
			if (lastIdx.offset==0)
			{
				lastIdx.time = tmpDouble;
				lastIdx.offset = tmp64;
			}
			if (tmpDouble>=time)
			{
				idx.time = lastIdx.time;
				idx.offset = lastIdx.offset;
				break;
			}

			lastIdx.time = tmpDouble;
			lastIdx.offset = tmp64;
		}
	} while (0);
	return result;
}

void flvIdxReader::resetCur()
{
	if (m_inited)
	{
		m_fileHander->seek(0, SEEK_SET);
	}
}
