#pragma once

#include "dictionary.h"

class CIniFileParser
{
public:
	CIniFileParser(void);
	~CIniFileParser(void);
public:
	int init(const char *path);
	int get_int(const char *key,int defaultVal);
	char *get_string(const char *key,char *defaultVal);
	void reload();
	void close();
private:
	dictionary *m_dict;
	char m_iniPath[255];
};

