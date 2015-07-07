#include "httpserver.h"
#include <netinet/tcp.h>
#include <string>
#include <sys/stat.h>
#include "Log.h"
#include "Tools.h"
#include "PeerGroup.h"

extern CHttpServerMgr g_httpServer;

CHttpServer::CHttpServer()
:m_interval(125)
{
	m_nsocket = 0;
	m_sended = 0;
	for(int i=0;i<MAXCONNECS;i++)
	{
		m_TimeArray[i] = 0;
	}


	int i;
	for(i = 0; i < MAXCONNECS; ++i)
	{
		pfds[i].fd = INVALID_SOCKET;
		pfds[i].events = 0;
		pfds[i].revents = 0;
	}

	m_listensock = INVALID_SOCKET;

}
CHttpServer::~CHttpServer()
{
}

int CHttpServer::run()
{
	m_status = true;
	m_sock = -1;
	pthread_create(&m_tid,NULL, Routine, this);
	return 0;
}
int CHttpServer::stop()
{
	m_status = false;

	pthread_join(m_tid,NULL);
	for(unsigned int i = 0;i< m_nsocket;i++)
	{
		close(m_SocketArray[i]);
	}

	m_listensock = INVALID_SOCKET;
	return 0;
}

bool CHttpServer::OnTimerEvent()
{
	CAutoLock lock(m_lock);
	if(m_nsocket < 1)
		return true;

	time_t curTime = time(NULL);
	unsigned int i;
	for(i = 0;i<m_nsocket;i++)
	{
		//¼ì²é³¬Ê±

		if(m_TimeArray[i] != 0)
		{
			if(curTime-m_TimeArray[i] > 30)
			{
				if (InnerRemoveSocket(m_SocketArray[i], 9))
				{
					i--;
				}
				continue;
			}
		}
	}
	return true;
}
bool CHttpServer::InitListen(unsigned short port)
{
	int s = socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
	sockaddr_in addr;
	memset(&addr,0,sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = htons(port);

	int reuseaddr = 1;
	int res = setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char*)&reuseaddr, sizeof(reuseaddr));
	if(res != 0)
	{
		return false;
	}

	if(0 != bind(s,(sockaddr*)&addr,sizeof(addr)))
	{
		CLog::getInstance()->error("httpserver listen on port fail %d: %d",port,errno);
		return false;
	}
	
	if(listen(s,4096) != 0)
	{
		return false;
	}
	CLog::getInstance()->info("httpserver listen on port %d",port);

	/* add listen socket to event */
	SetNonblocking(s);
	int optint = 1;
  	//int	keepAlive = 1;
	int	keepIdle = 30;
	int	keepInterval = 5;
	int	keepCount = 3;
	setsockopt(s, IPPROTO_TCP, TCP_NODELAY, (char *)&optint, sizeof(optint));
	setsockopt(s, SOL_SOCKET, SO_KEEPALIVE, (char *)&optint, sizeof(optint));
	setsockopt(s, SOL_TCP, TCP_KEEPIDLE, (void *)&keepIdle, sizeof(keepIdle));
	setsockopt(s, SOL_TCP, TCP_KEEPINTVL, (void *)&keepInterval, sizeof(keepInterval));
	setsockopt(s, SOL_TCP, TCP_KEEPCNT, (void *)&keepCount, sizeof(keepCount));

	pfds[m_nsocket].fd = s;
	pfds[m_nsocket].events |= POLLIN;

	/* add listen socket to the array, and then set m_listensock */
	m_SocketArray[m_nsocket] = s;
	m_TimeArray[m_nsocket] = 0;
	m_nsocket++;
	/* set value of m_listensock */
	m_listensock = s;

	return true;
}

bool CHttpServer::InnerAddSocket(int s,UINT ip)
{
	CAutoLock lock(m_lock);
	
	if(m_nsocket >= MAXCONNECS)
	{
		return false;
	}

	int sendbuf = 1024*256;
	setsockopt(s,SOL_SOCKET,SO_RCVBUF,(char*)&sendbuf,sizeof(int));
	
	m_SocketArray[m_nsocket] = s;
	m_SocketIP[m_nsocket] = ip;
	m_dataLen[m_nsocket] = 0;
	m_sendlen[m_nsocket] = 0;
	m_recvlen[m_nsocket] = 0;
	m_pFile[m_nsocket] = NULL;
	m_TimeArray[m_nsocket] = time(NULL);
	m_latestFileQue[m_nsocket].clear();
	m_filePath[m_nsocket] = "";
	m_fileInfo[m_nsocket] = "";


	/* add socket to event */
	SetNonblocking(s);
	int optint = 1;
  	//int	keepAlive = 1;
	int	keepIdle = 30;
	int	keepInterval = 5;
	int	keepCount = 3;
	setsockopt(s, IPPROTO_TCP, TCP_NODELAY, (char *)&optint, sizeof(optint));
	setsockopt(s, SOL_SOCKET, SO_KEEPALIVE, (char *)&optint, sizeof(optint));
	setsockopt(s, SOL_TCP, TCP_KEEPIDLE, (void *)&keepIdle, sizeof(keepIdle));
	setsockopt(s, SOL_TCP, TCP_KEEPINTVL, (void *)&keepInterval, sizeof(keepInterval));
	setsockopt(s, SOL_TCP, TCP_KEEPCNT, (void *)&keepCount, sizeof(keepCount));

	pfds[m_nsocket].fd = s;
	pfds[m_nsocket].events |= (POLLIN | POLLERR | POLLHUP | POLLNVAL);

	m_nsocket++;
	return true;		
}

bool CHttpServer::InnerRemoveSocket(int s, int removepoint)
{
	CAutoLock lock(m_lock);
	UINT i;
	if(s == m_listensock)
	{
		CLog::getInstance()->error("remove listen sock %d",s);
	}
	for (i = 0;i<m_nsocket;i++)
	{
		if (m_SocketArray[i] == s)
		{
			shutdown(s, 2);
			close(s);
			m_SocketArray[i] = m_SocketArray[--m_nsocket];


			pfds[i] = pfds[m_nsocket];

			/* reset pfds[m_nsocket] */
			pfds[m_nsocket].fd = -1;
			pfds[m_nsocket].events = 0;
			pfds[m_nsocket].revents = 0;

			m_SocketIP[i] = m_SocketIP[m_nsocket];
			m_TimeArray[i] = m_TimeArray[m_nsocket];

			m_dataLen[i] = m_dataLen[m_nsocket];
			m_sendlen[i] = m_sendlen[m_nsocket];
			m_recvlen[i] = m_recvlen[m_nsocket];
			m_latestFileQue[i] = m_latestFileQue[m_nsocket];
			m_filePath[i] = m_filePath[m_nsocket];
			m_fileInfo[i] = m_fileInfo[m_nsocket];
			if(m_pFile[i])
			{
				fclose(m_pFile[i]);
				m_pFile[i] = NULL;
			}
			m_pFile[i] = m_pFile[m_nsocket];
			memcpy(m_recvbuf[i],m_recvbuf[m_nsocket],m_recvlen[m_nsocket]);
			return true;
		}
	}
	return false;
}
bool CHttpServer::GetHttpUrlFromBuffer(char* buf,char* url,int urllen)
{
	char* start = NULL;
	char* end = NULL;
	start = strstr(buf,"GET ");
	if(start)
	{
		start += 4;
		end = strstr(start," HTTP/1.");
		if(end)
		{
			int length = end - start;
			if(length < urllen)
			{
				memcpy(url,start,length);
				url[length] = '\0';
				return true;
			}
		}
	}
	return false;
}

void CHttpServer::GetFileNameFromUrl(char* url,char* filename)
{
	char* pos = strrchr(url,'/');
	strcpy(filename,pos+1);
	pos = strstr(filename,"?");
	if(pos)
	{
		*pos = '\0';
	}
}

int CHttpServer::ProcessHttpReq(char* buf,int len,int i)
{
	//CLog::getInstance()->info("ProcessHttpReq recv len %d\n",len);
	char* pbuf = buf;
	char* pos = strstr(buf,"\r\n\r\n");
	if(NULL == m_pFile[i] && pos)
	{
		pbuf = pos + 4; 
		pos = strstr(buf,"POST ");
		if(pos)
		{
			pos += 5;//url _147648.ts?prefix=67427990971/30988/
			char* end = strstr(pos,"HTTP/");
			if(end)
			{
				end = strstr(pos,"?");
				if(end)
				{
					*end = '\0';
					string filename = pos;
					*end = '?';
					string prefix;
					char* para = end+1;
					end = strstr(para," ");
					if(end)
					{
						*end = '\0';

						prefix = GetValyeByKey(para,"prefix=");
						string userhash = GetValyeByKey(para,"userhash=");
						string channelhash = GetValyeByKey(para,"channelhash=");
						string bitrate = GetValyeByKey(para,"bitrate=");
						m_filePath[i] = "/data/wwwroot/live.co-cloud.com/liveuser/";
						if(userhash.size() > 0 && channelhash.size() > 0 && bitrate.size() > 0)
						{
							prefix = userhash + "/" + channelhash + "/" + bitrate + "/";
							m_fileInfo[i] = para;
							CLog::getInstance()->info("recv para %s,prefix %s",m_fileInfo[i].c_str(),prefix.c_str());
						}
						else if(prefix.size() > 0)
						{
							channelhash = prefix;
							CLog::getInstance()->info("recv test prefix %s",prefix.c_str());
						}
						else
						{
							CLog::getInstance()->info("2 invalid request %s",para);
							return 0;
							//prefix = para;
						}
						
						if ('/' == prefix[0])
						{
							prefix = prefix.substr(1);
						}
						if ('/' != prefix[prefix.size()-1])
						{
							prefix += "/";
						}
						
						m_filePath[i] += prefix;
						MakeDir((char*)m_filePath[i].c_str());
						
						if(!strstr(filename.c_str(),".m3u8"))
						{
							if(!CPeerGroup::Instance()->IsChannelExist(channelhash))
							{
								ChannelInfo info;
								info.lastNum = time(NULL);
								info.status = 0;
								info.updatetime = time(NULL);
								CPeerGroup::Instance()->AddChannel(channelhash,info);
							}
							CPeerGroup::Instance()->IncreChannelNum(channelhash);
							
							size_t suffixPos = filename.rfind(".");
							string suffix;
							if(string::npos != suffixPos)
							{
								suffix = filename.substr(suffixPos);
								CLog::getInstance()->info("filename %s,suffix %s",filename.c_str(),suffix.c_str());
							}
							else
							{
								CLog::getInstance()->info("filename %s, no suffix",filename.c_str());
							}

							char strtime[10] = {0};

							time_t timeNow = time(NULL);
							struct tm tmNow;
							localtime_r(&timeNow,&tmNow);
							snprintf(strtime,10,"%04d%02d%02d/",tmNow.tm_year+1900,tmNow.tm_mon+1,tmNow.tm_mday);
							
							filename = strtime;
							MakeDir((char*)(m_filePath[i]+filename).c_str());
						
							char tmp[50] = {0};
							sprintf(tmp,"_%lld",CPeerGroup::Instance()->GetChannelLastNum(channelhash));
							filename += tmp;
							filename += suffix;
						}
						
						m_filePath[i] += filename;
						m_pFile[i] = fopen(m_filePath[i].c_str(),"wb+");
						if(NULL == m_pFile[i])
						{
							CLog::getInstance()->error("can't create file %s %d: %s",m_filePath[i].c_str(),errno,strerror(errno));
							m_recvlen[i] = 0;
							return 0;
						}
						if(!strstr(filename.c_str(),".m3u8"))
						{
							m_latestFileQue[i].push_back(filename);
							while(m_latestFileQue[i].size() > 5)
							{
								m_latestFileQue[i].pop_front();
							}
						}
						
						*end = ' ';
					}
					
					end = strstr(buf,"Content-Length: ");
					if(end)
					{
						end += strlen("Content-Length: ");
						m_dataLen[i] = atoi(end);
					}
					CLog::getInstance()->info("recv a ts %s,prefix %s,len %d",filename.c_str(),prefix.c_str(),m_dataLen[i]);
				}
				else
				{
					CLog::getInstance()->info("3 invalid http request %0.20s",buf);
					return 0;
				}
			}
			else
			{
				CLog::getInstance()->info("2 invalid http request %0.20s",buf);
				return 0;
			}
		}
		else
		{
			if(memcmp(buf,"GET /monitor",8) == 0)
			{
				vector<string> channels;
				CPeerGroup::Instance()->GetAllChannel(channels);
				char contentbuf[2048] = {0};
				sprintf(contentbuf,"channels %d\n",channels.size());
				vector<string>::iterator itr = channels.begin();
				while(channels.end() != itr)
				{
					strcat(contentbuf,(*itr).c_str());
					strcat(contentbuf,"\n");
					if(strlen(contentbuf)>1800)
					{
						strcat(contentbuf,"...");
						break;
					}
					itr++;
				}
				char monitorResp[2500] = {0};
				sprintf(monitorResp,"HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: %d\r\n\r\n%s",strlen(contentbuf),contentbuf);
				send(m_SocketArray[i],monitorResp,strlen(monitorResp),0);
				return 0;
			}
			CLog::getInstance()->info("1 invalid http request %0.20s",buf);
			return 0;
		}
		len -= (int)(pbuf - buf);
		m_recvlen[i] = 0;
	}
	
	if(len > 0 && NULL != m_pFile[i])
	{
		int writelen = len;
		if((int)(m_dataLen[i]-m_sendlen[i]) < len)
		{
			writelen = m_dataLen[i]-m_sendlen[i];
		}
		fwrite(pbuf,1,writelen,m_pFile[i]);
		m_sendlen[i] += writelen;

		if(m_sendlen[i] == m_dataLen[i])
		{
			CLog::getInstance()->info("recv finish len %d\n",m_dataLen[i]);
			fclose(m_pFile[i]);
			
			if(!strstr(m_filePath[i].c_str(),".m3u8"))
			{
				WriteM3U8File(i);
				if(m_fileInfo[i].size()>0)
				{
					SendToDatabase(i);
				}
			}
			m_pFile[i] = NULL;
			m_sendlen[i] = 0;
			m_dataLen[i] = 0;
			m_recvlen[i] = 0;
			m_filePath[i] = "";
			m_fileInfo[i] = "";
			char httpHeadBuf[1024] = "HTTP/1.1 200 OK\r\nContent-Type: application/octet-stream\r\n\r\n";
			send(m_SocketArray[i],httpHeadBuf,strlen(httpHeadBuf),0);
			if(writelen < len)
			{
				CLog::getInstance()->error("http recv len error len %d, writelen %d\n",len,writelen);
				return 0;
			}
		}
	}
	else
	{
		m_recvlen[i] += len;
		if(NULL == m_pFile[i])
		{
			CLog::getInstance()->error("http head not finish recv len %d,%d",len,m_recvlen[i]);
		}
		if(m_recvlen[i] > 4095)
		{
			return 0;
		}
	}
	m_TimeArray[i] = time(NULL);
	return 1;
}

void CHttpServer::WriteM3U8File(int i)
{
	char buf[1024] = {0};
	const char* fmt = "#EXTM3U\n#EXT-X-VERSION:3\n#EXT-X-TARGETDURATION:5\n#EXT-X-MEDIA-SEQUENCE:%lld\n";
	string first = m_latestFileQue[i].front();
	size_t pos = first.rfind('/');
	if(string::npos != pos)
	{
		first = first.substr(pos+1);
	}
	while('_' == first[0])
	{
		first = first.substr(1);
	}
	unsigned long long seq = atoll(first.c_str());
	sprintf(buf,fmt,seq);
	list<string>::iterator itr = m_latestFileQue[i].begin();
	while(itr != m_latestFileQue[i].end())
	{
		strcat(buf,"#EXTINF:5.000,\n");
		strcat(buf,(*itr).c_str());
		strcat(buf,"\n");
		itr++;
	}
	string path = m_filePath[i];
	pos = path.rfind('/');
	if(string::npos != pos)
	{
		path = path.substr(0,pos+1);

		pos = path.rfind('/',pos-1);
		if(string::npos != pos)
		{
			path = path.substr(0,pos+1);
		}
		
		string m3u8Tmpfile = path + "_m.m3u8";
		string m3u8file = path + "m.m3u8";
		FILE* pfile = fopen(m3u8Tmpfile.c_str(),"wb+");
		if(pfile)
		{
			fwrite(buf,1,strlen(buf),pfile);
			fclose(pfile);
			CLog::getInstance()->info("write m3u8 %s",buf);

			remove(m3u8file.c_str());
			rename(m3u8Tmpfile.c_str(),m3u8file.c_str());
		}
		else
		{
			CLog::getInstance()->error("can't write m3u8 file %d:%s",errno,strerror(errno));
		}
	}
	else
	{
		CLog::getInstance()->error("write m3u8 path format wrong %s",path.c_str());
	}
}

void CHttpServer::SendToDatabase(int i)
{
	int ret = 0;
	if(-1 == m_sock)
	{
		hostent *pHost = gethostbyname("stat.co-cloud.com");
		unsigned int ip = 0;
		if( pHost != NULL )
		{
			memcpy(&ip,pHost->h_addr_list[0],sizeof(int));
		}
		struct sockaddr_in serv;
		serv.sin_family = AF_INET;
		serv.sin_addr.s_addr = ip;
		serv.sin_port = htons(80);
	
		m_sock = socket(AF_INET, SOCK_STREAM,IPPROTO_TCP);
		if(-1 == m_sock)
		{
			return ;
		}
		struct timeval timeout;
		timeout.tv_sec=3;
		timeout.tv_usec=0;

		setsockopt(m_sock, SOL_SOCKET,SO_RCVTIMEO, (char*)&timeout,sizeof(timeout));
		setsockopt(m_sock, SOL_SOCKET,SO_SNDTIMEO, (char*)&timeout,sizeof(timeout));
		
		ret = connect(m_sock,(struct sockaddr*)&serv,sizeof(serv));
		if(ret != 0)
		{
			close(m_sock);
			m_sock = -1;
			CLog::getInstance()->info("can NOT connect to DB server");
			return;
		}
	}

	const char* fmt = "GET /api/index.php?cmd=liveplayfileadd&%s&path=%s HTTP/1.1\r\nHost: stat.co-cloud.com\r\n\r\n";
	char buf[1024] = {0};
	sprintf(buf,fmt,m_fileInfo[i].c_str(),m_filePath[i].c_str());
	
	ret = send(m_sock,buf,strlen(buf),0);
	if(ret < 0)
	{
		close(m_sock);
		m_sock = -1;
		CLog::getInstance()->info("send to DB fail %d: %s",errno,buf);
		return ;
	}
	CLog::getInstance()->info("send to DB %s",buf);
	ret = recv(m_sock,buf,1023,0);
	if(ret <= 0)
	{
		close(m_sock);
		m_sock = -1;
		CLog::getInstance()->info("recv from DB fail %d",errno);
		return ;
	}
	buf[ret] = '\0';
	CLog::getInstance()->info("recv from DB %s",buf);
	string res;
	char* pos = strstr(buf,"\r\n\r\n");
	if(pos)
	{
		pos += 4;
		res = GetJsonValue(pos,"msg");
	}
	if("ok" == res)
	{
		CLog::getInstance()->info("insert DB ok");
	}
	else if("3" == res)
	{
		CLog::getInstance()->info("insert DB fail : channel NOT exist");
	}
	else if("2" == res)
	{
		CLog::getInstance()->info("insert DB fail : channel is LOCKED");
	}
	else if("4" == res)
	{
		CLog::getInstance()->info("insert DB fail : AUTH fail");
	}
	else if("5" == res)
	{
		CLog::getInstance()->info("insert DB fail : channel timeout");
	}
	else
	{
		CLog::getInstance()->info("insert DB fail unknow reason: %s",res.c_str());
	}
}

void* CHttpServer::Routine(void* pvoid)
{
	CHttpServer* psvr = (CHttpServer*)pvoid;
	printf("httpserver start\n");
	while(psvr->m_status)
	{
		psvr->OnTimerEvent();
		CPeerGroup::Instance()->DelTimeoutChannel();
		int SocketNum = psvr->m_nsocket;
		if(SocketNum < 1)
		{
			usleep(50);
			continue;
		}

		/* start poll */
		int result = poll(psvr->pfds, SocketNum, 5);
		if(result == -1)
		{
			usleep(100);

			int err = errno;
			if(err == EINTR) {
				/* a singal was caught */
				continue;
			}

			if(err == EBADF) {
				/* maybe already closed */
				continue;
			}
			CLog::getInstance()->error("program exit");
			exit(-1);
		}

		if(result == 0)
		{
			/* timeout */
			continue;
		}

		/* check socket event */
		int i;
		for(i = 0; i < SocketNum; ++i)
		{

			int s = psvr->GetSocket(i);
			if(psvr->pfds[i].revents & POLLIN)
			{
				/* socket have read event */
				if(s == psvr->m_listensock)
				{
					int iLoop = 0;
					while(true)
					{
						if(iLoop++ > 256)
						{
							break;
						}
						/* is m_listensock */
						sockaddr_in addr;
						socklen_t len = sizeof(addr);
						int ns = accept(s, (sockaddr*)&addr, &len);
						if(ns == -1)
						{
							if(ECONNABORTED == errno && iLoop < 256)
							{
								continue;
							}
							if(EAGAIN == errno || EMFILE == errno || errno == EINTR)//EMFILE:too many open files
							{
								break;
							}
							break;
						}
						printf("accept a conn sock %d\n",ns);
						if(!g_httpServer.InnerAddSocket(ns,addr.sin_addr.s_addr)) 
						{
							close(ns);
							continue;
					    }
					}

					continue;
				}


				/* common socket have read event */
				int nlen = recv(s, psvr->m_recvbuf[i]+psvr->m_recvlen[i], 8192, 0);
				if(nlen < 0)
				{
					/* error */
					int err = errno;
					if(EINTR != err && EAGAIN != err)
					{
						if(psvr->InnerRemoveSocket(s, 2*1000+err))
						{
							i--;
						}
					}
					continue;
				}
				else if(nlen == 0)
				{
					/* recv FIN */
					if(psvr->InnerRemoveSocket(s, 1))
					{
						i--;
					}
					continue;
				}

				int ilen = 0;
				char* buf = psvr->m_recvbuf[i];
				ilen = psvr->ProcessHttpReq(buf,nlen,i);
				
				if(ilen == 0)
				{
					if(psvr->InnerRemoveSocket(s, 3))
					{
						i--;
					}
					continue;
				}
			} /* POLLIN event */

			if(psvr->pfds[i].revents & (POLLERR | POLLHUP | POLLNVAL))
			{
				if(psvr->InnerRemoveSocket(s, 4))
				{
					i--;
				}
				continue;
			}
		} /* for */

	} /* while */
	return NULL;
}



CHttpServerMgr::CHttpServerMgr()
{
}

CHttpServerMgr::~CHttpServerMgr()
{
}

int CHttpServerMgr::run()
{
	CHttpServer* pServer = new CHttpServer();
	if(!pServer->InitListen(m_listenport))
	{
		return 2;
	}
	pServer->run();
	m_vctServer.push_back(pServer);
	return 0;
}
int CHttpServerMgr::stop()
{
	std::vector<CHttpServer*>::iterator it;
	CHttpServer* pServer = NULL;
	for(it = m_vctServer.begin();it!= m_vctServer.end();it++)
	{
		pServer = *it;
		pServer->stop();
		delete pServer;
	}
	m_vctServer.clear();
	return 0;
	
}

bool CHttpServerMgr::InnerAddSocket(int s,UINT ip)
{
	std::vector<CHttpServer*>::iterator it;
	CHttpServer* pServer = NULL;
	for(it = m_vctServer.begin();it!= m_vctServer.end();it++)
	{
		pServer = *it;
		if(pServer->GetSocketNum() < MAXCONNECS && INVALID_SOCKET == pServer->m_listensock)
		{
			return pServer->InnerAddSocket(s,ip);
		}
	}
	pServer = new CHttpServer();
	pServer->InnerAddSocket(s,ip);
	pServer->run();
	m_vctServer.push_back(pServer);
	return true;
}

void CHttpServerMgr::init(unsigned short port)
{
	m_listenport = port;
}

int CHttpServerMgr::GetTotalPeers()
{
	std::vector<CHttpServer*>::iterator it;
	CHttpServer* pServer = NULL;
	int num = 0;
	for(it = m_vctServer.begin(); it!= m_vctServer.end(); it++)
	{
		pServer = *it;
		num += pServer->GetSocketNum();
	}
	return num-1;
}