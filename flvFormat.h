#pragma once
#include <string>


const unsigned char FLV_F = 'F';
const unsigned char FLV_L = 'L';
const unsigned char FLV_V = 'V';
const unsigned char FLV_Tag_Audio = 8;
const unsigned char FLV_Tag_Video = 9;
const unsigned char FLV_Tag_ScriptData = 18;

class scriptDataString;
class scriptDataValue;
class scirptDataLongString;
class scriptDataVlaue;
class scriptDataVariable;
class scriptDataVariableEnd;
class scriptDataDate;
class scriptDataObject;
class scriptDataObjectEnd;

class flvHeader
{
public:
	flvHeader();
	virtual ~flvHeader();
public:
	//header
	unsigned char F;			//Signature byte always 'F' (0x46)	:8
	unsigned char L;			//Signature byte always 'L' (0x4C)	:8
	unsigned char V;			//Signature byte always 'V' (0x56)	:8
	unsigned char version;	//File version (for example, 0x01 for FLVversion 1):8

	unsigned char typeFlagsReservedAudio;	//Must be 0	:5
	unsigned char typeFlagsAudio;			//Audio tags are present	:1
	unsigned char typeFlagsReservedVideo;	//Must be 0	:1
	unsigned char typeFlagsVideo;			//Video tags are present:1
	unsigned int  dataOffset;				//Offset in bytes from start of file to start of body(that is, size of header):32
	/*The DataOffset field usually has a value of 9 for FLV version 1. This field is present to
		accommodate larger headers in future versions.从开始到头结束，flv为9:32*/

};

class flvTag
{
public:
	flvTag();
	~flvTag();

public:
	unsigned char tagType;	//Type of this tag. Values are:
							//8: audio
							//9 : video
							//18 : script data
							//all others : reserved*/:8
	unsigned int dataSize;	//Length of the data in the Data field:24
	unsigned int timestamp;	//Time in milliseconds at which the
							//data in this tag applies.This value is
							//relative to the first tag in the FLV
							//file, which always has a timestamp
							//of 0:24
	unsigned char timestampExtended;//时间戳的高八位:8

	unsigned int timestampFull;//32位时间戳

	unsigned int streamID;	//always 0:24
	unsigned char *data;	//body of tag;
							/*	If TagType == 8
								AUDIODATA
								If TagType == 9
								VIDEODATA
								If TagType == 18
								SCRIPTDATAOBJECT*/
};

class audioData
{
public:
	audioData();
	~audioData();

public:
	unsigned char soundFormat;//:4
		/*0 = Linear PCM, platform endian
		1 = ADPCM
		2 = MP3
		3 = Linear PCM, little endian
		4 = Nellymoser 16 - kHz mono
		5 = Nellymoser 8 - kHz mono
		6 = Nellymoser
		7 = G.711 A - law logarithmic PCM
		8 = G.711 mu - law logarithmic PCM
		9 = reserved
		10 = AAC
		11 = Speex
		14 = MP3 8 - Khz
		15 = Device - specific sound*/
	unsigned char soundRate;//aac always 3:2
		/*0 = 5.5 - kHz
		1 = 11 - kHz
		2 = 22 - kHz
		3 = 44 - kHz*/
	unsigned char soundSize;//:1
		/*0 = snd8Bit
		1 = snd16Bit*/
	unsigned char soundType;//Mono or stereo soundFor Nellymoser : always 0 For AAC : always 1 :1
		/*0 = sndMono
		1 = sndStereo*/
	unsigned char* soundData;//如果是AAC则AACAUDIODATA，否则音频自身格式数据
	static const int s_soundFormat_linear_PCM_platform_endian = 0;
	static const int s_soundFormat_ADPCM = 1;
	static const int s_soundFormat_MP3 = 2;
	static const int s_soundFormat_linear_PCM_little_endian = 3;
	static const int s_soundFormat_Nellymoser_16kHz_mono = 4;
	static const int s_soundFormat_Nellymoser_8kHz_mono = 5;
	static const int s_soundFormat_Nellymoser = 6;
	static const int s_soundFormat_G711_A_law_logarithmic_PCMM = 7;
	static const int s_soundFormat_G711_mu_law_logarithmic_PCM = 8;
	static const int s_soundFormat_reserved = 9;
	static const int s_soundFormat_AAC = 10;
	static const int s_soundFormat_Speex = 11;
	static const int s_soundFormat_MP3_8Khz = 14;
	static const int s_soundFormat_Device_specific_sound = 14;
};
//aac的负载数据
class aacAudioData
{
public:
	aacAudioData();
	~aacAudioData();

public:
	unsigned char aacPacketType;//0: AAC sequence header 1: AAC raw:8
	unsigned char *data;
};

class videoData
{
public:
	videoData();
	~videoData();

public:
	unsigned char frameType;//1:keyframe
							//2:inter frame 
							//3:disposable inter frame
							//4:generated keyframe (reserved for server use only)
							//5:video info/command frame
							//:4
	unsigned char codecID;	//1:jpeg;2: Sorenson H.263 3: Screen video
							//4 : On2 VP6 5 : On2 VP6 with alpha channel
							//6 : Screen video version 2 7 : AVC
							//:4
	unsigned char *videoDatas;

};
//videoData的负载数据
class avcVideoPacket
{
public:
	avcVideoPacket();
	~avcVideoPacket();

public:
	unsigned char avcPacketType;//AVC sequence header
								//AVC NALU
								//AVC end of sequence (lower level NALU sequence ender is not required or supported)
								//:8
	signed int compositionTime; //if AVCPacketType == 1
								//Composition time offset
								//else 0
								//:24
	unsigned char *data;		/*if AVCPacketType == 0
								AVCDecoderConfigurationRecord
								else if AVCPacketType == 1
								One or more NALUs(can be individual
									slices per FLV packets; that is, full frames
									are not strictly required)
								else if AVCPacketType == 2
								Empty*/
};

//data tags 既不是视频也不是音频的数据 


class scriptDataVariableEnd
{
public:
	scriptDataVariableEnd();
	~scriptDataVariableEnd();

public:
	unsigned int variableEndMarker1;//always 9:24 
};

class scriptDataObjectEnd
{
public:
	scriptDataObjectEnd();
	~scriptDataObjectEnd();

private:
	int m_value = 9;	//	:24
						/*Local time offset in minutes from UTC.For
						time zones located west of Greenwich, UK,
						this value is a negative number.Time zones
						east of Greenwich, UK, are positive
						本地时间相对于UTC的偏移量
						*/
};


class scriptDataValue
{
public:
	scriptDataValue();
	~scriptDataValue();

public:
	unsigned char type;//:8
					   /*0 = Number type doule
					   1 = Boolean type
					   2 = String type
					   3 = Object type
					   4 = MovieClip type
					   5 = Null type
					   6 = Undefined type
					   7 = Reference type
					   8 = ECMA array type
					   10 = Strict array type
					   11 = Date type
					   12 = Long string type*/
	unsigned int ecmaArrayLength;//if type=8,UI32
								 /*If Type == 0
								 DOUBLE
								 If Type == 1
								 UI8
								 If Type == 2
								 SCRIPTDATASTRING
								 If Type == 3
								 SCRIPTDATAOBJECT[n]
								 If Type == 4
								 SCRIPTDATASTRING
								 defining
								 the MovieClip path
								 If Type == 7
								 UI16
								 If Type == 8
								 SCRIPTDATAVARIABLE[EC
								 MAArrayLength]
								 If Type == 10
								 SCRIPTDATAVARIABLE[n]
								 If Type == 11
								 SCRIPTDATADATE
								 If Type == 12
								 SCRIPTDATALONGSTRING*/
	double	valueDouble;
	unsigned char valueUI8;
	unsigned short valueUI16;
	/*scriptDataString valueDataString;
	scriptDataObject valueDataObject;
	scriptDataVariable valueDataVariable;
	scriptDataDate	valueDataDate;
	scirptDataLongString valueDataLongString;*/
	void *valueTypeOthers;

	scriptDataObjectEnd scriptDataValueTerminator3;
	scriptDataVariableEnd scriptDataValueTerminator8;
};

class scriptDataString
{
public:
	scriptDataString();
	~scriptDataString();

public:
	unsigned short stringLength;
	std::string		stringData;
};


class scirptDataLongString
{
public:
	scirptDataLongString();
	~scirptDataLongString();

public:
	unsigned int stringLength;
	std::string		stringData;
};

class scriptDataVariable
{
public:
	scriptDataVariable();
	~scriptDataVariable();

public:
	scriptDataString	variableName;
	scriptDataValue		variableData;
};



class scriptDataDate
{
public:
	scriptDataDate();
	~scriptDataDate();

public:
	double	dateTime;//1970 UTC起的毫秒
	short localDateTimeOffset;	//:16
								//
};







class scriptDataObject
{
public:
	scriptDataObject();
	~scriptDataObject();

public:
	scriptDataString objectName;
	scriptDataValue  objectData;
};


class scriptData
{
public:
	scriptData();
	~scriptData();

public:
	scriptDataObject *objects;
	unsigned int end;//:24
};


unsigned long long fastHtonll(unsigned long long ull);
unsigned int fastHton3(unsigned int);

struct spsppsRecord
{
	unsigned short setlength;
	unsigned char *data;
};

struct AVCDecoderConfigurationRecord
{
	unsigned char configurationVersion;//1
	unsigned char AVCProfileIndication;//sps 1
	unsigned char profile_compatibility;//sps 2
	unsigned char AVCLevelIndication;//sps 3
	unsigned char reserved6;
	unsigned char lengthSizeMinusOne2;
	unsigned char reserved3;
	unsigned char numOfSequenceParameterSets5;
	unsigned char numOfPictureParameterSets;
};
