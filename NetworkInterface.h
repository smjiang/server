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
#include <vector>
#include "CLock.h"
#include "Db.h"
using namespace std;

#define SOCK_RECVBUF_SIZE 65536
typedef struct
{
	time_t updatetime;
	unsigned int ip;
	unsigned short port;
	string 	deviceID;
	char	recvbuf[SOCK_RECVBUF_SIZE];
	unsigned int datapos;
}SOCKINFO;

struct MsgHead
{
	unsigned short cmd;
	unsigned short len;
};

struct IMMSG
{
	string openID;
	string voiceUrl;
};

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
	void CloseSock(int sock);
	void DoSendMsg2WX();
	static void* AcceptThread(void* para);
	//static void* RecvThread(void* para);
	static void* ProcessThread(void* para);
	static void* SendMsg2WXThread(void* para);
	static CNetworkInterface* Instance();
	static void FreeInstance();
private:
	int ProcessBind(const char* openID,const char* equipmentID,const char* nickname,int bAdmin);
	int ProcessUnbind(const char* openID);
	int ProcessGetBind(const char* openID,string& uuid);
	int ProcessGetWXByDeviceID(const char* eID,vector<WXUserInfo>& openIDs);
	int ProcessGetDeviceIDByWX(const char* openID,string& deviceID);

	//int ProcessGetUserList(vector<WXUserInfo> userlist);
	bool IsWXUserAdmin(const char* openID);
	int ProcessDelWXUser(const char* openID);
	int ProcessSetWXUserAdmin(const char* openID);
	int ProcessSetWXUserNickName(const char* openID, const char* nickname);
	int ProcessGetNickName(const char* openID, string& nickname);

	int ProcessInsertQuestion(const char* question, const char* answer, int type);
	int ProcessGetAnswer(const char* question, string& answer, int& type);

	int SendMsgToWX(const char* openID,const char* msgurl);
	int SendMsgToDevice(const char* deviceID,const char* msgurl);

	
private:
	pthread_t m_threadid;
	bool m_status;
	int m_tcplistensock;
	int m_tcplistenport;
	int m_threadNum;//handle threads
	THREAD* m_threads;

	int m_httpServerSock;
	int m_httpClientSock;
	pthread_t m_threadMsg;
	queue<IMMSG> m_msgQue;
	CLock		 m_msgQueLock;
	
	int m_epfd;
	map<int,SOCKINFO> 	m_sockMap;
	CLock   m_sockLock;
	static CNetworkInterface* m_instance;
};
#endif