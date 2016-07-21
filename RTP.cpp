#include "RTP.h"

DataPacket::DataPacket():size(0),data(0)
{}

DataPacket::~DataPacket()
{
	if (data)
	{
		//delete[]data;
		//data = 0;
	}
}