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

	// 단일 커넥션, 동기 처리 기준
	class DBConnection
	{
	private:
		static SRWLOCK s_ConnectLock;

	public:
		
		// 환경 설정의 경우에는 생성자에서 하면 좋을듯, 
		DBConnection(const char* ipAddr, WORD port, const char* userId, const char* password, const char* schema, DWORD maxQueryTime);
		~DBConnection();

		bool Connect();

		int SendFormattedQuery(const char* queryForm, ...); // 스트링 포맷에 파라미터를 가변인자로 받는다.
		int SendQuery(const char* query);
		MYSQL_RES* GetResult();
		bool FetchRow(MYSQL_ROW* outRow); // 결과에서 한 줄 가져오기
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
		MYSQL m_Connection; // 연결 구조체
		DWORD m_MaxProfileTime; // 이 값을 넘어가는 쿼리의 프로파일링 결과는 로깅을 한다.

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

