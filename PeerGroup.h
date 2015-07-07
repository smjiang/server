#ifndef _PEER_GROUP_H
#define _PEER_GROUP_H
#include <string>
#include <time.h>
#include <map>
#include <vector>
#include "CLock.h"
using namespace std;

struct ChannelInfo
{
	unsigned long long lastNum;
	time_t	updatetime;
	char   status;
};
class CPeerGroup
{
public:
	CPeerGroup();
	~CPeerGroup();

	bool AddChannel(string id,ChannelInfo info);
	bool DelChannel(string id);
	bool IsChannelExist(string id);
	bool GetChannel(string id, ChannelInfo& info);
	bool UpdateChannel(string id, ChannelInfo& info);
	bool IncreChannelNum(string id);
	unsigned long long GetChannelLastNum(string id);
	void DelTimeoutChannel();
	bool GetAllChannel(vector<string>& ids);
	
	static CPeerGroup* Instance();
	static void FreeInstance();

private:
	CLock m_channelsLock;
	map<string,ChannelInfo> m_channels;
	static CPeerGroup*  m_instance;
};
#endif
