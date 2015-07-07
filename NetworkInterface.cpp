#include <unistd.h>
#include <errno.h>
#include "NetworkInterface.h"
#include "IniFileParser.h"
#include "Log.h"
#include "Tools.h"

#define MAXEVENT 100000

CNetworkInterface* CNetworkInterface::m_instance = NULL;
CNetworkInterface::CNetworkInterface()
{
}
CNetworkInterface::~CNetworkInterface()
{
}
CNetworkInterface* CNetworkInterface::Instance()
{
	if(NULL == m_instance)
	{
		m_instance = new CNetworkInterface();
	}
	return m_instance;
}
void CNetworkInterface::FreeInstance()
{
	if(m_instance)
	{
		delete m_instance;
		m_instance = NULL;
	}
}

int CNetworkInterface::Init()
{
	m_status = true;
	m_tcplistenport = 8087;
	m_threadNum = 2;
	
	unsigned int localip = GetLocalIP();
	char strip[16] = {0};
	IpInt2Str(localip,strip);
	CLog::getInstance()->info("Local IP %s",strip);

	CIniFileParser iniParser;
	int result = iniParser.init("server.ini");
	if(-1 == result)
	{
		CLog::getInstance()->error("can't open config file,use default parameters");
	}
	else
	{
		m_tcplistenport = iniParser.get_int("SERVER:tcplistenport",8087);
		CLog::getInstance()->info("read listen port %d",m_tcplistenport);

		m_threadNum = iniParser.get_int("SERVER:threadnum",1);
		CLog::getInstance()->info("read thread number %d",m_threadNum);
		if(m_threadNum == 0)
			m_threadNum = 1;
	}


	m_epfd = epoll_create(100000);
	if(-1 == m_epfd)
	{
		return -1;
	}
	
	m_tcplistensock = CreateTCPSocket();
	
	int reuseaddr = 1;
	int res = setsockopt(m_tcplistensock, SOL_SOCKET, SO_REUSEADDR, (char*)&reuseaddr, sizeof(reuseaddr));
	if(res != 0)
	{
		return -1;
	}
	sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(m_tcplistenport);
	addr.sin_addr.s_addr = INADDR_ANY;
	if(0 != bind(m_tcplistensock,(sockaddr*)&addr,sizeof(addr)))
	{
		CLog::getInstance()->error("bind port %d fail: %s",m_tcplistenport,strerror(errno));
		return -1;
	}
	if(listen(m_tcplistensock,4096) != 0)
	{
		return -1;
	}
	CLog::getInstance()->info("listen on port %d",m_tcplistenport);
	CLog::getInstance()->info("thread number  %d",m_threadNum);
	if(pthread_create(&m_threadid,NULL,AcceptThread,this) != 0)
	{
		return -1;
	}
	
	m_threads = new THREAD[m_threadNum];
	if(NULL == m_threads)
	{
		return -1;
	}
	for(int i=0; i < m_threadNum; i++)
	{
		int fds[2];
		if(pipe(fds))
		{
			return -1;
		}
		m_threads[i].rfd = fds[0];
		m_threads[i].sfd = fds[1];
		m_threads[i].idx = i;
		m_threads[i].epfd = m_epfd;
		m_threads[i].serv = this;

		pthread_create(&m_threads[i].pid,NULL,ProcessThread,m_threads+i);
		usleep(200);
	}
	return 0;
}

void CNetworkInterface::DispatchNewConn(int sock,int idx)
{
	THREAD* thd = m_threads + idx;
	thd->newconn.push(sock);
	write(thd->sfd,"",1);
}

void CNetworkInterface::DoAccept()
{
	while(true)
	{
		sockaddr_in addr;
		socklen_t len = sizeof(addr);
		int ns = accept(m_tcplistensock, (sockaddr*)&addr, &len);
		if(-1 == ns)
		{
			if(EMFILE == errno || errno == EINTR)//EMFILE:too many open files
			{
				CLog::getInstance()->error("accept sock fail %d: %s",errno, strerror(errno));
			}
			//CLog::getInstance()->error("2accept sock fail %d: %s",errno, strerror(errno));
			return;
		}
		SetNonblocking(ns);
      	int optint = 1;      		
		setsockopt(ns, SOL_SOCKET, SO_KEEPALIVE, (char *)&optint, sizeof(optint));
		setsockopt(ns, IPPROTO_TCP, TCP_NODELAY, (char *)&optint, sizeof(optint));
		
		struct epoll_event ev;
		memset(&ev, 0, sizeof(ev));		
		ev.events = EPOLLIN | EPOLLET;
		ev.data.fd = ns;
		if(epoll_ctl(m_epfd,EPOLL_CTL_ADD,ns,&ev) != 0)
		{
			CLog::getInstance()->error("epoll_ctl add sock fail %d: %s",errno, strerror(errno));
			close(ns);
			return;
		}
		CLog::getInstance()->info("accept a sock %d",ns);
		m_sockLock.Lock();
		SOCKINFO info;
		memset(&info,0,sizeof(info));
		info.ip = addr.sin_addr.s_addr;
		info.port = addr.sin_port;
		info.updatetime = time(NULL);
		map<int,SOCKINFO>::iterator itr = m_sockMap.find(ns);
		if(m_sockMap.end() != itr)
		{
			CLog::getInstance()->error("sock %d already in map",ns);
			m_sockMap.erase(itr);
		}
		m_sockMap.insert(make_pair(ns,info));
		m_sockLock.Unlock();
	}
}

void CNetworkInterface::DoRecv()
{
	struct epoll_event events[MAXEVENT];
	int fdnum = epoll_wait(m_epfd,events,MAXEVENT,10);
	for(int i=0; i < fdnum; i++)
	{
		int cfd = events[i].data.fd;
		if(cfd > 0)
		{
			int idx = cfd%m_threadNum;
			CLog::getInstance()->info("dispatch thread %d",idx);
			DispatchNewConn(cfd,idx);
		}
	}
}

void CNetworkInterface::DoProcess(int sock)
{
	pthread_t pid = pthread_self();
	SOCKINFO& info = m_sockMap[sock];
	info.updatetime = time(NULL);
	int ret = recv(sock,info.recvbuf+info.datapos, SOCK_RECVBUF_SIZE-info.datapos,0);
	if(0 == ret)
	{
		epoll_ctl(m_epfd,EPOLL_CTL_DEL,sock,NULL);
		close(sock);
		CLog::getInstance()->info("sock %d closed by peer",sock);
		m_sockLock.Lock();
		map<int,SOCKINFO>::iterator itr = m_sockMap.find(sock);
		if(m_sockMap.end() != itr)
		{
			m_sockMap.erase(itr);
		}
		m_sockLock.Unlock();
		return ;
	}
	else if (ret < 0)
	{
		struct epoll_event ev;
		memset(&ev, 0, sizeof(ev));
		ev.events = EPOLLIN | EPOLLET;
		ev.data.fd = sock;
		epoll_ctl(m_epfd,EPOLL_CTL_MOD,sock,&ev);
		return ;
	}
	while(ret > 0)
	{
		info.datapos += ret;
		info.recvbuf[info.datapos] = '\0';
		CLog::getInstance()->info("pid %d sock %d recv %s",pid,sock,info.recvbuf);

		ret = recv(sock,info.recvbuf+info.datapos, SOCK_RECVBUF_SIZE-info.datapos,0);
	}
	char* buf = info.recvbuf;
	char* pos = strstr(buf,"\r\n\r\n");
	if(memcmp(buf,"GET ",4) == 0)
	{
		const char* errmsg = "HTTP/1.1 404 Not Found\r\nContent-Type: text/plain\r\nContent-Length: 13\r\nConnection: close\r\n\r\n404 Not Found";
		if(pos)
		{
			send(sock,errmsg,strlen(errmsg),0);
			CLog::getInstance()->info("pid %d send %s",pid,errmsg);
		}
	}
	
	char* openID = "1234567890";
	char* msgType = "";
	char* msgContent;
	
}

void CNetworkInterface::DelTimeout()
{
	map<int,SOCKINFO>::iterator itr = m_sockMap.begin();
	time_t curtime = time(NULL);
	while(m_sockMap.end() != itr)
	{
		if(itr->second.updatetime+10 < curtime)
		{
			int sock = itr->first;
			CLog::getInstance()->info("Del Timeout sock %d",sock);
			epoll_ctl(m_epfd,EPOLL_CTL_DEL,sock,NULL);
			close(sock);
			m_sockMap.erase(itr++);
			continue;
		}
		itr++;
	}
}

void* CNetworkInterface::AcceptThread(void *para)
{
	CLog::getInstance()->info("AcceptThread start");
	CNetworkInterface* pNetwork = (CNetworkInterface*)para;
	while(pNetwork->m_status)
	{
		pNetwork->DoAccept();
		pNetwork->DoRecv();
		pNetwork->DelTimeout();
	}
	CLog::getInstance()->info("AcceptThread exit");
	return NULL;
}

/*void* CNetworkInterface::RecvThread(void *para)
{
	CLog::getInstance()->info("RecvThread start");
	CNetworkInterface* pNetwork = (CNetworkInterface*)para;
	while(pNetwork->m_status)
	{
		pNetwork->DoRecv();
	}
	CLog::getInstance()->info("RecvThread exit");
	return NULL;
}*/

void* CNetworkInterface::ProcessThread(void * para)
{
	THREAD* req = (THREAD*)para;
	CNetworkInterface* pNetwork = (CNetworkInterface*)req->serv;
	CLog::getInstance()->info("ProcessThread %d start",req->idx);
	SetNonblocking(req->rfd);
	while(pNetwork->m_status)
	{
		char buf[1];
		//CLog::getInstance()->info("ProcessThread read 1");
		if(read(req->rfd,buf,1) != 1)
		{
			usleep(10000);
			//CLog::getInstance()->info("ProcessThread read 2");
			continue;
		}
		int cfd = req->newconn.front();
		CLog::getInstance()->info("ProcessThread read sock %d",cfd);
		if(cfd > 0)
		{
			pNetwork->DoProcess(cfd);
		}
		req->newconn.pop();
	}
	CLog::getInstance()->info("ProcessThread %d start",req->idx);
	return NULL;
}

void CNetworkInterface::Stop()
{
	m_status = false;
	pthread_join(m_threadid,NULL);
	if(m_threads)
	{
		for(int i=0; i < m_threadNum; i++)
		{
			pthread_join(m_threads[i].pid,NULL);
		}
		delete[] m_threads;
	}
	CLog::getInstance()->info("network stop");
}

int CNetworkInterface::CreateUDPSocket()
{
	int s = socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);
	
	return s;
}

int CNetworkInterface::CreateTCPSocket()
{
	int s = socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
	SetNonblocking(s);
	int optint = 1;
	int	keepIdle = 30;
	int	keepInterval = 5;
	int	keepCount = 3;
	setsockopt(s, IPPROTO_TCP, TCP_NODELAY, (char *)&optint, sizeof(optint));
	setsockopt(s, SOL_SOCKET, SO_KEEPALIVE, (char *)&optint, sizeof(optint));
	setsockopt(s, SOL_TCP, TCP_KEEPIDLE, (void *)&keepIdle, sizeof(keepIdle));
	setsockopt(s, SOL_TCP, TCP_KEEPINTVL, (void *)&keepInterval, sizeof(keepInterval));
	setsockopt(s, SOL_TCP, TCP_KEEPCNT, (void *)&keepCount, sizeof(keepCount));

	return s;
}