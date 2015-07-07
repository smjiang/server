#include "IniFileParser.h"
#include "iniparser.h"


CIniFileParser::CIniFileParser(void)
{
	m_dict = NULL;
}


CIniFileParser::~CIniFileParser(void)
{
	if (m_dict != NULL)
	{
		this->close();
	}
}

int CIniFileParser::init(const char *path)
{
	int result = 0;
	strncpy(m_iniPath,path,sizeof(m_iniPath));
	m_dict = iniparser_load(path);
	if (m_dict == NULL)
	{
		result = -1;
	}
	return result;
}

int CIniFileParser::get_int(const char *key,int defaultVal)
{
	int result = defaultVal;
	if (m_dict != NULL)
	{
		result = iniparser_getint(m_dict,key,defaultVal);
	}
	return result;
}

char *CIniFileParser::get_string(const char *key,char *defaultVal)
{
	char *result = defaultVal;
	if (m_dict != NULL)
	{
		result = iniparser_getstring(m_dict,key,defaultVal);
	}
	return result;
}

void CIniFileParser::reload()
{
	if (m_dict != NULL)
	{
		iniparser_freedict(m_dict);
		m_dict = NULL;
	}
	m_dict = iniparser_load(m_iniPath);
}

void CIniFileParser::close()
{
	iniparser_freedict(m_dict);
	m_dict = NULL;
}
