#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "httpserver.h"
#include "NetworkInterface.h"
#include "Log.h"
int g_debug = 0;
CHttpServerMgr g_httpServer;


void signal_handler(int sig);
bool mydaemonize(void);

int main(int argc,char *argv[])
{
	CLog::getInstance()->print("fileserver start");
	CLog::getInstance()->init("log");
	if (argc == 1 && !mydaemonize())
	{
		CLog::getInstance()->print("mydeamonize failed!");
		return 1;
	}

	if (argc > 1)
	{
		g_debug = 1;
	}
	int result = 0;

	//ÐÅºÅ´¦Àí
	signal(SIGPIPE,SIG_IGN);
	signal(SIGSEGV,SIG_IGN);
	signal(SIGTRAP,SIG_IGN);
	signal(SIGUSR1,signal_handler);
	signal(SIGTERM,signal_handler);
	signal(SIGINT,signal_handler);
	signal(SIGHUP,signal_handler);
	signal(SIGCHLD,signal_handler);
	signal(SIGQUIT,signal_handler);

	g_httpServer.init(8088);
	if(g_httpServer.run() == 2)
	{
		CLog::getInstance()->error("fileserver run fail");
		return 0;
	}

	if(CNetworkInterface::Instance()->Init() < 0)
	{
		CLog::getInstance()->error("network init fail");
		return 0;
	}
	
	while(true)
	{
		pause();
	}
	CLog::getInstance()->debug("fileserver exited!");
	return result;
}

void signal_handler(int sig)
{
	CLog::getInstance()->debug("server received a signal: %d",sig);
	switch (sig)
	{
	case SIGTERM:
	case SIGINT:
	case SIGKILL:
	case SIGQUIT:
	case SIGHUP:
		g_httpServer.stop();
		CNetworkInterface::Instance()->Stop();
		exit(0);
		break;
	case SIGALRM:
		break;
	case SIGUSR1:
		break;
	default:
		break;
	}
} 

bool mydaemonize(void)
{
	fflush(stdout);
	fflush(stderr);

	pid_t pid = fork();	
	switch(pid)
	{
	case -1:
		{
			CLog::getInstance()->print("%s:%d,error: %d",__FUNCTION__,__LINE__,errno);
			return false;
		}
	case 0:
		{
			break;
		}
	default:
		{
			_exit(0);
		}
	}

	if(setsid() == -1)
	{
		CLog::getInstance()->print("%s:%d,error: %d",__FUNCTION__,__LINE__,errno);
		return false;
	}

	switch(fork())
	{
	case -1:
		{
			CLog::getInstance()->print("%s:%d,error: %d",__FUNCTION__,__LINE__,errno);
			return false;
		}
	case 0:
		{
			break;
		}
	default:
		{
			_exit(0);
		}
	}

	umask(0);

	close(0);
	close(1);
	close(2);

	int fd = open("/dev/null", O_RDWR, 0);
	if(fd != -1)
	{
		dup2(fd, 0);
		dup2(fd, 1);
		dup2(fd, 2);

		if(fd > 2)
		{
			close(fd);
		}
	}
	return true;
}

