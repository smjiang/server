#ifndef _NETWORKINTERFACE_H_
#define _NETWORKINTERFACE_H_
#include <pthread.h>
#include <queue>
#include <string>
#include <sys/epoll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <map>
#include <queue>
#include "CLock.h"
using namespace std;

#define SOCK_RECVBUF_SIZE 65536
typedef struct
{
	time_t updatetime;
	unsigned int ip;
	unsigned short port;
	char	recvbuf[SOCK_RECVBUF_SIZE];
	unsigned int datapos;
}SOCKINFO;

typedef struct 
{
	void* 		serv;
	pthread_t	pid;
	int 		idx;
	int 		epfd;
	int			rfd;
	int			sfd;
	queue<int>  newconn;
}THREAD;

class CNetworkInterface
{
public:
	CNetworkInterface();
	~CNetworkInterface();

	int Init();
	void Stop();
	int CreateUDPSocket();
	int CreateTCPSocket();
	void DoAccept();
	void DoRecv();
	void DoProcess(int sock);
	void DelTimeout();
	void DispatchNewConn(int sock,int idx);
	static void* AcceptThread(void* para);
	//static void* RecvThread(void* para);
	static void* ProcessThread(void* para);
	static CNetworkInterface* Instance();
	static void FreeInstance();
private:
	pthread_t m_threadid;
	bool m_status;
	int m_tcplistensock;
	int m_tcplistenport;
	int m_threadNum;//handle threads
	THREAD* m_threads;

	int m_epfd;
	map<int,SOCKINFO> 	m_sockMap;
	CLock   m_sockLock;
	static CNetworkInterface* m_instance;
};
#endif