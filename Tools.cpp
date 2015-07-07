#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include "Tools.h"
#include "Log.h"

int GetAddrInfo(char* host,unsigned int& ip)
{
	struct addrinfo *answer,hint,*result;
	bzero(&hint,sizeof(hint));
	hint.ai_family = AF_INET;
	hint.ai_socktype = SOCK_STREAM;
	int ret = getaddrinfo(host,NULL,&hint,&answer);
	if(0 != ret)
	{
		CLog::getInstance()->error("getaddrinfo fail %s",gai_strerror(ret));
		return ret;
	}
	for (result = answer; result != NULL; result = result->ai_next)
	{
		//char ipstr[16] = {0};
		//inet_ntop(AF_INET,&(((struct sockaddr_in *)(result->ai_addr))->sin_addr),ipstr, 16);
		ip = ((struct sockaddr_in *)(result->ai_addr))->sin_addr.s_addr;
		break;
		//CLog::getInstance()->info("get host %s ip : %s",host, ipstr);
	}
	freeaddrinfo(answer);
	return 0;
}

void IpInt2Str(int iIp,char* strIp)
{
	unsigned char ip[4];
	memcpy(ip,(void*)&iIp,4);
	sprintf(strIp,"%u.%u.%u.%u",ip[0],ip[1],ip[2],ip[3]);
}

int SetNonblocking(int sock)
{
	int opts;
    opts = fcntl(sock, F_GETFL);
    if(opts < 0)
	{
        return -1;
    }

    opts |= O_NONBLOCK;
    if(fcntl(sock, F_SETFL, opts) < 0)
	{
        return -1;
    }
	return 0;
}

int MakeDir(char* dirname)
{
	int res;	
	char path[512] = {0};	
	memcpy(path, dirname, strlen(dirname));
	char* pEnd = strstr(path+1, "/");
	if(NULL == pEnd)	
	{
		res = mkdir(path, 0777);
	}
	while(pEnd)	
	{
		*pEnd = 0;
		res = mkdir(path, 0777);
		*pEnd = '/';
		if(NULL == strstr(pEnd + 1, "/"))		
		{
			if('\0' != *(pEnd+1))		
			{				
				mkdir(path, 0777);		
			}
			break;		
		}
		else		
		{
			pEnd = strstr(pEnd + 1, "/");
		}
	}

	if(-1 == res) 
	{
		return -1;
	}
	return 0;
}

string GetValyeByKey(const char* para, const char* key)
{
	if(NULL == para || NULL == key)
	{
		return "";
	}
	char* pos = strstr((char*)para,key);
	if(pos)
	{
		pos += strlen(key);
		if('=' == *pos)
			pos++;
		char* end = strstr(pos,"&");
		if(end)
		{
			string value(pos,end-pos);
			return value;
		}
		else
		{
			return pos;
		}
	}
	return "";
}

//{"msg":"ok"}
string GetJsonValue(const char* msg,const char* key)
{
	if(NULL == msg || NULL == key)
	{
		return "";
	}
	char buf[256]={0};
	sprintf(buf,"\"%s\"",key);
	char* pos = strstr((char*)msg,buf);
	if(pos)
	{
		pos += strlen(buf);
		pos = strstr(pos,":");
		if(pos)
		{
			pos++;
			pos = strstr(pos,"\"");
			if(pos)
			{
				pos++;
				char* end = strstr(pos,"\"");
				if(end)
				{
					return string(pos,end-pos);
				}
			}
		}
	}
	return "";
}

unsigned int GetLocalIP()
{
	int res, sockfd;
	unsigned int localip = 0;
	char ip_str[32] = {0};
	const int MAXINTERFACES = 16;
	struct ifreq buf[MAXINTERFACES];
	struct sockaddr_in *host = NULL;

	sockfd = socket(PF_INET, SOCK_STREAM, 0);
    if(sockfd == -1)
	{
        return 0;
    }	
	struct ifconf ifc;
	ifc.ifc_len = sizeof(buf);
	ifc.ifc_buf = (caddr_t)buf;

	/* get all the interfaces */
	res = ioctl(sockfd, SIOCGIFCONF, (char*)&ifc);
	if(res < 0)
	{	
		return 0;	
	}
	int num = ifc.ifc_len / sizeof(struct ifreq);
	while(num-- > 0)
	{
		/* get ip of the interface */
		res = ioctl(sockfd, SIOCGIFADDR, (char*)&buf[num]);
		if(res < 0)
		{
			return 0;		
		}		
		memset(ip_str, 0, sizeof(ip_str));
		host = (struct sockaddr_in*)(&buf[num].ifr_addr);
		inet_ntop(AF_INET, &host->sin_addr, ip_str, sizeof(ip_str));
		if(strcmp(ip_str, "127.0.0.1") != 0 && strcmp(ip_str, "10.9.8.1") != 0)
		{
			/* not 127.0.0.1 */
			break;
		}
	}

	localip = host->sin_addr.s_addr;
    close(sockfd);
	return localip;

}
