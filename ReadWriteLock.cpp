#include"util.h"

ReadWriteLock::ReadWriteLock(void):numReaders_(0)
{
}

ReadWriteLock::~ReadWriteLock(void)
{
}

void ReadWriteLock::acquireReader()
{
	lock(mtx_writer_, mtx_numReaders_);
	numReaders_++;
	mtx_writer_.unlock();
	mtx_numReaders_.unlock();
}

void ReadWriteLock::releaseReader()
{
	std::unique_lock<std::mutex> _(mtx_numReaders_);

	numReaders_--;
	if (numReaders_ == 0)
		cv_numReaders_.notify_one();
}

void ReadWriteLock::acquireWriter()
{
	lock(mtx_writer_, mtx_numReaders_);
	cv_numReaders_.wait(mtx_numReaders_, [&] { return numReaders_ == 0; });
}

void ReadWriteLock::releaseWriter()
{
	cv_numReaders_.notify_one();
	mtx_writer_.unlock();
	mtx_numReaders_.unlock();
}
