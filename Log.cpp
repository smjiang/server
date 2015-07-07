#include "Log.h"
#include <stdarg.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <arpa/inet.h>

CLog CLog::m_instance;

CLog::CLog(void)
{
	m_timestamp = 0;
}

CLog::~CLog(void)
{
}

CLog* CLog::getInstance()
{
	return &m_instance;
}

int CLog::init(const char *path)
{
	int result;
	mkdir(path,S_IRWXU|S_IRWXG|S_IRWXO);
	strncpy(m_logPath,path,sizeof(m_logPath));
	pthread_mutex_init(&m_lock,NULL);
	pthread_mutex_init(&m_lockgdb,NULL);
	result = createLog();
	return result;
}

int CLog::info(const char *fmt,...)
{
	int result = 0;
	va_list ap;
	va_start(ap,fmt);
	result = log(LL_INFO,fmt,ap);
	va_end(ap);
	return result;
}

int CLog::debug(const char *fmt,...)
{
	int result = 0;
	va_list ap;
	va_start(ap,fmt);
	result = log(LL_DEBUG,fmt,ap);
	va_end(ap);
	return result;
}

int CLog::warning(const char *fmt,...)
{
	int result = 0;
	va_list ap;
	va_start(ap,fmt);
	result = log(LL_WARNING,fmt,ap);
	va_end(ap);
	return result;
}

int CLog::error(const char *fmt,...)
{
	int result = 0;
	va_list ap;
	va_start(ap,fmt);
	result = log(LL_ERROR,fmt,ap);
	va_end(ap);
	return result;
}

int CLog::statistic(const char *fmt,...)
{
	int result = 0;
	va_list ap;
	va_start(ap,fmt);
	result = log(LL_STA,fmt,ap);
	va_end(ap);
	return result;
}

int CLog::print(const char *fmt,...)
{
	int result = 0;

	va_list ap;
	time_t timeNow;
	struct tm tmNow;
	char buf[2048];
	int len;
	va_start(ap,fmt);
	timeNow = time(NULL);
	localtime_r(&timeNow,&tmNow);
	len = snprintf(buf,sizeof(buf)-1,"[%02d:%02d:%02d] [%s] [",tmNow.tm_hour,tmNow.tm_min,tmNow.tm_sec," INFO  ");
	len += vsnprintf(buf+len,sizeof(buf)-len,fmt,ap);
	len += snprintf(buf+len,sizeof(buf)-len,"]\n");
	result = printf("%s",buf);
	va_end(ap);

	return result;
}

int CLog::dump(const char *data,int len,const char *file,const char *func,int line)
{
	int result = 0;


	printf("dump begin %s:%s:%d\n",file,func,line);
	for (int i = 0; i < len; i++)
	{
		printf("%02x ",(unsigned char)data[i]);
		if ((i+1) % 16 == 0)
		{
			printf("\n");
		}
	}
	printf("\nend  ------------------------------------------------\n");


	return result;
}

int CLog::wirte(const char *msg,int len)
{
	int result = 0;
	time_t timeNow;

	timeNow = time(NULL);
	pthread_mutex_lock(&m_lock);
	if (m_logFile && timeNow >= m_timestamp + 86400)
	{
		m_timestamp += 86400;
		fflush(m_logFile);
		fclose(m_logFile);
		m_logFile = NULL;
	}
	if (m_logFile == NULL)
	{
		createLog();
	}
	if (m_logFile != NULL)
	{
#ifdef IMOVE_DEBUG
		printf("%s",msg);
#endif

		fwrite(msg,1,len,m_logFile);
		fflush(m_logFile);
	}
	pthread_mutex_unlock(&m_lock);
	return result;
}

int CLog::release()
{
	int result = 0;
	pthread_mutex_lock(&m_lock);
	fflush(m_logFile);
	fclose(m_logFile);
	m_logFile = NULL;
	pthread_mutex_unlock(&m_lock);
	return result;
}

int CLog::createLog()
{
	int result = 0;
	time_t timeNow;
	struct tm tmNow;
	char logPath[255];

	timeNow = time(NULL);
	localtime_r(&timeNow,&tmNow);
	snprintf(logPath,sizeof(logPath),"%s/%04d-%02d-%02d.log",m_logPath,tmNow.tm_year+1900,tmNow.tm_mon+1,tmNow.tm_mday);
	m_logFile = fopen(logPath,"a+");
	if (m_logFile == NULL)
	{
		result = -1;
	}

	tmNow.tm_hour = 0;
	tmNow.tm_min = 0;
	tmNow.tm_sec = 0;
	m_timestamp = mktime(&tmNow);
	return result;
}

int CLog::log(LOG_LEVEL level,const char *fmt,va_list varp)
{
	int len = 0;
	int result = 0;
	char buf[2048] = {0};
	time_t timeNow;
	struct tm tmNow;
	const char *pLevel;
	switch (level)
	{
	case LL_INFO:
		pLevel = " INFO  ";
		break;
	case LL_DEBUG:
		pLevel = " DEBUG ";
		break;
	case LL_ERROR:
		pLevel = " ERROR ";
		break;
	case LL_WARNING:
		pLevel = "WARNING";
		break;
	case LL_STA:
		pLevel = "STATIST";
		break;
	default:
		pLevel = "UNKNOWN";
		break;
	}
	timeNow = time(NULL);
	localtime_r(&timeNow,&tmNow);
	len = snprintf(buf,sizeof(buf)-1,"[%02d:%02d:%02d] [%s] [",tmNow.tm_hour,tmNow.tm_min,tmNow.tm_sec,pLevel);
	len += vsnprintf(buf+len,sizeof(buf)-len,fmt,varp);
	len += snprintf(buf+len,sizeof(buf)-len,"]\n");
	result = this->wirte(buf,len);
	printf(buf);
	return result;
}

int CLog::bin2str(const char *data,char *str,int len)
{
	int result = 0;
	for (int i = 0; i < len; i++)
	{
		sprintf(str+i*2,"%02X",(unsigned char)data[i]);
	}
	return result;
}

int CLog::str2bin(const char *str,unsigned char *data,int len)
{
	int result;
	int i;
	i = 0;
	while (len > 0)
	{
		sscanf(str+i*2,"%02X",&result);
		data[i] = result & 0xFF;
		len -= 2;
		i++;
	}
	return 0;
}

int CLog::ip2str(unsigned int ip,char *strIP)
{
	int result;
	unsigned char *p;
	p = (unsigned char *)&ip;
	result = sprintf(strIP,"%d.%d.%d.%d",p[0],p[1],p[2],p[3]);
	return result;
}

unsigned long long CLog::htonll(unsigned long long n)
{
	unsigned long long retval;

	retval = ((unsigned long long) htonl(n & 0xFFFFFFFFLLU)) << 32;
	retval |= htonl((n & 0xFFFFFFFF00000000LLU) >> 32);
	return(retval); 
}   

unsigned long long CLog::ntohll(unsigned long long n)
{
	unsigned long long retval;

	retval = ((uint64_t) ntohl(n & 0xFFFFFFFFLLU)) << 32;
	retval |= ntohl((n & 0xFFFFFFFF00000000LLU) >> 32);
	return(retval);
}

int CLog::gdb(const char *fmt,...)
{
	int result = 0;
	va_list ap;
	time_t timeNow;
	struct tm tmNow;
	char buf[2048];
	int len;
	FILE *pFile;

	va_start(ap,fmt);
	timeNow = time(NULL);
	localtime_r(&timeNow,&tmNow);
	len = snprintf(buf,sizeof(buf)-1,"[%02d:%02d:%02d] [%s] [",tmNow.tm_hour,tmNow.tm_min,tmNow.tm_sec,"GDB");
	len += vsnprintf(buf+len,sizeof(buf)-len,fmt,ap);
	len += snprintf(buf+len,sizeof(buf)-len,"]\n");
	va_end(ap);

	pthread_mutex_lock(&m_lockgdb);
	pFile = fopen("gdb.log","a+");
	if (pFile)
	{
		fprintf(pFile,"%s",buf);
		fflush(pFile);
		fclose(pFile);
	}
	pthread_mutex_unlock(&m_lockgdb);
	return result;
}

int CLog::gdb_dump(const char *data,int len)
{
	char buf[5120];

#ifdef IMOVE_DEBUG
	result = snprintf(buf,buflen,"dump begin\r\n");
	for (int i = 0; i < len; i++)
	{
		result += snprintf(buf+result,buflen-result,"%02x ",(unsigned char)data[i]);
		if ((i+1) % 16 == 0)
		{
			result += snprintf(buf+result,buflen-result,"\r\n");
		}
	}
	result += snprintf(buf+result,buflen-result,"\r\nend  ------------------------------------------------\r\n");
#endif
	return this->gdb("%s",buf);
}
