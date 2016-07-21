#pragma once
#include "flvFormat.h"
#include "fileHander.h"
#include "RTP.h"


class flvParser
{
public:
	flvParser(std::string fileName);
	virtual ~flvParser();
	timedPacket getNextFrame();
	timedPacket getNextFrame(flvPacketType type);
	double duration();
	bool inited();
	bool seekToPos(long long pos);
private:
	flvParser(const flvParser&) = delete;
	flvParser&operator=(const flvParser&) = delete;

	//Ω‚ŒˆÕ∑∫Õmetadata
	void parseFlvHeader();
	bool readTag(flvTag &tagOut);
	bool parseScriptData(flvTag &scriptTag);
	bool parserAudioData(flvTag &audioTag, timedPacket &outPkt);
	bool parserVideoData(flvTag &videoTag, timedPacket &outPkt);
	fileHander	*m_fileHander;
	flvHeader	m_flvHeader;
	double m_duration;
	bool m_inited;
};

