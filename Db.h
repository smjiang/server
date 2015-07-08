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
	int CreateDB(char* dbname);
	
	int CreateUserTable();
	int InsertUserTable(char* equipmentID, char* username);
	
	int CreateWX2UserTable();
	int InsertWX2UserTable(char* openID, char* equipmentID);//binding weixin to equipment
	int DelWX2UserTable(char* openID);//disbinding weixin from equipment
	int GetUserIDByWX(char* openID, string& eID);
	
	static CDbInterface* Instance();
	static void FreeInstance();
private:
	MYSQL*			m_sql;
	unsigned short  m_port;
	string			m_host;
	string			m_username;
	string			m_passwd;

	string			m_dbname;
	string			m_usertablename;
	string			m_wx2usertablename;//weixin to user 
	static CDbInterface* m_instance;
};
#endif