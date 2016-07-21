#include "localFileReader.h"
#include "log4z.h"

localFileReader::localFileReader(const char *file_name,int cache_size/* =CACHE_SIZE_DEFAULT */):
m_file_name(0),m_cache_size(cache_size),m_file_size(0),m_file_cache(0),m_fp(0),m_cur(0),
	m_cur_cache_start_pos(0),m_valid_cache_size(0)
{
	if (initFileReader(file_name)!=0)
	{
		printf("init file: %s failed;",file_name);
	}
}

localFileReader::~localFileReader()
{
	if (m_fp)
	{
		fclose(m_fp);
		LOGT("fclose" << m_fp);
		m_fp=0;
	}
	if (m_file_name)
	{
		delete []m_file_name;
		m_file_name=0;
	}
	if (m_file_cache)
	{
		delete []m_file_cache;
		m_file_cache=0;
	}
}

long localFileReader::fileSize()
{
	return	m_file_size;
}

long localFileReader::fileCur()
{
	return	m_cur_cache_start_pos+m_cur;
}

int localFileReader::readFile(char *out_buf,int  &read_len,bool remove)
{
	int iRet=0;
	do
	{
		//如果read_len大于cache size，不读;
		if (read_len>m_cache_size)
		{
			iRet=-2;
			break;
		}
		//如果read_len大于整个文件,读有效数据;
		//如果read_len在buffer范围外，重定位;
		//重新读一次文件大小;
		if (read_len+m_cur>m_valid_cache_size)
		{
			seekToPos(m_cur+m_cur_cache_start_pos);
		}
		if (m_valid_cache_size>=read_len)
		{
			memcpy(out_buf,m_file_cache+m_cur,read_len);
			/*int value = out_buf[0];
			printf("%d %02x\n", m_cur_cache_start_pos + m_cur, value);*/
			if (remove)
			{
				m_cur += read_len;
			}
			break;
		}else
		{
			memcpy(out_buf,m_file_cache+m_cur,m_valid_cache_size);
			read_len=m_valid_cache_size;
			if (remove)
			{
				m_cur += read_len;
			}
			iRet=-1;
			break;
		}
	} while (0);
	return iRet;
}

int localFileReader::seekFile(long pos)
{
	if (!m_file_name||!strlen(m_file_name))
	{
		return	0;
	}
	int iRet=0;
	do
	{
		if (pos<0||pos>m_file_size)
		{
			iRet=-1;
			break;
		}
		m_fp=fopen(m_file_name,"rb");
		LOGT("fopen" << m_file_name);
		if (!m_fp)
		{
			iRet=-1;
			break;
		}
		//seek的范围是否在cache里面;
		long cache_end=m_cur_cache_start_pos+m_valid_cache_size;
		//如果在范围内，直接修改值即可;
		if (pos>=m_cur_cache_start_pos&&pos<=cache_end)
		{
			m_cur=pos-m_cur_cache_start_pos;
			break;
		}
		//如果不在，重新读取文件;
		iRet=seekToPos(pos);
		if (iRet!=0)
		{
			break;
		}
	} while (0);
	if (m_fp)
	{
		fclose(m_fp);
		LOGT("fclose" << m_fp);
		m_fp=0;
	}
	return iRet;
}

int localFileReader::backword(long bytes)
{
	if (bytes<m_cur)
	{
		m_cur -= bytes;
		return 0;
	}
	return -1;
}

localFileReader::localFileReader(localFileReader &)
{
}

int localFileReader::initFileReader(const char *file_name)
{
	int iRet=0;
	do
	{
		if (!file_name||strlen(file_name)<=0)
		{
			iRet=-1;
			break;
		}
		m_file_name=new char[strlen(file_name)+1];
		strcpy(m_file_name,file_name);
		m_fp=fopen(file_name,"rb");
		LOGT("fopen" << file_name);
		if (!m_fp)
		{
			iRet=-1;
			break;
		}
		m_file_size=getFileSize(m_fp);
		m_file_cache=new char[m_cache_size];
		fclose(m_fp);
		m_fp = 0;
		iRet=seekToPos(0);
		if (iRet!=0)
		{
			break;
		}
	} while (0);
	if (m_fp)
	{
		fclose(m_fp);
		LOGT("fclose" << m_fp);
		m_fp=0;
	}
	return iRet;
}

long localFileReader::getFileSize(FILE *fp)
{
	if (!fp)
	{
		return 0;
	}
	long cur_pos=ftell(fp);
	fseek(fp,0,SEEK_END);
	int file_size=ftell(fp);
	fseek(fp,cur_pos,SEEK_SET);
	return file_size;
}

int localFileReader::seekToPos(long pos)
{
	int iRet=0;
	do
	{
		if (pos<0)
		{
			iRet=-1;
			break;
		}
		m_fp=fopen(m_file_name,"rb");
		LOGT("fopen" << m_file_name<<"");
		if (!m_fp)
		{
			iRet=-1;
			break;
		}
		fseek(m_fp,0,SEEK_END);
		m_file_size=ftell(m_fp);
		if (pos>=m_file_size)
		{
			m_valid_cache_size = 0;
			iRet=-1;
			break;
		}
		//将pos设置为起点;
		m_cur=0;
		m_cur_cache_start_pos=pos;
		if(0!=fseek(m_fp,m_cur_cache_start_pos,SEEK_SET))
		{
			iRet=-1;
			break;
		}
		if (m_file_size-m_cur_cache_start_pos>m_cache_size)
		{
			m_valid_cache_size=m_cache_size;
		}else
		{
			m_valid_cache_size=m_file_size-m_cur_cache_start_pos;
		}
		if(1!=fread(m_file_cache,m_valid_cache_size,1,m_fp))
		{
			iRet=-1;
			break;
		}


	} while (0);
	if (m_fp)
	{
		fclose(m_fp);
		LOGT("fclose" << m_fp);
		m_fp=0;
	}
	return iRet;
}
