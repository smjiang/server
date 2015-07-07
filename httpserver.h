#ifndef _HTTPSERVER_H
#define _HTTPSERVER_H
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <poll.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/ioctl.h>
#include <net/if_arp.h>
#include <net/if.h>
#include <iostream>
#include <vector>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <list>
#include "CLock.h"
using namespace std;


#define MAXCONNECS 64
#define HTTP_BUFFER_LENGTH 65536
#define INVALID_SOCKET -1
typedef unsigned long long UINT64;
typedef unsigned int UINT;

class CHttpServer
{	
public:
	CHttpServer();
	virtual ~CHttpServer();

	bool GetHttpUrlFromBuffer(char* buf,char* url,int urllen);
	void GetFileNameFromUrl(char* url,char* filename);
	virtual int run();
	virtual int stop();
	bool OnTimerEvent();

	int GetSocket(int i){return m_SocketArray[i];}

	int GetSocketNum(){return m_nsocket;}
	virtual bool InnerAddSocket(int s,UINT ip);
	bool InnerRemoveSocket(int s, int removepoint);
	bool InitListen(unsigned short port);

	int ProcessHttpReq(char* buf,int len,int i);
	void WriteM3U8File(int i);
	void SendToDatabase(int i);

	static void* Routine(void* pvoid);
	int m_listensock; /* only for recording the value of listen socket */

private:
	UINT64 m_tickecount;
	UINT64 m_interval;
	CLock  m_lock;
	bool   m_status;
	pthread_t m_tid;
	time_t   m_TimeArray[MAXCONNECS];

	/*
	 * no need to connect, only listen.
	 */
	struct pollfd pfds[MAXCONNECS];


	int m_SocketArray[MAXCONNECS]; /* including listen sockets */
	UINT m_SocketIP[MAXCONNECS];
	FILE* m_pFile[MAXCONNECS];

	char m_recvbuf[MAXCONNECS][HTTP_BUFFER_LENGTH];
	int  m_recvlen[MAXCONNECS];


	UINT m_nsocket;//套接字数目，最多只能4个
	UINT m_sended;
	UINT m_dataLen[MAXCONNECS];
	UINT m_sendlen[MAXCONNECS];

	std::list<std::string> m_latestFileQue[MAXCONNECS];
	std::string 		  m_filePath[MAXCONNECS];
	std::string 		  m_fileInfo[MAXCONNECS];

	int  m_sock;

};

class CHttpServerMgr: public CHttpServer
{
public:
	CHttpServerMgr();
	virtual ~CHttpServerMgr();
	virtual int run();
	virtual int stop();
	void init(unsigned short port);
	virtual bool InnerAddSocket(int s,UINT ip);
	int GetTotalPeers();
private:
	std::vector<CHttpServer*> m_vctServer;
	unsigned short m_listenport;
};
#endif

