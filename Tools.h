#ifndef _TOOLS_H_
#define _TOOLS_H_
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <string>
#include <sys/stat.h>
using namespace std;

int GetAddrInfo(char* host,unsigned int& ip);
void IpInt2Str(int iIp,char* strIp);
int SetNonblocking(int sock);
int MakeDir(char* dirname);
string GetValyeByKey(const char* para, const char* key);
string GetJsonValue(const char* msg,const char* key);
unsigned int GetLocalIP();

#endif
