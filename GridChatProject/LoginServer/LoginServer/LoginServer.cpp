#include "LoginServer.h"
#include "CommonProtocol.h"
#include <process.h>
#include <Utils\PerformanceMonitor.h>
#include <strsafe.h>
#include <bcrypt.h>

// BCrypt 라이브러리 링크를 위해
#pragma comment(lib, "bcrypt.lib")

__int64 s_PlayerIdCounter = 0;
PerformanceMonitor s_PerfMonitor;
PerformanceMonitor::MONITOR_INFO s_hInfo;
LONG s_SnapRecvTPS;
LONG s_SnapSentTPS;
LONG s_SnapAuthTPS;


#define MAX_HASH_OBJECT_SIZE 512

int SHA512Hash(const char* data, char* outHash, size_t outHashBufferSize);

bool LoginServer::Handle_EN_PACKET_CS_LOGIN_REQ_LOGIN(__int64 sessionId, __int64 AccountNo, char* SessionKey)
{
	/*--------------------------------------------------------------
	// DB에서 토큰을 가져와서 클라이언트에서 받은 토큰과 비교하는 로직
	----------------------------------------------------------------*/
	DBConnection* dbConn = m_TLSDBConnections->GetDBConnection();
	dbConn->SendFormattedQuery("select sessionkey from sessionkey where accountno = %d;", AccountNo);
	MYSQL_ROW row;
	dbConn->FetchRow(&row);

	if (row[0] != NULL) {
		OnSessionError(sessionId, EN_PACKET_CS_LOGIN_REQ_LOGIN, L"SessionKey is different.", __FILE__, __LINE__);
		return false;
	}

	dbConn->FreeRes();

	/*----------------------------------
	//	토큰 생성
	----------------------------------*/
	char sessionKeyChat[64]; char sessionKeyGame[64];
	{
		char inputData[100];
		sprintf_s(inputData, _countof(inputData), "GAME_%lld_%s", AccountNo, SessionKey);
		SHA512Hash(inputData, sessionKeyGame, _countof(sessionKeyChat));
	}

	{
		char inputData[100];
		sprintf_s(inputData, _countof(inputData), "CHAT_%lld_%s", AccountNo, SessionKey);
		SHA512Hash(inputData, sessionKeyChat, _countof(sessionKeyChat));
	}
	

	/*------------------------------------
	// Redis에 Token을 저장
	-------------------------------------*/
	StoreToken(sessionKeyGame, AccountNo);
	StoreToken(sessionKeyChat, AccountNo);


	// DB에서 ID와 NickName 데이터를 가져온다.
	dbConn->SendFormattedQuery("select userid, usernick from account where accountno = %d;", AccountNo);
	dbConn->FetchRow(&row);

	wchar_t accountId[40];
	wchar_t accountNick[40];

	// DB로부터 받아온 결과는 utf8이기 떄문에 utf-16으로 변환한다.
	ConvertAtoW(accountId, row[0]);
	ConvertAtoW(accountNick, row[1]);
	dbConn->FreeRes();

	CPacketRef resPkt = Make_EN_PACKET_CS_LOGIN_RES_LOGIN_Packet(this, AccountNo, dfLOGIN_STATUS_OK, sessionKeyGame, sessionKeyChat, (char*)accountId, (char*)accountNick, (char*)m_GameServerIP, m_GameServerPort, (char*)m_ChatServerIP, m_ChatServerPort);
	SendPacket(sessionId, *resPkt);
}


unsigned __stdcall LoginServer::Monitor(void* argv)
{
	LoginServer* server = (LoginServer*)argv;

	WCHAR* mLog = (WCHAR*)malloc(sizeof(WCHAR) * 4096);
	if (mLog == NULL) {
		CCrashDump::Crash();
		return 0;
	}

	ULONGLONG prevAcceptTotal = 0;

	while (true) {
		::Sleep(1000);

		ULONGLONG adjUserTotal = 0;
		ULONGLONG zeroCount = 0;


		if (GetAsyncKeyState(VK_F1)) {
			ProfileDataOutText(L"PROFILE");
			ProfileReset();
		}
		else if (GetAsyncKeyState(VK_F2)) {
			server->Stop();
			//break;
		}

		StringCchPrintf(mLog, 4096, L"==========================================================\n");
		StringCchPrintf(mLog, 4096, L"%sHardWare\n", mLog);
		StringCchPrintf(mLog, 4096, L"%s==========================================================\n", mLog);
		StringCchPrintf(mLog, 4096, L"%sCPU [T : %0.1f% / U : %0.1f% / K : %0.1f%], Proc [T : %0.1f% / U : %0.1f% / K : %0.1f%]\n", mLog, s_hInfo.m_ProcessorTotalUsage, s_hInfo.m_ProcessorUserUsage, s_hInfo.m_ProcessorKernelUsage, s_hInfo.m_ProcessTotalUsage, s_hInfo.m_ProcessUserUsage, s_hInfo.m_ProcessKernelUsage);
		StringCchPrintf(mLog, 4096, L"%sMemory [Available : %0.0lf(MB) / NP : %0.0lf(KB)]\nProcMemory [Private : %0.0lf(KB) / NP : %0.0lf(KB)]\n", mLog, s_hInfo.m_AvailableMBytes, s_hInfo.m_NonPagedPoolBytes / 1000, s_hInfo.m_ProcessPrivateBytes / 1000, s_hInfo.m_ProcessNonPagedPoolBytes / 1000);
		StringCchPrintf(mLog, 4096, L"%sNet [Recved : %llu(B) / Sent : %llu(B)]\n\n", mLog, s_hInfo.m_NetRecvBytes, s_hInfo.m_NetSentBytes);

		StringCchPrintf(mLog, 4096, L"%s==========================================================\n", mLog);
		StringCchPrintf(mLog, 4096, L"%sLib\n", mLog);
		StringCchPrintf(mLog, 4096, L"%s==========================================================\n", mLog);

		ULONGLONG curAcceptTotal = server->m_AcceptTotal;

		StringCchPrintf(mLog, 4096, L"%sAcceptTotal : %llu / AcceptTPS : %llu\n", mLog, curAcceptTotal, curAcceptTotal - prevAcceptTotal);
		StringCchPrintf(mLog, 4096, L"%sJobTimerPool : %d\n", mLog, server->m_JobTimerQ->GetSize());
		prevAcceptTotal = curAcceptTotal;

		StringCchPrintf(mLog, 4096, L"%sRecvedTPS : %d / SentTPS : %d\n", mLog, s_SnapRecvTPS, s_SnapSentTPS);
		StringCchPrintf(mLog, 4096, L"%sUsePacket : %d / UseChunk : %d\n", mLog, server->GetUsePacketSize(), server->GetPacketChunkSize());
		StringCchPrintf(mLog, 4096, L"%sSessionNum : %d\n", mLog, server->m_CurSessionCnt);
		StringCchPrintf(mLog, 4096, L"%s==========================================================\n", mLog);
		StringCchPrintf(mLog, 4096, L"%sContents\n", mLog);
		StringCchPrintf(mLog, 4096, L"%s==========================================================\n", mLog);
		StringCchPrintf(mLog, 4096, L"%sAuthTPS : %d\n", mLog, s_SnapAuthTPS);
		StringCchPrintf(mLog, 4096, L"%sPQCSCnt : %d\n", mLog, server->m_PQCSCnt);

		::wprintf(L"%s", mLog);
	}

	::free(mLog);
	::printf("Monitor Thread Returned ( threadId : %d )\n", GetCurrentThreadId());

	return 0;
}

LoginServer::LoginServer() : m_ChatServerPort(0), m_GameServerPort(0), m_TLSDBConnections(NULL), m_RedisClient(NULL), m_TlsTokenIdx(NULL), m_MonitorClient(new MonitorClient), m_AuthTPS(0)
{
	ZeroMemory(&m_ChatServerIP, sizeof(m_ChatServerIP));
	ZeroMemory(&m_GameServerIP, sizeof(m_GameServerIP));

	m_TlsTokenIdx = ::TlsAlloc();

	if (m_TlsTokenIdx == TLS_OUT_OF_INDEXES) {
		//TODO : 로그
		CCrashDump::Crash();
	}

	InitializeSRWLock(&m_TokenLock);

	unsigned int threadId;
	m_MonitorThread = (HANDLE)::_beginthreadex(NULL, 0, Monitor, this, 0, &threadId);
}

LoginServer::~LoginServer()
{
	delete m_TLSDBConnections;
	delete m_RedisClient;
	CloseHandle(m_MonitorThread);
}

void LoginServer::StoreToken(const char* key, LONG64 value)
{
	LONG64 idx = (LONG64)::TlsGetValue(m_TlsTokenIdx);
	
	AcquireSRWLockExclusive(&m_TokenLock);
	if (idx == 0) {
		m_TlsTokenKeys.push_back(string(64, 0));
		m_TlsTokenValues.push_back(string(64, 0));
		idx = m_TlsTokenKeys.size();
		
		::TlsSetValue(m_TlsTokenIdx, (LPVOID)idx);
		m_TlsTokenKeys[idx - 1].resize(64);
	}
	idx--;
	memcpy(&m_TlsTokenKeys[idx][0], key, 64);
	m_TlsTokenValues[idx] = to_string(value);
	m_RedisClient->set(m_TlsTokenKeys[idx], m_TlsTokenValues[idx]);
	m_RedisClient->sync_commit();
	m_RedisClient->expire(m_TlsTokenKeys[idx], 30); // 30초 동안 토큰을 유지합니다.
	m_RedisClient->sync_commit();
	ReleaseSRWLockExclusive(&m_TokenLock);
}


bool LoginServer::OnConnectionRequest(const char* IP, short port)
{
	return false;
}

void LoginServer::OnClientJoin(__int64 sessionID)
{
}

void LoginServer::OnSessionRelease(__int64 sessionID)
{
}

void LoginServer::OnRecv(__int64 sessionID, CPacketRef& pkt)
{
	InterlockedIncrement(&m_AuthTPS);
	PacketProc(this, sessionID, pkt);
}

void LoginServer::OnSessionError(__int64 sessionId, int errCode, const wchar_t* errMsg, const char* filePath, unsigned int fileLine)
{
	_LOG(LOG_LEVEL_WARNING, filePath, fileLine, L"%s (%d)", errMsg, errCode);

	switch (errCode)
	{
	default:
	{
		Disconnect(sessionId);
		break;
	}
	}
}

void LoginServer::OnSystemError(int errCode, const wchar_t* errMsg, const char* filePath, unsigned int fileLine)
{
	_LOG(LOG_LEVEL_ERROR, filePath, fileLine, L"%s (%d)", errMsg, errCode);
}


void LoginServer::OnStart(ConfigLoader* confLoader)
{
	wstring chatServerIP;
	confLoader->GetValue(&chatServerIP, L"SERVICE", L"CHAT_SERVER_IP");

	int chatServerPort;
	confLoader->GetValue(&chatServerPort, L"SERVICE", L"CHAT_SERVER_PORT");


	wstring gameServerIP;
	confLoader->GetValue(&gameServerIP, L"SERVICE", L"GAME_SERVER_IP");

	int gameServerPort;
	confLoader->GetValue(&gameServerPort, L"SERVICE", L"GAME_SERVER_PORT");

	wstring DBIP;
	char aDBIP[64];
	confLoader->GetValue(&DBIP, L"DB", L"IP");
	ConvertWtoA(aDBIP, DBIP.c_str());

	int DBPort;
	confLoader->GetValue(&DBPort, L"DB", L"PORT");

	wstring DBId;
	char aDBId[64];
	confLoader->GetValue(&DBId, L"DB", L"ID");
	ConvertWtoA(aDBId, DBId.c_str());

	wstring DBPass;
	char aDBPass[64];
	confLoader->GetValue(&DBPass, L"DB", L"PASSWORD");
	ConvertWtoA(aDBPass, DBPass.c_str());

	wstring DBSchema;
	char aDBSchema[64];
	confLoader->GetValue(&DBSchema, L"DB", L"SCHEMA");
	ConvertWtoA(aDBSchema, DBSchema.c_str());

	int DBProfileTime;
	confLoader->GetValue(&DBProfileTime, L"DB", L"PROFILE_TIME");

	::wmemcpy(m_ChatServerIP, chatServerIP.c_str(), chatServerIP.size() + 1);
	m_ChatServerPort = (WORD)chatServerPort;

	::wmemcpy(m_GameServerIP, gameServerIP.c_str(), gameServerIP.size() + 1);
	m_GameServerPort = (WORD)gameServerPort;


	m_TLSDBConnections = new DBConnectionTLS(aDBIP, DBPort, aDBId, aDBPass, aDBSchema, DBProfileTime);

	wstring redisIP;
	char aRedisIP[64];
	confLoader->GetValue(&redisIP, L"REDIS", L"IP");
	ConvertWtoA(aRedisIP, redisIP.c_str());

	int redisPort;
	confLoader->GetValue(&redisPort, L"REDIS", L"PORT");

	m_RedisClient = new cpp_redis::client;
	m_RedisClient->connect(string(aRedisIP), (size_t)redisPort);

	MonitorClient* client = m_MonitorClient;
	client->Start(L"MonitorClient.cnf");
	printf("connecting monitor server...\n");
	client->Connect();
	printf("success connected!\n");

	WorkerJobRegister(1000, 0, true, (__int64)m_MonitorClient);
}

void LoginServer::OnStop()
{


}

void LoginServer::OnWorker(__int64 completionKey)
{
	s_SnapRecvTPS = InterlockedExchange(&m_RecvTPS, 0);
	s_SnapSentTPS = InterlockedExchange(&m_SendTPS, 0);
	s_SnapAuthTPS = InterlockedExchange(&m_AuthTPS, 0);


	s_PerfMonitor.CollectMonitorData(&s_hInfo);
	int timestamp = time(NULL);

	while (true) {
		if (m_MonitorClient->m_SessionId != NULL) {
			m_MonitorClient->Request_EN_PACKET_SS_MONITOR_DATA_UPDATE_Packet(dfMONITOR_DATA_TYPE_LOGIN_SERVER_CPU, s_hInfo.m_ProcessTotalUsage, timestamp);
			m_MonitorClient->Request_EN_PACKET_SS_MONITOR_DATA_UPDATE_Packet(dfMONITOR_DATA_TYPE_LOGIN_SERVER_MEM, s_hInfo.m_ProcessPrivateBytes / 1000 / 1000, timestamp);
			m_MonitorClient->Request_EN_PACKET_SS_MONITOR_DATA_UPDATE_Packet(dfMONITOR_DATA_TYPE_LOGIN_SESSION, m_CurSessionCnt, timestamp);
			m_MonitorClient->Request_EN_PACKET_SS_MONITOR_DATA_UPDATE_Packet(dfMONITOR_DATA_TYPE_LOGIN_SESSION, m_CurSessionCnt, timestamp);
			m_MonitorClient->Request_EN_PACKET_SS_MONITOR_DATA_UPDATE_Packet(dfMONITOR_DATA_TYPE_LOGIN_PACKET_POOL, GetPacketChunkSize(), timestamp);
			break;
		}
		else {
			if (InterlockedExchange8((char*)&m_MonitorClient->m_IsConn, 1) == 0) {
				printf("connecting monitor server...\n");
				m_MonitorClient->Connect();
				printf("success connected!\n");
				m_MonitorClient->m_IsConn = false;
				continue;
			}
			break;
		}
	}
}

void LoginServer::OnWorkerExpired(__int64 completionKey)
{
}

int SHA512Hash(const char* data, char* outHash, size_t outHashBufferSize)
{
	{
		BCRYPT_ALG_HANDLE hAlg = NULL;
		NTSTATUS status;
		DWORD hashObjectSize = 0, dataSize = 0, hashLength = 0;
		BYTE hashObject[MAX_HASH_OBJECT_SIZE];  // 스택에 고정 크기 배열 할당

		// SHA512 알고리즘 핸들 열기
		status = BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA512_ALGORITHM, NULL, 0);
		if (!BCRYPT_SUCCESS(status)) {
			return -1;
		}

		// 해시 객체 크기 가져오기
		status = BCryptGetProperty(hAlg, BCRYPT_OBJECT_LENGTH, (PUCHAR)&hashObjectSize, sizeof(DWORD), &dataSize, 0);
		if (!BCRYPT_SUCCESS(status)) {
			BCryptCloseAlgorithmProvider(hAlg, 0);
			return -1;
		}

		// 할당한 버퍼의 크기가 실제 필요한 크기보다 충분한지 확인
		if (hashObjectSize > MAX_HASH_OBJECT_SIZE) {
			BCryptCloseAlgorithmProvider(hAlg, 0);
			return -1;
		}

		// 해시 길이 가져오기 (SHA512의 경우 64바이트)
		status = BCryptGetProperty(hAlg, BCRYPT_HASH_LENGTH, (PUCHAR)&hashLength, sizeof(DWORD), &dataSize, 0);
		if (!BCRYPT_SUCCESS(status)) {
			BCryptCloseAlgorithmProvider(hAlg, 0);
			return -1;
		}

		if (outHashBufferSize < hashLength) {
			BCryptCloseAlgorithmProvider(hAlg, 0);
			return -1;
		}

		// 해시 객체 생성 (스택에 할당한 hashObject 사용)
		BCRYPT_HASH_HANDLE hHash = NULL;
		status = BCryptCreateHash(hAlg, &hHash, hashObject, hashObjectSize, NULL, 0, 0);
		if (!BCRYPT_SUCCESS(status)) {
			BCryptCloseAlgorithmProvider(hAlg, 0);
			return -1;
		}

		// 입력 데이터 해싱
		status = BCryptHashData(hHash, (PUCHAR)data, (ULONG)strlen(data), 0);
		if (!BCRYPT_SUCCESS(status)) {
			BCryptDestroyHash(hHash);
			BCryptCloseAlgorithmProvider(hAlg, 0);
			return -1;
		}

		// 해시 최종화: 결과를 외부에서 할당한 outHash 버퍼에 저장
		status = BCryptFinishHash(hHash, (byte*)outHash, hashLength, 0);
		if (!BCRYPT_SUCCESS(status)) {
			BCryptDestroyHash(hHash);
			BCryptCloseAlgorithmProvider(hAlg, 0);
			return -1;
		}

		// 자원 정리
		BCryptDestroyHash(hHash);
		BCryptCloseAlgorithmProvider(hAlg, 0);

		return 0;
	}
}
