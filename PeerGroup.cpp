#include "PeerGroup.h"
#include "Log.h"

CPeerGroup*  CPeerGroup::m_instance=NULL;

CPeerGroup* CPeerGroup::Instance()
{
	if(NULL == m_instance)
	{
		m_instance = new CPeerGroup();
	}
	return m_instance;
}
void CPeerGroup::FreeInstance()
{
	if(m_instance)
	{
		delete m_instance;
		m_instance = NULL;
	}
}

CPeerGroup::CPeerGroup()
{
}
CPeerGroup::~CPeerGroup()
{
}

bool CPeerGroup::AddChannel(string id,ChannelInfo info)
{
	CAutoLock lock(m_channelsLock);
	if(m_channels.find(id) == m_channels.end())
	{
		m_channels.insert(make_pair(id,info));
		return true;
	}
	return false;
}

bool CPeerGroup::DelChannel(string id)
{
	CAutoLock lock(m_channelsLock);
	map<string,ChannelInfo>::iterator itr = m_channels.find(id);
	if(itr != m_channels.end())
	{
		m_channels.erase(itr);
		return true;
	}
	return false;
}


bool CPeerGroup::IsChannelExist(string id)
{
	CAutoLock lock(m_channelsLock);
	map<string,ChannelInfo>::iterator itr = m_channels.find(id);
	if(itr != m_channels.end())
	{
		return true;
	}
	return false;
}

bool CPeerGroup::GetChannel(string id,ChannelInfo & info)
{
	CAutoLock lock(m_channelsLock);
	map<string,ChannelInfo>::iterator itr = m_channels.find(id);
	if(itr != m_channels.end())
	{
		info.lastNum = itr->second.lastNum;
		info.status = itr->second.status;
		info.updatetime = itr->second.updatetime;
		return true;
	}
	return false;
}

bool CPeerGroup::UpdateChannel(string id,ChannelInfo & info)
{
	CAutoLock lock(m_channelsLock);
	map<string,ChannelInfo>::iterator itr = m_channels.find(id);
	if(itr != m_channels.end())
	{
		itr->second.lastNum = info.lastNum;
		itr->second.status = info.status;
		itr->second.updatetime = time(NULL);
		return true;
	}
	return false;
}

bool CPeerGroup::IncreChannelNum(string id)
{
	CAutoLock lock(m_channelsLock);
	map<string,ChannelInfo>::iterator itr = m_channels.find(id);
	if(itr != m_channels.end())
	{
		if(0 == itr->second.lastNum)
		{
			itr->second.lastNum = (unsigned long long)time(NULL);
		}
		else
		{
			itr->second.lastNum++;
		}

		itr->second.updatetime = time(NULL);
		return true;
	}
	return false;
}

unsigned long long CPeerGroup::GetChannelLastNum(string id)
{
	CAutoLock lock(m_channelsLock);
	map<string,ChannelInfo>::iterator itr = m_channels.find(id);
	if(itr != m_channels.end())
	{
		return itr->second.lastNum;
	}
	return 0;
}

void CPeerGroup::DelTimeoutChannel()
{
	CAutoLock lock(m_channelsLock);
	time_t curtime = time(NULL);
	map<string,ChannelInfo>::iterator itr = m_channels.begin();
	while(m_channels.end() != itr)
	{
		if(itr->second.updatetime + 50 < curtime)
		{
			CLog::getInstance()->info("del timeout channel %s",itr->first.c_str());
			m_channels.erase(itr++);
			continue;
		}
		itr++;
	}
}

bool CPeerGroup::GetAllChannel(vector < string >& ids)
{
	CAutoLock lock(m_channelsLock);
	map<string,ChannelInfo>::iterator itr = m_channels.begin();
	while(m_channels.end() != itr)
	{
		ids.push_back(itr->first);
		itr++;
	}
	return true;
}

