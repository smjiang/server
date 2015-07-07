#include "Db.h"
#include "Log.h"

CDbInterface* CDbInterface::m_instance = NULL;

CDbInterface::CDbInterface()
{
	m_sql = NULL;
	m_port = 3306;
	m_host = "127.0.0.1";

	m_dbname = "smartoy";
	m_usertablename = "users";
	m_wx2usertablename = "wx2user";
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
		return false;
	}
	if(!mysq_real_connect(m_sql,m_host.c_str(),m_usename.c_str(),m_passwd.c_str(),NULL,m_port,NULL,0))
	{
		CLog::getInstance()->error("connect db fail %s",mysql_error(m_sql));
		delete m_sql;
		return false;
	}
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
bool CDbInterface::CreateDB(char * dbname)
{
	char buf[256] = {0};
	sprintf(buf, "create database %s", dbname);
	unsigned int ret = mysql_query(m_sql, buf);
	if(0 != ret)
	{
		CLog::getInstance()->error("create db %s fail: %s",dbname,mysql_error(m_sql));
		return false;
	}
	ret = mysql_select_db(m_sql, dbname);	
	if (ret != 0)	
	{
		CLog::getInstance()->error("use db %s fail: %s",dbname,mysql_error(m_sql));
		return false;	
	}
	return true;
}

bool CDbInterface::CreateUserTable()
{
	char buf[1024];	
	memset(buf, 0, sizeof(buf));
	sprintf(buf, "create table %s (equipmentID VARCHAR(64) NOT NULL PRIMARY KEY, username VARCHAR(255), createtime int)", m_usertablename.c_str());
	int ret = mysql_query(m_sql, buf);
	if(0 != ret)
	{
		CLog::getInstance()->error("create usertable fail: %s",mysql_error(m_sql));
		return false;
	}
	return true;
}
bool CDbInterface::InsertUserTable(char* equipmentID, char* username)
{
	char buf[1024];	
	memset(buf, 0, sizeof(buf));
	unsigned int curtime = (unsigned int)time(NULL);
	sprintf(buf, "INSERT INTO %s (equipmentID VARCHAR(64) NOT NULL PRIMARY KEY, username VARCHAR(255), createtime int) VALUES(%s,%s,%u)", m_usertablename.c_str(),equipmentID,username,curtime);
	int ret = mysql_query(m_sql, buf);
	if(0 != ret)
	{
		CLog::getInstance()->error("insert usertable fail: %s",mysql_error(m_sql));
		return false;
	}
	return true;
}

bool CDbInterface::CreateWX2UserTable()
{
	char buf[1024];	
	memset(buf, 0, sizeof(buf));
	sprintf(buf, "create table %s (openID VARCHAR(64) NOT NULL PRIMARY KEY, equipmentID VARCHAR(64) NOT NULL, createtime int)", m_wx2usertablename.c_str());
	int ret = mysql_query(m_sql, buf);
	if(0 != ret)
	{
		CLog::getInstance()->error("create weixin to user table fail: %s",mysql_error(m_sql));
		return false;
	}
	return true;
}
bool CDbInterface::InsertWX2UserTable(char* openID, char* equipmentID)
{
	char buf[1024];	
	memset(buf, 0, sizeof(buf));
	unsigned int curtime = (unsigned int)time(NULL);
	sprintf(buf, "INSERT INTO %s (openID VARCHAR(64) NOT NULL PRIMARY KEY, equipmentID VARCHAR(64) NOT NULL, createtime int) VALUES(%s,%s,%u)", m_wx2usertablename.c_str(),openID,equipmentID,curtime);
	int ret = mysql_query(m_sql, buf);
	if(0 != ret)
	{
		CLog::getInstance()->error("insert usertable fail: %s",mysql_error(m_sql));
		return false;
	}
	return true;
}
string CDbInterface::GetUserIDByWX(char* openID)
{
	char buf[1024];	
	memset(buf, 0, sizeof(buf));
	unsigned int curtime = (unsigned int)time(NULL);
	sprintf(buf, "SELECT equipmentID FROM %s WHERE openID=%s", m_wx2usertablename.c_str(),openID);
	int ret = mysql_query(m_sql, buf);
	if(0 != ret)
	{
		CLog::getInstance()->error("insert usertable fail: %s",mysql_error(m_sql));
		return NULL;
	}
	
	MYSQL_RES mysqlRes = mysql_store_result(m_sql);
	if (mysqlRes == NULL)
	{
		CLog::getInstance()->error("mysql_store_result fail %u: %s",mysql_errno(m_sql),mysql_error(m_sql));
		return NULL;
	}
 	MYSQL_ROW mysqlRow = mysql_fetch_row(mysqlRes);
	string eID = mysqlRow[0];
	mysql_free_result(mysqlRes);
	return eID;
}