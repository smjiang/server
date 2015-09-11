#ifndef _DB_H
#define _DB_H
#include <mysqld_error.h>
#include <mysql.h>
#include <string>
#include <vector>
using namespace std;

struct WXUserInfo
{
	string openID;
	string nickname;
	string bAdmin;
};
class CDbInterface
{
public:
	CDbInterface();
	~CDbInterface();
	bool Init();
	bool DBConnect();
	bool DBDisConnect();
	int CreateDB(const char* dbname);
	
	int CreateUserTable();
	int InsertUserTable(char* equipmentID, char* username);
	
	int CreateWX2UserTable();
	int InsertWX2UserTable(const char* openID, const char* equipmentID, const char* nickname, int bAdmin);//binding weixin to equipment
	int DelWX2UserTable(const char* openID);//disbinding weixin from equipment
	int GetUserIDByWX(const char* openID, string& eID);
	int GetWXByUserID(const char* eID, vector<WXUserInfo>& openIDs);
	int IsWXAdmin(const char* openID, int& isAdmin);
	int SetWXNickName(const char* openID, const char* nickname);
	int GetWXNickName(const char* openID, string& nickname);
	int SetWXAdmin(const char* openID);

	int CreateQuestionTable();
	int InsertQuestionTable(const char* question, const char* answer, int type);//type: 1 txt; 2 audio
	int GetAnswerByQuestion(const char* question, string& answer, int& type);

	int GetWXList(char* eID, vector<string>& userlist);
	
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
	string			m_questiontablename;
	static CDbInterface* m_instance;
};
#endif