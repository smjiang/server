#ifndef _DB_H
#define _DB_H
#include <mysqld_error.h>
#include <mysql.h>
#include <string>
using namespace std;

class CDbInterface
{
public:
	CDbInterface();
	~CDbInterface();
	bool Init();
	bool DBConnect();
	bool DBDisConnect();
	bool CreateDB(char* dbname);
	
	bool CreateUserTable();
	bool InsertUserTable(char* equipmentID, char* username);
	
	bool CreateWX2UserTable();
	bool InsertWX2UserTable(char* openID, char* equipmentID);
	string GetUserIDByWX(char* openID);
	
	static CDbInterface* Instance();
	static void FreeInstance();
private:
	MYSQL*			m_sql;
	unsigned short  m_port;
	string			m_host;
	string			m_usename;
	string			m_passwd;

	string			m_dbname;
	string			m_usertablename;
	string			m_wx2usertablename;//weixin to user 
	static CDbInterface* m_instance;
};
#endif