#pragma once

#include <MySql\include\mysql.h>
#include <MySql\include\errmsg.h>
#include <Windows.h>
#include <vector>
#include <synchapi.h>

#pragma comment(lib, "MySql\\mysqlclient.lib")

namespace onenewarm
{
	constexpr int DB_TLS_SIZE = 10;
	constexpr int MAX_QUERY_SIZE = 8192;

	// ���� Ŀ�ؼ�, ���� ó�� ����
	class DBConnection
	{
	private:
		static SRWLOCK s_ConnectLock;

	public:
		
		// ȯ�� ������ ��쿡�� �����ڿ��� �ϸ� ������, 
		DBConnection(const char* ipAddr, WORD port, const char* userId, const char* password, const char* schema, DWORD maxQueryTime);
		~DBConnection();

		bool Connect();

		int SendFormattedQuery(const char* queryForm, ...); // ��Ʈ�� ���˿� �Ķ���͸� �������ڷ� �޴´�.
		int SendQuery(const char* query);
		MYSQL_RES* GetResult();
		bool FetchRow(MYSQL_ROW* outRow); // ������� �� �� ��������
		void FreeRes()
		{
			mysql_free_result(m_Results);
		}
		DWORD GetErrCode()
		{
			return mysql_errno(&m_Connection);
		}
		


	private:
		MYSQL_RES* m_Results;
		MYSQL m_Connection; // ���� ����ü
		DWORD m_MaxProfileTime; // �� ���� �Ѿ�� ������ �������ϸ� ����� �α��� �Ѵ�.

		char m_IP[16];
		WORD m_Port;
		char m_UserId[64];
		char m_Password[64];
		char m_Schema[64];
	};


	class DBConnectionTLS
	{
	public:


		DBConnectionTLS(const char* ipAddr, WORD port, const char* userId, const char* password, const char* schema, DWORD maxQueryTime);
		~DBConnectionTLS();

		DBConnection* GetDBConnection();
	private:
		DWORD m_TlsIdx;
		std::vector<DBConnection*> m_TLSConnections;
		SRWLOCK m_Lock;
		char m_IP[16];
		WORD m_Port;
		char m_UserId[64];
		char m_Password[64];
		char m_Schema[64];
		DWORD m_MaxProfileTime;
	};
}

