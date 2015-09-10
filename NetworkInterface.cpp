#include <unistd.h>
#include <errno.h>
#include "NetworkInterface.h"
#include "IniFileParser.h"
#include "Log.h"
#include "Tools.h"
#include "Json/json.h"
#include "PeerGroup.h"

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
	m_tcplistenport = 8088;
	m_threadNum = 1;
	m_httpClientSock = -1;
	m_httpServerSock = -1;
	
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
	if(pthread_create(&m_threadMsg,NULL,SendMsg2WXThread,this) != 0)
	{
		return -1;
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
		info.datapos = 0;
		info.recvbuf[0] = '\0';
		info.ip = addr.sin_addr.s_addr;
		info.port = addr.sin_port;
		info.updatetime = time(NULL);
		info.deviceID = "";
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
		CloseSock(sock);
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
	if(info.datapos < 4)
	{
		return;
	}

	const char* respFmt = "{\"errcode\":\"%d\",\"errmsg\":\"%s\",\"data\":[%s]}";
	const char* sendfmt = "HTTP/1.1 200 OK\r\nContent-Type: text/json\r\nContent-Length: %d\r\nConnection: keep-alive\r\n\r\n%s";
	char* buf = info.recvbuf;
	MsgHead* head = (MsgHead*)buf;
	if(0x0101 == htons(head->cmd))
	{
		int len = htons(head->len);
		if(info.datapos < len+4)
		{
			CLog::getInstance()->info("pid %d sock %d recvfrom equipment,msg not finish:%d,recved %d",pid,sock,len,info.datapos);
			return;
		}

		char respBuf[1024] = {0};
		MsgHead* respHead = (MsgHead*)respBuf;
		char* pResp = respBuf + 4;
		respHead->cmd = htons(0x0101);
		const char* respJson = "{\"cmd\":\"%s\",\"result\":\"%s\",\"code\":\"%d\"}";
		unsigned short jsonLen = 0;
		Json::Reader reader;
		Json::Value value;
		if(reader.parse(buf+4,value,false))
		{
			Json::Value cmd = value.get("cmd","");
			string strCmd = cmd.toStyledString_t();
			Json::Value deviceID = value.get("device_id","");
			string strDeviceID = deviceID.toStyledString_t();
			if("login" == strCmd)
			{
				CLog::getInstance()->info("msg from equipment : %s,device id %s",strCmd.c_str(),strDeviceID.c_str());
				info.deviceID = strDeviceID;
				
				ChannelInfo cInfo;
				cInfo.sock = sock;
				cInfo.updatetime = time(NULL);
				if(CPeerGroup::Instance()->IsChannelExist(strDeviceID))
				{
					CPeerGroup::Instance()->UpdateChannel(strDeviceID,cInfo);
					snprintf(pResp,1000,respJson,strCmd.c_str(),"fail",1);
				}
				else
				{
					CPeerGroup::Instance()->AddChannel(strDeviceID,cInfo);
					snprintf(pResp,1000,respJson,strCmd.c_str(),"ok",0);
				}
				jsonLen = strlen(pResp);
				respHead->len = htons(jsonLen);
				send(sock,respBuf,jsonLen+4,0);
				CLog::getInstance()->info("send %s",respBuf+4);
			}
			else if("send_voice" == strCmd)
			{
				Json::Value voiceurl = value.get("voice_uri","");
				string strVoiceUrl = voiceurl.toStyledString_t();
				Json::Value valueContactID = value.get("contact_id","");
				string strContactID = valueContactID.toStyledString_t();
				vector<WXUserInfo> openIDs;
				CLog::getInstance()->info("msg from equipment : %s,deviceID %s,contactID %s,voiceurl %s",strCmd.c_str(),strDeviceID.c_str(),strContactID.c_str(),strVoiceUrl.c_str());
				ret = ProcessGetWXByDeviceID(strDeviceID.c_str(),openIDs);
				if(0 == ret || openIDs.size() > 0)
				{
					vector<WXUserInfo>::iterator itr = openIDs.begin();
					while(openIDs.end() != itr)
					{
						if((*itr).nickname == strContactID)
						{
							break;
						}
						itr++;
					}
					if(openIDs.end() != itr)
					{
						snprintf(pResp,1000,respJson,strCmd.c_str(),"ok",0);
						IMMSG immsg;
						immsg.openID = (*itr).openID;//"o0ojGwbrZZtuGknGZvMhrtbOOOos";//openID;
						immsg.voiceUrl = strVoiceUrl;
						m_msgQueLock.Lock();
						m_msgQue.push(immsg);
						m_msgQueLock.Unlock();
					}
					else
					{
						snprintf(pResp,1000,respJson,strCmd.c_str(),"fail",3);
					}
				}
				else if(openIDs.size() == 0)
				{
					snprintf(pResp,1000,respJson,strCmd.c_str(),"fail",2);
				}
				else
				{
					snprintf(pResp,1000,respJson,strCmd.c_str(),"fail",1);
				}
				jsonLen = strlen(pResp);
				respHead->len = htons(jsonLen);
				int sendres = send(sock,respBuf,jsonLen+4,0);
				CLog::getInstance()->info("send len %d: %s,%d",sendres,respBuf+4,errno);
			}
			else if("play_voice" == strCmd || "play_music" == strCmd)
			{
				CLog::getInstance()->info("msg from equipment response : %s",strCmd.c_str());
			}
			else if("play_status" == strCmd || "list_file" == strCmd)
			{
				CLog::getInstance()->info("msg from equipment : %s",strCmd.c_str());
				if(m_httpServerSock > 0)
				{
					snprintf(respBuf,respFmt,0,"Success",buf+4);
					char sendbuf[1024] = {0};
					snprintf(sendbuf,sendfmt, strlen(respBuf),respBuf);
					ret = send(m_httpServerSock, sendbuf, strlen(sendbuf), 0);
					m_httpServerSock = -1;
					CLog::getInstance()->info("send weixin %s %d Byte: %s",strCmd.c_str(),ret,respBuf);
				}
			}
			else if("question_answer" == strCmd)
			{
				Json::Value valueSeqNum = value.get("seq_num","");
				string strSeqNum = valueSeqNum.toStyledString_t();
				Json::Value valueQuestion = value.get("question","");
				string strQuestion = valueQuestion.toStyledString_t();
				string strAnswer;
				int type;
				ProcessGetAnswer(strQuestion.c_str(),strAnswer,type);
				const char* sndFmt = "{\"cmd\":\"question_answer\",\"device_id\":\"%s\",\"seq_num\":\"%s\",\"type\":\"%s\",\"result\":\"%s\"}";
				snprintf(pResp,1000,sndFmt,strDeviceID.c_str(),strSeqNum.c_str(),"uri",strAnswer.c_str());
				jsonLen = strlen(pResp);
				respHead->len = htons(jsonLen);
				ret = send(sock,respBuf,jsonLen+4,0);
				CLog::getInstance()->info("%s send device %d: %s",strCmd.c_str(),ret,pResp);
			}
			else if("get_contact" == strCmd)
			{
				vector<WXUserInfo> openIDs;
				ProcessGetWXByDeviceID(strDeviceID.c_str(),openIDs);
				const char* idFmt = "{\"openID\":\"%s\",\"nickname\":\"%s\"}";
				char idbuf[1024] = {0};
				vector<WXUserInfo>::iterator itr = openIDs.begin();
				if(openIDs.end() != itr)
				{
					snprintf(idbuf,idFmt,(*itr).openID.c_str(),(*itr).nickname.c_str());
					itr++;
				}
				while(openIDs.end() != itr)
				{
					strcat(idbuf, ",");
					snprintf(idbuf+strlen(idbuf),idFmt,(*itr).openID.c_str(),(*itr).nickname.c_str());
					itr++;
				}
				const char* sndFmt = "{\"cmd\":\"get_contact\",\"device_id\":\"%s\",\"result\":[%s]}";
				snprintf(pResp,1000,sndFmt,strDeviceID.c_str(),idbuf);
				jsonLen = strlen(pResp);
				respHead->len = htons(jsonLen);
				ret = send(sock,respBuf,jsonLen+4,0);
				CLog::getInstance()->info("get_contact send device %d,errno %d: %s",ret,errno,pResp);
			}
			else if("set_vol" == strCmd || "power_off" == strCmd)
			{
				CLog::getInstance()->info("msg from equipment response : %s",strCmd.c_str());
			}
			else
			{
				CLog::getInstance()->error("msg from equipment unknown cmd %s",strCmd.c_str());
				snprintf(pResp,1000,respJson,strCmd.c_str(),"fail",1);
				jsonLen = strlen(pResp);
				respHead->len = htons(jsonLen);
				send(sock,respBuf,jsonLen+4,0);
			}
		}
		else
		{
			CLog::getInstance()->error("msg from equipment format invalid: %s",buf+4);
			snprintf(pResp,1000,respJson,"unknown","fail",1);
			jsonLen = strlen(pResp);
			respHead->len = htons(jsonLen);
			send(sock,respBuf,jsonLen+4,0);
		}
		int leftlen = info.datapos-len-4;
		if(leftlen > 0)
		{
			memmove(info.recvbuf,info.recvbuf+info.datapos,leftlen);
		}
		info.datapos = leftlen;
		info.recvbuf[leftlen] = '\0';
		return;
	}
	char* pos = strstr(buf,"\r\n\r\n");
	if(NULL == pos)
	{
		if(info.datapos > 4096)
		{
			CloseSock(sock);
		}
		return;
	}
	const char* errmsg = "HTTP/1.1 404 Not Found\r\nContent-Type: text/plain\r\nContent-Length: 13\r\nConnection: close\r\n\r\n404 Not Found";
	if(memcmp(buf,"GET ",4) == 0 || memcmp(buf,"POST ",5) == 0)
	{
		if(pos)
		{
			pos += 4;
			//pos = "{\"openId\":\"asdfasdf\",\"msgType\":\"operation\",\"msgContent\":\"12312312\",\"operationType\":\"bind\"}";
			Json::Reader reader;
			Json::Value value;
			if(reader.parse(pos,value,false))
			{
				ret = 0;
				char respBuf[1024] = {0};
				Json::Value openID = value.get("openId","");
				string strOpenID = openID.toStyledString_t();
				Json::Value msgtype = value.get("msgType","");
				string strMsgType = msgtype.toStyledString_t();
				Json::Value msgcontent = value.get("msgContent","");
				string strMsgContent = msgcontent.toStyledString_t();
				CLog::getInstance()->info("get openID %s, msgType %s, msgContent %s",strOpenID.c_str(),strMsgType.c_str(),strMsgContent.c_str());
				Json::Value deviceID = value.get("deviceID","");
				string strDeviceID = deviceID.toStyledString_t();
				if("txt" == strMsgType)
				{
				}
				else if("audio" == strMsgType)
				{
					Json::Value audiourl = value.get("url","");
					string strAudioUrl = audiourl.toStyledString_t();
					int ret = CDbInterface::Instance()->GetUserIDByWX(strOpenID.c_str(),strDeviceID);
					if(0 == ret)
					{
						const char* audioJsonFmt = "{\"cmd\":\"play_voice\",\"device_id\":\"%s\",\"contact_id\":\"%s\",\"voice_uri\":\"%s\"}";
						char sendAudioBuf[1024] = {0};
						string nickname = "¸¸Ä¸";
						ProcessGetNickName(strOpenID.c_str(),nickname);
						snprintf(sendAudioBuf,1023,audioJsonFmt,strDeviceID.c_str(),nickname.c_str(),strAudioUrl.c_str());

						SendMsgToDevice(strDeviceID.c_str(),sendAudioBuf);
					}
					else
					{
						CLog::getInstance()->info("send audio fail : no bind device");
					}
					strMsgContent = "";
				}
				else if("operation" == strMsgType)
				{
					Json::Value operationType = value.get("operationType","");
					string strOperationType = operationType.toStyledString_t();
					if("bind" == strOperationType)
					{
						vector<WXUserInfo> users;
						ProcessGetWXByDeviceID(strDeviceID.c_str(),users);
						int bAdmin = 0;
						if(users.size() == 0)
						{
							bAdmin = 1;
						}
						ret = ProcessBind(strOpenID.c_str(),strDeviceID.c_str(),strMsgContent.c_str(), bAdmin);
						strMsgContent = "";
					}
					else if("unbind" == strOperationType)
					{
						ret = ProcessUnbind(strOpenID.c_str());
						strMsgContent = "";
					}
					else if("bindlist" == strOperationType)
					{
						ret = ProcessGetBind(strOpenID.c_str(),strMsgContent);
						string tmp = "\"";
						tmp += strMsgContent;
						tmp += "\"";
						strMsgContent = tmp;
					}
					else if("contentPlay" == strOperationType)
					{
						Json::Value audiourl = value.get("url","");
						string strAudioUrl = audiourl.toStyledString_t();
						ret = CDbInterface::Instance()->GetUserIDByWX(strOpenID.c_str(),strDeviceID);
						if(0 == ret)
						{
							const char* audioJsonFmt = "{\"cmd\":\"play_music\",\"device_id\":\"%s\",\"voice_uri\":\"%s\"}";
							char sendPlayBuf[1024] = {0};
							snprintf(sendPlayBuf,1023,audioJsonFmt,strDeviceID.c_str(),strAudioUrl.c_str());

							if(SendMsgToDevice(strDeviceID.c_str(),sendPlayBuf) < 0)
							{
								ret = -3;
							}
						}
						else
						{
							CLog::getInstance()->info("send play music fail : no bind device");
						}
						strMsgContent = "";
					}
					else if("userlist" == strOperationType)//query weixin users binding the same toy
					{
						vector<WXUserInfo> openIDs;
						ret = ProcessGetWXByDeviceID(strDeviceID.c_str(),openIDs);
						vector<WXUserInfo>::iterator itr = openIDs.begin();
						char idBuf[1024] = {0};
						strMsgContent = "";
						const char* idFmt = "{\"openID\":\"%s\",\"nickname\":\"%s\",\"bAdmin\":\"%s\"}";
						if(openIDs.end() != itr)
						{
							snprintf(idBuf,sizeof(idBuf), idFmt, (*itr).openID.c_str(), (*itr).nickname.c_str(), (*itr).bAdmin.c_str());
							strMsgContent = idBuf;
							itr++;
						}
						while(openIDs.end() != itr)
						{
							snprintf(idBuf,sizeof(idBuf), idFmt, (*itr).openID.c_str(), (*itr).nickname.c_str(), (*itr).bAdmin.c_str());
							strMsgContent += ",";
							strMsgContent += idBuf;
							itr++;
						}
					}
					else if("deleteuser" == strOperationType)//administrator delete other weixin user
					{
						if(IsWXUserAdmin(strOpenID.c_str()))
						{
							ret = ProcessDelWXUser(strMsgContent.c_str());
						}
						else
						{
							ret = -2;
						}
						strMsgContent = "";
					}
					else if("setuser" == strOperationType)//set weixin user to be administrator
					{
						if(IsWXUserAdmin(strOpenID.c_str()))
						{
							ret = ProcessSetWXUserAdmin(strMsgContent.c_str());
						}
						else
						{
							ret = -2;
						}
						strMsgContent = "";
					}
					else if("setname" == strOperationType)//set weixin user's nickname
					{
						ret = ProcessSetWXUserNickName(strOpenID.c_str(),strMsgContent.c_str());
						strMsgContent = "";
					}
					else if("setvoice" == strOperationType)
					{
						if(strDeviceID.c_str() == 0)
						{
							CDbInterface::Instance()->GetUserIDByWX(strOpenID.c_str(),strDeviceID);
						}
						if(strDeviceID.c_str() > 0)
						{
							const char* jsonFmt = "{\"cmd\":\"set_vol\",\"device_id\":\"%s\",\"volume\":\"%s\"}";
							char sendVolBuf[1024] = {0};
							snprintf(sendVolBuf,1023,jsonFmt,strDeviceID.c_str(),strMsgContent.c_str());

							if(SendMsgToDevice(strDeviceID.c_str(),sendVolBuf) < 0)
							{
								ret = -3;
							}
						}
						strMsgContent = "";
					}
					else if("playstatus" == strOperationType)
					{
						const char* jsonFmt = "{\"cmd\":\"play_status\",\"device_id\":\"%s\"}";
						char statusBuf[256] = {0};
						snprintf(statusBuf,255,jsonFmt,strDeviceID.c_str());
						if(SendMsgToDevice(strDeviceID.c_str(),statusBuf) > 0)
						{
							info.datapos = 0;
							info.recvbuf[0] = '\0';
							m_httpServerSock = sock;
							return;
						}
						ret = -3;//query fail
						strMsgContent = "";
					}
					else if("defineQA" == strOperationType)
					{
						Json::Value audiourl = value.get("url","");
						string strAudioUrl = audiourl.toStyledString_t();
						int answerType = 1;//txt
						size_t pos = strAudioUrl.find("http://");
						if(string::npos != pos)
						{
							answerType = 2;//audio 
						}
						ret = ProcessInsertQuestion(strMsgContent.c_str(),strAudioUrl.c_str(),answerType);
						strMsgContent = "";
					}
					else if("listfile" == strOperationType)
					{
						ret = CDbInterface::Instance()->GetUserIDByWX(strOpenID.c_str(),strDeviceID);
						if(0 == ret)
						{
							const char* jsonFmt = "{\"cmd\":\"list_file\",\"device_id\":\"%s\",\"page_num\":\"%s\"}";
							char sendListBuf[1024] = {0};
							snprintf(sendListBuf,1023,jsonFmt,strDeviceID.c_str(),strMsgContent.c_str());

							if(SendMsgToDevice(strDeviceID.c_str(),sendListBuf) > 0)
							{
								info.datapos = 0;
								info.recvbuf[0] = '\0';
								m_httpServerSock = sock;
								return;
							}
							ret = -3;
						}
						else
						{
							CLog::getInstance()->info("send list file fail : no bind device");
						}
						strMsgContent = "";			
					}
					else if("poweroff" == strOperationType)
					{
						if(strDeviceID.c_str() == 0)
						{
							CDbInterface::Instance()->GetUserIDByWX(strOpenID.c_str(),strDeviceID);
						}
						if(strDeviceID.c_str() > 0)
						{
							const char* jsonFmt = "{\"cmd\":\"power_off\",\"device_id\":\"%s\"}";
							char sendOffBuf[1024] = {0};
							snprintf(sendOffBuf,1023,jsonFmt,strDeviceID.c_str());

							if(SendMsgToDevice(strDeviceID.c_str(),sendOffBuf) < 0)
							{
								ret = -3;
							}
						}
						strMsgContent = "";
					}
					else
					{
						ret = 2;//unknown Operation
					}
				}
				else
				{
					ret = 1;//unknown msgType
					CLog::getInstance()->info("Unknown msgType %s",strMsgType.c_str());
				}
				if(0 == ret)
				{
					snprintf(respBuf,1024,respFmt,ret,"Success",strMsgContent.c_str());
				}
				else if(-1 == ret)
				{
					snprintf(respBuf,1024,respFmt,ret,"Repeated Bind","");	
				}
				else if(-2 == ret)
				{
					snprintf(respBuf,1024,respFmt,ret,"Not Administrator","");
				}
				else if(-3 == ret)
				{
					snprintf(respBuf,1024,respFmt,ret,"Device Not Online","");
				}
				else
				{
					snprintf(respBuf,1024,respFmt,ret,"Fail","Unknown Operation");
				}
				
				char sendbuf[1024] = {0};
				snprintf(sendbuf,1024,sendfmt,strlen(respBuf),respBuf);
				send(sock,sendbuf,strlen(sendbuf),0);
				CLog::getInstance()->info("send weixin response %s",respBuf);
				info.datapos = 0;
				info.recvbuf[0] = '\0';
				return;
			}
		}
	}
	send(sock,errmsg,strlen(errmsg),0);
	CLog::getInstance()->info("pid %d send errmsg %s",pid,errmsg);
	info.datapos = 0;
	info.recvbuf[0] = '\0';
}

int CNetworkInterface::ProcessBind(const char * openID,const char * equipmentID,const char* nickname,int bAdmin)
{
	int ret = CDbInterface::Instance()->InsertWX2UserTable((char*)openID,equipmentID,nickname,bAdmin);
	if(0 == ret)
	{
		//insert success
		return ret;
	}
	else if(1062 == ret)
	{
		//already exist
		return -1;
	}
	else if(2006 == ret || 2013 == ret)
	{
		//mysql disconnect
		CDbInterface::Instance()->DBDisConnect();
		if(CDbInterface::Instance()->DBConnect())
		{
			ret = CDbInterface::Instance()->InsertWX2UserTable((char*)openID,equipmentID,nickname,bAdmin);
			if(0 == ret)
			{
				//insert success
			}
		}
	}
	return ret;
}
int CNetworkInterface::ProcessUnbind(const char * openID)
{
	int ret = CDbInterface::Instance()->DelWX2UserTable((char*)openID);
	if(0 == ret)
	{
		//success
		return ret;
	}
	else if(2006 == ret || 2013 == ret)
	{
		//mysql disconnect
		CDbInterface::Instance()->DBDisConnect();
		if(CDbInterface::Instance()->DBConnect())
		{
			ret = CDbInterface::Instance()->DelWX2UserTable((char*)openID);
		}
	}
	return ret;
}
int CNetworkInterface::ProcessGetBind(const char * openID,string& uuid)
{
	int ret = CDbInterface::Instance()->GetUserIDByWX((char*)openID,uuid);
	if(0 == ret)
	{
		//success
		return ret;
	}
	else if(2006 == ret || 2013 == ret)
	{
		//mysql disconnect
		CDbInterface::Instance()->DBDisConnect();
		if(CDbInterface::Instance()->DBConnect())
		{
			ret = CDbInterface::Instance()->GetUserIDByWX((char*)openID,uuid);
		}
	}
	return ret;
}
int CNetworkInterface::ProcessGetWXByDeviceID(const char* eID,vector<WXUserInfo>& openIDs)
{
	int ret = CDbInterface::Instance()->GetWXByUserID((char*)eID,openIDs);
	if(0 == ret)
	{
		//success
		return ret;
	}
	else if(2006 == ret || 2013 == ret)
	{
		//mysql disconnect
		CDbInterface::Instance()->DBDisConnect();
		if(CDbInterface::Instance()->DBConnect())
		{
			ret = CDbInterface::Instance()->GetWXByUserID((char*)eID,openIDs);
		}
	}
	return ret;
}

bool CNetworkInterface::IsWXUserAdmin(const char *openID)
{
	int admin = 0;
	int ret = CDbInterface::Instance()->IsWXAdmin(openID, admin);
	if(2006 == ret || 2013 == ret)
	{
		//mysql disconnect
		CDbInterface::Instance()->DBDisConnect();
		if(CDbInterface::Instance()->DBConnect())
		{
			ret = CDbInterface::Instance()->IsWXAdmin(openID, admin);
		}
	}
	if(0 == ret)
	{
		return admin>0?true:false;
	}
	return false;

}

int CNetworkInterface::ProcessDelWXUser(const char *openID)
{
	int ret = CDbInterface::Instance()->DelWX2UserTable(openID);
	if(0 == ret)
	{
		//success
		return ret;
	}
	else if(2006 == ret || 2013 == ret)
	{
		//mysql disconnect
		CDbInterface::Instance()->DBDisConnect();
		if(CDbInterface::Instance()->DBConnect())
		{
			ret = CDbInterface::Instance()->DelWX2UserTable(openID);
		}
	}
	return ret;
}

int CNetworkInterface::ProcessSetWXUserAdmin(const char *openID)
{
	int ret = CDbInterface::Instance()->SetWXAdmin(openID);
	if(0 == ret)
	{
		//success
		return ret;
	}
	else if(2006 == ret || 2013 == ret)
	{
		//mysql disconnect
		CDbInterface::Instance()->DBDisConnect();
		if(CDbInterface::Instance()->DBConnect())
		{
			ret = CDbInterface::Instance()->SetWXAdmin(openID);
		}
	}
	return ret;

}

int CNetworkInterface::ProcessSetWXUserNickName(const char *openID, const char* nickname)
{
	int ret = CDbInterface::Instance()->SetWXNickName(openID, nickname);
	if(0 == ret)
	{
		//success
		return ret;
	}
	else if(2006 == ret || 2013 == ret)
	{
		//mysql disconnect
		CDbInterface::Instance()->DBDisConnect();
		if(CDbInterface::Instance()->DBConnect())
		{
			ret = CDbInterface::Instance()->SetWXNickName(openID, nickname);
		}
	}
	return ret;

}

int CNetworkInterface::ProcessGetNickName(const char *openID, string& nickname)
{
	int ret = CDbInterface::Instance()->GetWXNickName(openID, nickname);
	if(0 == ret)
	{
		//success
		return ret;
	}
	else if(2006 == ret || 2013 == ret)
	{
		//mysql disconnect
		CDbInterface::Instance()->DBDisConnect();
		if(CDbInterface::Instance()->DBConnect())
		{
			ret = CDbInterface::Instance()->GetWXNickName(openID, nickname);
		}
	}
	return ret;

}

int CNetworkInterface::ProcessInsertQuestion(char * question,char * answer,int type)
{
	int ret = CDbInterface::Instance()->InsertQuestionTable(question,answer,type);
	if(0 == ret)
	{
		//insert success
		return ret;
	}
	else if(1062 == ret)
	{
		//already exist
		return -1;
	}
	else if(2006 == ret || 2013 == ret)
	{
		//mysql disconnect
		CDbInterface::Instance()->DBDisConnect();
		if(CDbInterface::Instance()->DBConnect())
		{
			ret = CDbInterface::Instance()->InsertQuestionTable(question,answer,type);
			if(0 == ret)
			{
				//insert success
			}
		}
	}
	return ret;

}

int CNetworkInterface::ProcessGetAnswer(char * question,string& answer,int& type)
{
	int ret = CDbInterface::Instance()->GetAnswerByQuestion(question,answer,type);
	if(0 == ret)
	{
		//success
		return ret;
	}
	else if(2006 == ret || 2013 == ret)
	{
		//mysql disconnect
		CDbInterface::Instance()->DBDisConnect();
		if(CDbInterface::Instance()->DBConnect())
		{
			ret = CDbInterface::Instance()->GetAnswerByQuestion(question,answer,type);
		}
	}
	return ret;

}

int CNetworkInterface::SendMsgToWX(const char * openID,char * msgurl)
{
	int ret = 0;
	if(-1 == m_httpClientSock)
	{
		hostent *pHost = gethostbyname("aistoy.com");
		unsigned int ip = 0;
		if( pHost != NULL )
		{
			memcpy(&ip,pHost->h_addr_list[0],sizeof(int));
		}
		struct sockaddr_in serv;
		serv.sin_family = AF_INET;
		serv.sin_addr.s_addr = ip;
		serv.sin_port = htons(80);
	
		m_httpClientSock = socket(AF_INET, SOCK_STREAM,IPPROTO_TCP);
		if(-1 == m_httpClientSock)
		{
			return -1;
		}
		struct timeval timeout;
		timeout.tv_sec=3;
		timeout.tv_usec=0;

		setsockopt(m_httpClientSock, SOL_SOCKET,SO_RCVTIMEO, (char*)&timeout,sizeof(timeout));
		setsockopt(m_httpClientSock, SOL_SOCKET,SO_SNDTIMEO, (char*)&timeout,sizeof(timeout));
		
		ret = connect(m_httpClientSock,(struct sockaddr*)&serv,sizeof(serv));
		if(ret != 0)
		{
			close(m_httpClientSock);
			m_httpClientSock = -1;
			CLog::getInstance()->info("can NOT connect to weixin server");
			return -1;
		}
	}
	const char* fmt = "POST /welcome/aist HTTP/1.1\r\nHost: aistoy.com\r\nContent-Length: %d\r\n\r\n%s";
	const char* jsonfmt = "{\"openId\":\"%s\",\"msgType\":\"audio\",\"url\":\"%s\"}";
	char buf[1024] = {0};
	char jsonbuf[1024] = {0};
	snprintf(jsonbuf,1023,jsonfmt,openID,msgurl);
	snprintf(buf,1023,fmt,strlen(jsonbuf),jsonbuf);
	
	ret = send(m_httpClientSock,buf,strlen(buf),0);
	if(ret < 0)
	{
		close(m_httpClientSock);
		m_httpClientSock = -1;
		CLog::getInstance()->info("send to weixin server fail %d: %s",errno,buf);
		return -1;
	}
	CLog::getInstance()->info("send to weixin %s",buf);
	ret = recv(m_httpClientSock,buf,1023,0);
	if(ret <= 0)
	{
		close(m_httpClientSock);
		m_httpClientSock = -1;
		CLog::getInstance()->info("recv from weixin %d",errno);
		return -1;
	}
	close(m_httpClientSock);
	m_httpClientSock = -1;
	buf[ret] = '\0';
	CLog::getInstance()->info("recv from weixin %s",buf);
	return 0;
}

int CNetworkInterface::SendMsgToDevice(const char *deviceID,char *msgurl)
{
	ChannelInfo info;
	if(CPeerGroup::Instance()->GetChannel(deviceID,info))
	{
		int sock = info.sock;
		if(sock > 0)
		{
			MsgHead pMsgHead;
			pMsgHead.cmd = htons(0x0101);
			int jlen = strlen(msgurl);
			pMsgHead.len = htons(jlen+sizeof(pMsgHead));
			int ret = send(sock,(char*)&pMsgHead,sizeof(pMsgHead),0);
			ret = send(sock,msgurl,jlen,0);
			CLog::getInstance()->info("send audio to device sock %d len %d: %s",sock,ret,msgurl);
			return ret;
		}
		else
		{
			CLog::getInstance()->info("send audio fail : socket not exist");
			return -1;
		}
	}
	else
	{
		CLog::getInstance()->info("send audio fail : device NOT online");
		return -2;
	}
}

void CNetworkInterface::DelTimeout()
{
	map<int,SOCKINFO>::iterator itr = m_sockMap.begin();
	time_t curtime = time(NULL);
	while(m_sockMap.end() != itr)
	{
		if(itr->second.updatetime+600 < curtime)
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

void CNetworkInterface::CloseSock(int sock)
{
	epoll_ctl(m_epfd,EPOLL_CTL_DEL,sock,NULL);
	close(sock);
	CLog::getInstance()->info("close sock %d",sock);
	map<int,SOCKINFO>::iterator itr = m_sockMap.find(sock);
	if(m_sockMap.end() != itr)
	{
		CPeerGroup::Instance()->DelChannel(itr->second.deviceID);
		m_sockMap.erase(itr);
	}
}

void CNetworkInterface::DoSendMsg2WX()
{
	m_msgQueLock.Lock();
	if(m_msgQue.empty())
	{
		m_msgQueLock.Unlock();
		usleep(10000);
		return;
	}
	IMMSG& msg = m_msgQue.front();
	string openid = msg.openID;
	string url = msg.voiceUrl;
	m_msgQue.pop();
	m_msgQueLock.Unlock();
	SendMsgToWX(openid.c_str(),url.c_str());
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

void* CNetworkInterface::SendMsg2WXThread(void *para)
{
	CLog::getInstance()->info("SendMsg2WXThread start");
	CNetworkInterface* pNetwork = (CNetworkInterface*)para;
	while(pNetwork->m_status)
	{
		pNetwork->DoSendMsg2WX();
	}
	CLog::getInstance()->info("SendMsg2WXThread exit");
	return NULL;
}

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
	pthread_join(m_threadMsg,NULL);
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
