#include <string.h>
#include "Db.h"
#include "Log.h"
#include "IniFileParser.h"

CDbInterface* CDbInterface::m_instance = NULL;

CDbInterface::CDbInterface()
{
	m_sql = NULL;
	m_port = 3306;
	m_host = "127.0.0.1";

	m_dbname = "smartoy";
	m_usertablename = "users";
	m_wx2usertablename = "wx2user";
	m_questiontablename = "question";
}
CDbInterface::~CDbInterface()
{
}

CDbInterface* CDbInterface::Instance()
{
	if(NULL == m_instance)
	{
		m_instance = new CDbInterface();
	}
	return m_instance;
}
void CDbInterface::FreeInstance()
{
	if(m_instance)
	{
		delete m_instance;
		m_instance = NULL;
	}
}

bool CDbInterface::Init()
{
	CIniFileParser iniParser;
	int result = iniParser.init("server.ini");
	if(-1 == result)
	{
		CLog::getInstance()->error("can't open db config file,use default parameters");
	}
	else
	{
		m_host = iniParser.get_string("DATABASE:dbhost","127.0.0.1");
		CLog::getInstance()->info("read db host    %s",m_host.c_str());
		m_port = iniParser.get_int("DATABASE:dbport",3306);
		CLog::getInstance()->info("read db port    %d",m_port);
		m_username = iniParser.get_string("DATABASE:dbuser","");
		CLog::getInstance()->info("read db account %s",m_username.c_str());
		m_passwd = iniParser.get_string("DATABASE:dbpasswd","");
		CLog::getInstance()->info("read db passwd  %s",m_passwd.c_str());

		m_dbname = iniParser.get_string("DATABASE:dbname","smartoy");
		CLog::getInstance()->info("read db name    %s",m_dbname.c_str());

	}

	if(!DBConnect())
	{
		CLog::getInstance()->error("database init fail");
		return false;
	}
	
	return true;
}

bool CDbInterface::DBConnect()
{
	if(m_sql)
	{
		return false;
	}
	m_sql = new MYSQL;
	if(NULL == m_sql)
	{
		return false;
	}
	if(!mysql_init(m_sql))
	{
		CLog::getInstance()->error("init db fail %s",mysql_error(m_sql));
		delete m_sql;
		m_sql = NULL;
		return false;
	}
	if(!mysql_real_connect(m_sql,m_host.c_str(),m_username.c_str(),m_passwd.c_str(),NULL,m_port,NULL,0))
	{
		CLog::getInstance()->error("connect db fail %s",mysql_error(m_sql));
		delete m_sql;
		m_sql = NULL;
		return false;
	}
	if(CreateDB(m_dbname.c_str()) != 0)
	{
		return false;
	}
	CreateUserTable();
	CreateWX2UserTable();
	CreateQuestionTable();
	CLog::getInstance()->info("connect db %s success",m_dbname.c_str());
	return true;
}
bool CDbInterface::DBDisConnect()
{
	if(m_sql)
	{
		mysql_close(m_sql);
		delete m_sql;
		m_sql = NULL;
	}
	return true;
}
int CDbInterface::CreateDB(char * dbname)
{
	char buf[256] = {0};
	sprintf(buf, "create database if not exists %s", dbname);
	unsigned int ret = mysql_query(m_sql, buf);
	if(0 != ret)
	{
		CLog::getInstance()->error("create db %s fail %d %d: %s",dbname,ret,mysql_errno(m_sql),mysql_error(m_sql));
		return mysql_errno(m_sql);
	}
	ret = mysql_select_db(m_sql, dbname);	
	if (ret != 0)	
	{
		CLog::getInstance()->error("use db %s fail %d: %s",dbname,ret,mysql_error(m_sql));
		return mysql_errno(m_sql);	
	}
	return ret;
}

int CDbInterface::CreateUserTable()
{
	char buf[1024];	
	memset(buf, 0, sizeof(buf));
	sprintf(buf, "create table if not exists %s (equipmentID VARCHAR(64) NOT NULL, username VARCHAR(255), createtime int)", m_usertablename.c_str());
	int ret = mysql_query(m_sql, buf);
	if(0 != ret)
	{
		CLog::getInstance()->error("create usertable fail %d: %s",ret,mysql_error(m_sql));
		return mysql_errno(m_sql);
	}
	return ret;
}
int CDbInterface::InsertUserTable(char* equipmentID, char* username)
{
	char buf[1024];	
	memset(buf, 0, sizeof(buf));
	unsigned int curtime = (unsigned int)time(NULL);
	sprintf(buf, "INSERT INTO %s (equipmentID, username, createtime) VALUES('%s','%s',%u)", m_usertablename.c_str(),equipmentID,username,curtime);
	int ret = mysql_query(m_sql, buf);
	if(0 != ret)
	{
		ret = mysql_errno(m_sql);
		CLog::getInstance()->error("insert usertable fail %d: %s",ret,mysql_error(m_sql));
		return ret;
	}
	return ret;
}

int CDbInterface::GetWXList(char *eID,vector<string>& userlist)
{
	return 0;
}

int CDbInterface::CreateWX2UserTable()
{
	char buf[1024];	
	memset(buf, 0, sizeof(buf));
	sprintf(buf, "create table if not exists %s (openID VARCHAR(64) NOT NULL PRIMARY KEY, equipmentID VARCHAR(64) NOT NULL,nickname VARCHAR(255), bAdmin int, createtime int)", m_wx2usertablename.c_str());
	int ret = mysql_query(m_sql, buf);
	if(0 != ret)
	{
		CLog::getInstance()->error("create weixin to user table fail %d: %s",ret,mysql_error(m_sql));
		return mysql_errno(m_sql);
	}
	return ret;
}
int CDbInterface::InsertWX2UserTable(char* openID, char* equipmentID, char* nickname,int bAdmin)
{
	char buf[1024];	
	memset(buf, 0, sizeof(buf));
	unsigned int curtime = (unsigned int)time(NULL);
	sprintf(buf, "INSERT INTO %s (openID, equipmentID, nickname, bAdmin, createtime) VALUES('%s','%s','%s',%d,%u)", m_wx2usertablename.c_str(),openID,equipmentID,nickname,bAdmin,curtime);
	int ret = mysql_query(m_sql, buf);
	if(0 != ret)
	{
		ret = mysql_errno(m_sql);
		CLog::getInstance()->error("insert wx2user table fail %d: %s",ret,mysql_error(m_sql));
		return ret;
	}
	return ret;
}
int CDbInterface::DelWX2UserTable(char* openID)
{
	char buf[1024];	
	memset(buf, 0, sizeof(buf));
	sprintf(buf, "DELETE FROM %s WHERE openID='%s'", m_wx2usertablename.c_str(),openID);
	int ret = mysql_query(m_sql, buf);
	if(0 != ret)
	{
		ret = mysql_errno(m_sql);
		CLog::getInstance()->error("delete wx2user openID fail %d: %s",ret,mysql_error(m_sql));
		return ret;
	}
	return ret;
}
int CDbInterface::GetUserIDByWX(char* openID,string& eID)
{
	char buf[1024];	
	memset(buf, 0, sizeof(buf));
	sprintf(buf, "SELECT equipmentID FROM %s WHERE openID='%s'", m_wx2usertablename.c_str(),openID);
	int ret = mysql_query(m_sql, buf);
	if(0 != ret)
	{
		ret = mysql_errno(m_sql);
		CLog::getInstance()->error("GetUserIDByWX fail %d: %s",ret,mysql_error(m_sql));
		return ret;
	}
	MYSQL_RES* mysqlRes = mysql_store_result(m_sql);
	if (NULL == mysqlRes)
	{
		CLog::getInstance()->error("GetUserIDByWX mysql_store_result fail %u: %s",mysql_errno(m_sql),mysql_error(m_sql));
		return mysql_errno(m_sql);
	}
 	MYSQL_ROW mysqlRow = mysql_fetch_row(mysqlRes);
	if(mysqlRow)
	{
		eID = mysqlRow[0];
	}
	mysql_free_result(mysqlRes);
	return 0;
}
int CDbInterface::GetWXByUserID(char* eID,vector<WXUserInfo>& openIDs)
{
	char buf[1024];	
	memset(buf, 0, sizeof(buf));
	sprintf(buf, "SELECT openID,nickname,bAdmin FROM %s WHERE equipmentID='%s'", m_wx2usertablename.c_str(),eID);
	int ret = mysql_query(m_sql, buf);
	if(0 != ret)
	{
		ret = mysql_errno(m_sql);
		CLog::getInstance()->error("GetWXByUserID fail %d: %s",ret,mysql_error(m_sql));
		return ret;
	}
	
	MYSQL_RES* mysqlRes = mysql_store_result(m_sql);
	if (NULL == mysqlRes)
	{
		CLog::getInstance()->error("GetWXByUserID mysql_store_result fail %u: %s",mysql_errno(m_sql),mysql_error(m_sql));
		return mysql_errno(m_sql);
	}
	int num_rows = mysql_num_rows(mysqlRes);
 	MYSQL_ROW mysqlRow;
	for(int i = 0; i < num_rows; i++)
	{
		mysqlRow = mysql_fetch_row(mysqlRes);
		if(mysqlRow)
		{
			WXUserInfo userinfo;
			userinfo.openID = mysqlRow[0];
			userinfo.nickname = mysqlRow[1];
			userinfo.bAdmin = mysqlRow[2];
			openIDs.push_back(userinfo);
		}
	}
	mysql_free_result(mysqlRes);
	return 0;
}

int CDbInterface::IsWXAdmin(const char *openID, int& isAdmin)
{
	char buf[1024];	
	memset(buf, 0, sizeof(buf));
	sprintf(buf, "SELECT bAdmin FROM %s WHERE openID='%s'", m_wx2usertablename.c_str(),openID);
	int ret = mysql_query(m_sql, buf);
	if(0 != ret)
	{
		ret = mysql_errno(m_sql);
		CLog::getInstance()->error("IsWXAdmin fail %d: %s",ret,mysql_error(m_sql));
		return ret;
	}
	
	MYSQL_RES* mysqlRes = mysql_store_result(m_sql);
	if (NULL == mysqlRes)
	{
		CLog::getInstance()->error("IsWXAdmin mysql_store_result fail %u: %s",mysql_errno(m_sql),mysql_error(m_sql));
		return mysql_errno(m_sql);
	}
 	MYSQL_ROW mysqlRow = mysql_fetch_row(mysqlRes);
	if(mysqlRow)
	{
		isAdmin = mysqlRow[0];
	}
	mysql_free_result(mysqlRes);
	return 0;
}

int CDbInterface::SetWXNickName(const char *openID,const char *nickname)
{
	char buf[1024];	
	memset(buf, 0, sizeof(buf));
	sprintf(buf, "UPDATE %s SET nickname='%s' WHERE openID='%s'", m_wx2usertablename.c_str(),nickname,openID);
	int ret = mysql_query(m_sql, buf);
	if(0 != ret)
	{
		ret = mysql_errno(m_sql);
		CLog::getInstance()->error("SetWXNickName fail %d: %s",ret,mysql_error(m_sql));
		return ret;
	}
	return 0;
}

int CDbInterface::GetWXNickName(const char* openID,string& nickname)
{
	char buf[1024];	
	memset(buf, 0, sizeof(buf));
	sprintf(buf, "SELECT nickname FROM %s WHERE openID='%s'", m_wx2usertablename.c_str(),openID);
	int ret = mysql_query(m_sql, buf);
	if(0 != ret)
	{
		ret = mysql_errno(m_sql);
		CLog::getInstance()->error("GetWXNickName fail %d: %s",ret,mysql_error(m_sql));
		return ret;
	}
	
	MYSQL_RES* mysqlRes = mysql_store_result(m_sql);
	if (NULL == mysqlRes)
	{
		CLog::getInstance()->error("GetWXNickName mysql_store_result fail %u: %s",mysql_errno(m_sql),mysql_error(m_sql));
		return mysql_errno(m_sql);
	}
 	MYSQL_ROW mysqlRow = mysql_fetch_row(mysqlRes);
	if(mysqlRow)
	{
		nickname = mysqlRow[0];
	}
	mysql_free_result(mysqlRes);
	return 0;
}

int CDbInterface::SetWXAdmin(const char * openID)
{
	char buf[1024];	
	memset(buf, 0, sizeof(buf));
	sprintf(buf, "UPDATE %s SET bAdmin=1 WHERE openID='%s'", m_wx2usertablename.c_str(),openID);
	int ret = mysql_query(m_sql, buf);
	if(0 != ret)
	{
		ret = mysql_errno(m_sql);
		CLog::getInstance()->error("SetWXAdmin fail %d: %s",ret,mysql_error(m_sql));
		return ret;
	}
	return 0;
}

int CDbInterface::CreateQuestionTable()
{
	char buf[1024];	
	memset(buf, 0, sizeof(buf));
	sprintf(buf, "create table if not exists %s (question VARCHAR(128) NOT NULL PRIMARY KEY, answer VARCHAR(128) NOT NULL,type int, createtime int)", m_questiontablename.c_str());
	int ret = mysql_query(m_sql, buf);
	if(0 != ret)
	{
		CLog::getInstance()->error("create question to user table fail %d: %s",ret,mysql_error(m_sql));
		return mysql_errno(m_sql);
	}
	return ret;
}

int CDbInterface::InsertQuestionTable(char* question, char* answer,int type)
{
	char buf[1024];	
	memset(buf, 0, sizeof(buf));
	unsigned int curtime = (unsigned int)time(NULL);
	sprintf(buf, "INSERT INTO %s (question, answer, type, createtime) VALUES('%s','%s',%u,%u)", m_questiontablename.c_str(),question,answer,type,curtime);
	int ret = mysql_query(m_sql, buf);
	if(0 != ret)
	{
		ret = mysql_errno(m_sql);
		CLog::getInstance()->error("insert question table fail %d: %s",ret,mysql_error(m_sql));
		return ret;
	}
	return ret;
}

int CDbInterface::GetAnswerByQuestion(char * question,string& answer,int& type)
{
	char buf[1024];	
	memset(buf, 0, sizeof(buf));
	sprintf(buf, "SELECT answer,type FROM %s WHERE question='%s'", m_questiontablename.c_str(),question);
	int ret = mysql_query(m_sql, buf);
	if(0 != ret)
	{
		ret = mysql_errno(m_sql);
		CLog::getInstance()->error("GetAnswerByQuestion fail %d: %s",ret,mysql_error(m_sql));
		return ret;
	}
	
	MYSQL_RES* mysqlRes = mysql_store_result(m_sql);
	if (NULL == mysqlRes)
	{
		CLog::getInstance()->error("GetWXByUserID mysql_store_result fail %u: %s",mysql_errno(m_sql),mysql_error(m_sql));
		return mysql_errno(m_sql);
	}
 	MYSQL_ROW mysqlRow = mysql_fetch_row(mysqlRes);
	if(mysqlRow)
	{
		answer = mysqlRow[0];
		type = mysqlRow[1];
	}
	mysql_free_result(mysqlRes);
	return 0;
}