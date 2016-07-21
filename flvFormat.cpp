#include "flvFormat.h"
#include "socket_type.h"


flvHeader::flvHeader()
{
}


flvHeader::~flvHeader()
{
}


flvTag::flvTag()
{
}

flvTag::~flvTag()
{
}


audioData::audioData()
{
}

audioData::~audioData()
{
}


aacAudioData::aacAudioData()
{
}

aacAudioData::~aacAudioData()
{
}

videoData::videoData()
{
}

videoData::~videoData()
{
}
	

avcVideoPacket::avcVideoPacket()
{
}

avcVideoPacket::~avcVideoPacket()
{
}


scriptDataString::scriptDataString()
{
}

scriptDataString::~scriptDataString()
{
}


scirptDataLongString::scirptDataLongString()
{
}

scirptDataLongString::~scirptDataLongString()
{
}

scriptDataValue::scriptDataValue()
{
}

scriptDataValue::~scriptDataValue()
{
}

scriptDataObject::scriptDataObject()
{
}

scriptDataObject::~scriptDataObject()
{
}

scriptData::scriptData()
{
}

scriptData::~scriptData()
{
}


scriptDataObjectEnd::scriptDataObjectEnd()
{
}

scriptDataObjectEnd::~scriptDataObjectEnd()
{
}

scriptDataVariable::scriptDataVariable()
{
}

scriptDataVariable::~scriptDataVariable()
{
}


scriptDataVariableEnd::scriptDataVariableEnd()
{
}

scriptDataVariableEnd::~scriptDataVariableEnd()
{
}


scriptDataDate::scriptDataDate()
{
}

scriptDataDate::~scriptDataDate()
{
}

unsigned long long fastHtonll(unsigned long long ull)
{
#if (BYTE_ORDER==LITTLE_ENDIAN)
	return (((ull >> 56) & 0xFF) | (((ull >> 48) & 0xFF) << 8) | (((ull >> 40) & 0xFF) << 16) | (((ull >> 32) & 0xFF) << 24) | \
		(((ull >> 24) & 0xFF) << 32) | (((ull >> 16) & 0xFF) << 40) | (((ull >> 8) & 0xFF) << 48) | ((ull & 0xFF) << 56));
#else
	return ull;
#endif
}

unsigned int fastHton3(unsigned int i3)
{
#if (BYTE_ORDER==LITTLE_ENDIAN)
	int a = ((i3 & 0xff0000) >> 16);
	int b = ((i3 & 0xff) << 16);
	int c = ((i3 & 0xff00));
	return (((i3 & 0xff0000) >> 16) | ((i3 & 0xff) << 16)|((i3&0xff00)));
#else
	return i3;
#endif
}

