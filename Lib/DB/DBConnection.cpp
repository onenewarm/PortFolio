#include <DB\DBConnection.h>
#include <Utils\Profiler.h>
#include <Utils\Logger.h>
#include <Utils\CCrashDump.h>
#include <Utils\TextParser\TextParser.h>

using namespace onenewarm;

SRWLOCK onenewarm::DBConnection::s_ConnectLock = SRWLOCK_INIT;

onenewarm::DBConnection::DBConnection(const char* ipAddr, WORD port, const char* userId, const char* password, const char* schema, DWORD maxQueryTime) : m_Results(NULL), m_MaxProfileTime(maxQueryTime), m_Port(port)
{
	// 초기화
	if (mysql_init(&m_Connection) == NULL) {
		_LOG(LOG_LEVEL_ERROR, __FILE__, __LINE__, L"Threre was insufficient memory to allocate a new object.");
		CCrashDump::Crash();
	}

	/* For Capture Wire Shark
	unsigned int ssl_mode = SSL_MODE_DISABLED;
	mysql_options(conn, MYSQL_OPT_SSL_MODE, &ssl_mode);
	//*/

	strcpy_s(m_IP, 16, ipAddr);
	strcpy_s(m_UserId, 64, userId);
	strcpy_s(m_Password, 64, password);
	strcpy_s(m_Schema, 64, schema);
}

onenewarm::DBConnection::~DBConnection()
{
	mysql_free_result(m_Results);
	delete m_Results;
}

bool onenewarm::DBConnection::Connect()
{
	bool isSuccess = false;

	AcquireSRWLockExclusive(&s_ConnectLock);
	//for (int cnt = 0; cnt < 5; ++cnt) {
		if (mysql_real_connect(&m_Connection, m_IP, m_UserId, m_Password, m_Schema, m_Port, (char*)NULL, 0) == NULL) {
			//continue;
		}
		isSuccess = true;
		//break;
	//}
	ReleaseSRWLockExclusive(&s_ConnectLock);

	if (isSuccess == true) return true;
	else return false;
}


int onenewarm::DBConnection::SendFormattedQuery(const char* queryForm, ...)
{
	int queryRet;

	char query[MAX_QUERY_SIZE];

	// 가변 인자를 쿼리 포맷을 파싱하여 하나의 문자열로 만듭니다.
	va_list args;
	va_start(args, queryForm);
	vsnprintf(query, sizeof(query), queryForm, args);
	va_end(args);  

	wchar_t wQuery[MAX_QUERY_SIZE];

	while (true) {
		DWORD start, end;
		start = timeGetTime();
		queryRet = mysql_query(&m_Connection, query);
		end = timeGetTime();

		// ms(단위)
		if (end - start > m_MaxProfileTime) {
			ConvertAtoW(wQuery, query);
			_LOG(LOG_LEVEL_SYSTEM, __FILE__, __LINE__, L"Over max query time : [query : %s], [spended : %d(ms)]\n", wQuery, (end - start));
		}

		if (queryRet != 0) {
			int errNo = mysql_errno(&m_Connection);
			ConvertAtoW(wQuery, query);
			wchar_t errMsg[256];
			ConvertAtoW(errMsg, mysql_error(&m_Connection));
			_LOG(LOG_LEVEL_ERROR, __FILE__, __LINE__, L"%s : [query : %s], [errno : %d]\n", errMsg, wQuery, queryRet);
			CCrashDump::Crash();
			return mysql_errno(&m_Connection);
		}
		
		GetResult();
		break;
	}
	
	return 0;

}

int onenewarm::DBConnection::SendQuery(const char* query)
{
	int queryRet;
	wchar_t wQuery[MAX_QUERY_SIZE];

	while (true) {
		DWORD start, end;
		start = timeGetTime();
		queryRet = mysql_query(&m_Connection, query);
		end = timeGetTime();

		// ms(단위)
		if (end - start > m_MaxProfileTime) {
			ConvertAtoW(wQuery, query);
			_LOG(LOG_LEVEL_SYSTEM, __FILE__, __LINE__, L"Over max query time : [query : %s], [spended : %d(ms)]\n", wQuery, (end - start));
		}

		if (queryRet != 0) {
			int errNo = mysql_errno(&m_Connection);
			ConvertAtoW(wQuery, query);
			wchar_t errMsg[256];
			ConvertAtoW(errMsg, mysql_error(&m_Connection));
			_LOG(LOG_LEVEL_ERROR, __FILE__, __LINE__, L"%s : [query : %s], [errno : %d]\n", errMsg, wQuery, queryRet);
			CCrashDump::Crash();
			return mysql_errno(&m_Connection);
		}
		GetResult();
		break;
	}

	return 0;
}

MYSQL_RES* onenewarm::DBConnection::GetResult()
{
	// 결과 출력 전체를 미리 가져옴
	m_Results = mysql_store_result(&m_Connection);

	// 결과를 fetch_row 호출시 1개씩 가져옴
	//res = mysql_use_result(m_Connections);

	if (m_Results == NULL) {
		if (*mysql_error(&m_Connection)) {
			_LOG(LOG_LEVEL_ERROR, __FILE__, __LINE__, L"Failed calling mysql_store_result. ( errno : %d )\n", mysql_errno(&m_Connection));
			CCrashDump::Crash();
		}

		// Query를 요청하지 않고 Result를 요청하는 경우
		return NULL;
	}

	return m_Results;
}

bool onenewarm::DBConnection::FetchRow(MYSQL_ROW* outRow)
{
	// 결과를 fetch_row 호출시 1개씩 가져옴
	//res = mysql_use_result(&m_Connection);
	*outRow = mysql_fetch_row(m_Results);

	if (*outRow == NULL) {
		unsigned int errCode = mysql_errno(&m_Connection);
		wchar_t errMsg[256];
		ConvertAtoW(errMsg, mysql_error(&m_Connection));
		_LOG(LOG_LEVEL_ERROR, __FILE__, __LINE__, L"%s ( errno : %d )\n", errMsg, errCode);
		CCrashDump::Crash();

		// 마지막 row인 경우
		return false;
	}

	return true;
}

onenewarm::DBConnectionTLS::DBConnectionTLS(const char* ipAddr, WORD port, const char* userId, const char* password, const char* schema, DWORD maxQueryTime) : m_MaxProfileTime(maxQueryTime), m_Port(port)
{
	strcpy_s(m_IP, 16, ipAddr);
	strcpy_s(m_UserId, 64, userId);
	strcpy_s(m_Password, 64, password);
	strcpy_s(m_Schema, 64, schema);

	m_TlsIdx = ::TlsAlloc();
	if (m_TlsIdx == TLS_OUT_OF_INDEXES) {
		_LOG(LOG_LEVEL_ERROR, __FILE__, __LINE__, L"TLS_OUT_OF_INDEXES\n");
		CCrashDump::Crash();
	}

	InitializeSRWLock(&m_Lock);
}

onenewarm::DBConnectionTLS::~DBConnectionTLS()
{
	for (int cnt = 0; cnt < m_TLSConnections.size(); ++cnt) {
		delete m_TLSConnections[cnt];
	}

	::TlsFree(m_TlsIdx);
}

DBConnection* onenewarm::DBConnectionTLS::GetDBConnection()
{
	LONG64 idx = (LONG64)::TlsGetValue(m_TlsIdx);

	if (idx == NULL) {
		AcquireSRWLockExclusive(&m_Lock);
		m_TLSConnections.push_back(new DBConnection(m_IP, m_Port, m_UserId, m_Password, m_Schema, m_MaxProfileTime));
		idx = m_TLSConnections.size();
		ReleaseSRWLockExclusive(&m_Lock);
		::TlsSetValue(m_TlsIdx, (LPVOID)m_TLSConnections.size());

		if (m_TLSConnections[idx - 1]->Connect() == false) DebugBreak();
	}
	--idx;
	return m_TLSConnections[idx];
}
