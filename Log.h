#pragma once

#include <time.h>
#include <stdio.h>
#include <pthread.h>
#include <stdarg.h>

enum LOG_LEVEL
{
	LL_INFO,LL_DEBUG,LL_WARNING,LL_ERROR,LL_STA
};

class CLog
{
private:
	CLog(void);
	~CLog(void);
public:
	static CLog* getInstance();
	int init(const char *path);
	int info(const char *fmt,...);
	int debug(const char *fmt,...);
	int warning(const char *fmt,...);
	int error(const char *fmt,...);
	int print(const char *fmt,...);
	int statistic(const char *fmt,...);
	int dump(const char *data,int len,const char *file=__FILE__,const char *func=__FUNCTION__,int line=__LINE__);
	int ip2str(unsigned int ip,char *strIP);
	int bin2str(const char *data,char *str,int len);
	int str2bin(const char *str,unsigned char *data,int len);
	int wirte(const char *msg,int len);
	int release();
public:
	int gdb(const char *fmt,...);
	int gdb_dump(const char *data,int len);
public:
	unsigned long long htonll(unsigned long long n);
	unsigned long long ntohll(unsigned long long n);
private:
	int createLog();
	int log(LOG_LEVEL level,const char *fmt,va_list varp);
private:
	char m_logPath[255];
	time_t m_timestamp;
	FILE	*m_logFile;
	pthread_mutex_t m_lock;
	pthread_mutex_t	m_lockgdb;
	static CLog m_instance;
};

//dump
#ifdef IMOVE_DEBUG
#define DATADUMP(a,b)	CLog::getInstance()->dump(a,b,__FILE__,__FUNCTION__,__LINE__)
#else
#define DATADUMP(a,b)
#endif

