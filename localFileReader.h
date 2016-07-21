#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const int CACHE_SIZE_DEFAULT=1024*1024*4;

class localFileReader
{
public:
	localFileReader(const char *file_name,int cache_size=CACHE_SIZE_DEFAULT);
	~localFileReader();
	//该次读取时该文件总大小;
	long fileSize();
	//当前读取进度;
	long fileCur();
	//读取文件,成功返回0;文件结束-1;缓存不够错误-2;文件不存在-3;
	int readFile(char *out_buf,int  &read_len,bool remove=true);
	//seek文件,成功返回0;
	int seekFile(long pos);
	//后退几个字节;
	int backword(long bytes);
protected:
	localFileReader(localFileReader&);
private:
	int initFileReader(const char *file_name);
	long getFileSize(FILE *fp);
	//成功返回0;
	int seekToPos(long pos);
	long	m_file_size;
	//当前cache相对于文件的起点;
	long m_cur_cache_start_pos;
	//读到当前cache的位置，从0开始;
	long m_cur;
	char *m_file_cache;
	int m_cache_size;
	int m_valid_cache_size;
	FILE *m_fp;
	char *m_file_name;
};
