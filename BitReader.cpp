#include "BitReader.h"
#include "socketEvent.h"


CBitReader::CBitReader(void):m_buf(0),m_curBit(0),m_TotalByte(0),m_ref(false)
{
}


CBitReader::~CBitReader(void)
{
	if (!m_ref)
	{
		deletePointerArray(m_buf);
	}
}

int	CBitReader::SetBitReader(unsigned char* Buf,int len,bool useRef)
{
	if (!Buf)
	{
		return	-1;
	}
	if (!m_ref)
	{
		deletePointerArray(m_buf);
	}
	m_ref = useRef;
	if (m_ref)
	{
		m_curBit = 0;
		m_TotalByte = len;
		m_buf = Buf;
	}
	else
	{
		m_curBit = 0;
		m_TotalByte = len;
		m_buf = new unsigned char[len];
		if (!m_buf)
		{
			printf("out of memory\n");
			return	-1;
		}
		memcpy(m_buf, Buf, len);
	}
	
	
	return	0;
}

unsigned int CBitReader::ReadBit()
{
	if (m_curBit>(m_TotalByte<<3))
	{
		printf("Error: out of range in ReadBit");
		return	-1;
	}
	int nIndex=(m_curBit>>3);
	int nOffset=m_curBit%8+1;
	m_curBit++;

	return	(m_buf[nIndex]>>(8-nOffset))&0x01;
}


unsigned int CBitReader::ReadBits(int nBits)
{
	unsigned int r = 0;
	int i;
	for (i = 0; i < nBits; i++)
	{
		r |= (ReadBit() << (nBits - i - 1));
	}
	return r;
}

unsigned int CBitReader::Read32Bits()
{
	unsigned int r=0;
	int nIndex=(m_curBit>>3);
#if (BYTE_ORDER==LITTLE_ENDIAN)
	r=((m_buf[nIndex]<<24)|(m_buf[nIndex+1]<<16|(m_buf[nIndex+2]<<8))|(m_buf[nIndex+3]));
#else
	r=((m_buf[nIndex+3]<<24)|(m_buf[nIndex+2]<<16|(m_buf[nIndex+1]<<8))|(m_buf[nIndex]));
#endif
	m_curBit+=32;
	return r;
}

int	CBitReader::CurrentBitCount()
{
	return	m_curBit;
}

int	CBitReader::CurrentByteCount()
{
	return	(m_curBit>>3);
}

unsigned char* CBitReader::CurrentByteData()
{
	return	&m_buf[(m_curBit>>3)];
}

void	CBitReader::IgnoreBytes(int nBytesIgnore)
{
	m_curBit+=(nBytesIgnore<<3);
}

unsigned int CBitReader::ReadExponentialGolombCode()
{
	int r = 0;
	int i = 0;

	while ((ReadBit() == 0) && (i < 32))
	{
		i++;
	}

	r = ReadBits(i);
	r += (1 << i) - 1;
	return r;
}

unsigned int CBitReader::ReadSE()
{
	int r = ReadExponentialGolombCode();
	if (r & 0x01)
	{
		r = (r + 1) / 2;
	}
	else
	{
		r = -(r / 2);
	}
	return r;
}
