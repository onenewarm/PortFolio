#include "GameServer.h"
#include "GameGroup.h"
#include <strsafe.h>
#include <Utils/PerformanceMonitor.h>
#include "CommonProtocol.h"

using namespace onenewarm;

PerformanceMonitor s_PerfMonitor;
PerformanceMonitor::MONITOR_INFO s_hInfo;
LONG s_SnapRecvTPS;
LONG s_SnapSentTPS;
LONG s_SnapAuthFPS;
LONG s_SnapGameFPS;
LONG s_AcceptTPS;
ULONGLONG s_PrevAcceptTotal = 0;

unsigned __stdcall GameServer::Monitor(void* argv)
{
	GameServer* server = (GameServer*)argv;

	WCHAR* mLog = (WCHAR*)malloc(sizeof(WCHAR) * 4096);
	if (mLog == NULL) {
		CCrashDump::Crash();
		return 0;
	}

	DWORD nextFrameTime = timeGetTime() + 1000;

	while (true) {
		::Sleep(1000);


		if (GetAsyncKeyState(VK_F1)) {
			ProfileDataOutText(L"PROFILE");
			ProfileReset();
		}
		else if (GetAsyncKeyState(VK_F2)) {
			server->Stop();
			//break;
		}

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

		StringCchPrintf(mLog, 4096, L"%sAcceptTotal : %llu / AcceptTPS : %llu\n", mLog, curAcceptTotal, s_AcceptTPS);
		StringCchPrintf(mLog, 4096, L"%sRecvedTPS : %d / SentTPS : %d\n", mLog, s_SnapRecvTPS, s_SnapSentTPS);
		StringCchPrintf(mLog, 4096, L"%sUsePacket : %d / UseChunk : %d\n", mLog, server->GetUsePacketSize(), server->GetPacketChunkSize());
		StringCchPrintf(mLog, 4096, L"%sSessionNum : %d\n", mLog, server->m_CurSessionCnt);
		StringCchPrintf(mLog, 4096, L"%sPQCSCnt : %d\n\n", mLog, server->m_PQCSCnt);
		StringCchPrintf(mLog, 4096, L"%s==========================================================\n", mLog);
		StringCchPrintf(mLog, 4096, L"%sContents\n", mLog);
		StringCchPrintf(mLog, 4096, L"%s==========================================================\n", mLog);
		StringCchPrintf(mLog, 4096, L"%sLoginFPS : %d / GameFPS : %d\n", mLog, s_SnapAuthFPS, s_SnapGameFPS);

		::wprintf(L"%s", mLog);
	}

	::free(mLog);
	::printf("Monitor Thread Returned ( threadId : %d )\n", GetCurrentThreadId());

	return 0;
}

bool onenewarm::GameServer::OnConnectionRequest(const char* IP, short port)
{
	return false;
}

void onenewarm::GameServer::OnClientJoin(__int64 sessionID)
{
	MoveGroup(m_AuthGroup, sessionID, 0);
}

void onenewarm::GameServer::OnSessionRelease(__int64 sessionID)
{
}

void onenewarm::GameServer::OnRecv(__int64 sessionID, CPacketRef& pkt)
{
}

void onenewarm::GameServer::OnSessionError(__int64 sessionId, int errCode, const wchar_t* errMsg, const char* filePath, unsigned int fileLine)
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

void onenewarm::GameServer::OnSystemError(int errCode, const wchar_t* errMsg, const char* filePath, unsigned int fileLine)
{
}

void onenewarm::GameServer::OnStart(ConfigLoader* confLoader)
{

	// Auth 그룹 생성
	m_AuthGroup = new AuthGroup();
	// Game 그룹 생성
	m_GameGroup = new GameGroup();

	AttachGroup(m_AuthGroup, 25);
	AttachGroup(m_GameGroup, 25);

	wstring redisIP;
	char szRedisIP[64];
	confLoader->GetValue(&redisIP, L"REDIS", L"IP");
	ConvertWtoA(szRedisIP, redisIP.c_str());

	int redisPort;
	confLoader->GetValue(&redisPort, L"REDIS", L"PORT");

	m_AuthGroup->m_RedisClient = new cpp_redis::client;
	m_AuthGroup->m_RedisClient->connect(string(szRedisIP), (size_t)redisPort);

	MonitorClient* client = m_MonitorClient;
	client->Start(L"MonitorClient.cnf");
	printf("connecting monitor server...\n");
	client->Connect();
	printf("success connected!\n");

	JobTimerItemRegister(1000, 0, true, 1000, (__int64)m_MonitorClient);
}

void onenewarm::GameServer::OnStop()
{
	
}

void onenewarm::GameServer::OnWorker(__int64 completionKey)
{
	s_SnapRecvTPS = InterlockedExchange(&m_RecvTPS, 0);
	s_SnapSentTPS = InterlockedExchange(&m_SendTPS, 0);
	s_SnapAuthFPS = InterlockedExchange(&m_AuthGroup->m_FPS, 0);
	s_SnapGameFPS = InterlockedExchange(&m_GameGroup->m_FPS, 0);
	ULONGLONG snapAcceptTotal = m_AcceptTotal;
	s_AcceptTPS = snapAcceptTotal - s_PrevAcceptTotal;
	s_PrevAcceptTotal = snapAcceptTotal;

	s_PerfMonitor.CollectMonitorData(&s_hInfo);
	int timestamp = time(NULL);

	if (m_MonitorClient->m_SessionId != NULL) {
		m_MonitorClient->Request_EN_PACKET_SS_MONITOR_DATA_UPDATE_Packet(dfMONITOR_DATA_TYPE_GAME_SERVER_CPU, s_hInfo.m_ProcessTotalUsage, timestamp);
		m_MonitorClient->Request_EN_PACKET_SS_MONITOR_DATA_UPDATE_Packet(dfMONITOR_DATA_TYPE_GAME_SERVER_MEM, s_hInfo.m_ProcessPrivateBytes / 1000 / 1000, timestamp);
		m_MonitorClient->Request_EN_PACKET_SS_MONITOR_DATA_UPDATE_Packet(dfMONITOR_DATA_TYPE_GAME_SESSION, m_CurSessionCnt, timestamp);
		m_MonitorClient->Request_EN_PACKET_SS_MONITOR_DATA_UPDATE_Packet(dfMONITOR_DATA_TYPE_GAME_AUTH_PLAYER, m_AuthGroup->GetPlayerCount(), timestamp);
		m_MonitorClient->Request_EN_PACKET_SS_MONITOR_DATA_UPDATE_Packet(dfMONITOR_DATA_TYPE_GAME_ACCEPT_TPS, s_AcceptTPS, timestamp);
		m_MonitorClient->Request_EN_PACKET_SS_MONITOR_DATA_UPDATE_Packet(dfMONITOR_DATA_TYPE_GAME_PACKET_RECV_TPS, s_SnapRecvTPS, timestamp);
		m_MonitorClient->Request_EN_PACKET_SS_MONITOR_DATA_UPDATE_Packet(dfMONITOR_DATA_TYPE_GAME_PACKET_SEND_TPS, s_SnapSentTPS, timestamp);
		m_MonitorClient->Request_EN_PACKET_SS_MONITOR_DATA_UPDATE_Packet(dfMONITOR_DATA_TYPE_GAME_AUTH_THREAD_FPS, s_SnapAuthFPS, timestamp);
		m_MonitorClient->Request_EN_PACKET_SS_MONITOR_DATA_UPDATE_Packet(dfMONITOR_DATA_TYPE_GAME_GAME_THREAD_FPS, s_SnapGameFPS, timestamp);
		m_MonitorClient->Request_EN_PACKET_SS_MONITOR_DATA_UPDATE_Packet(dfMONITOR_DATA_TYPE_GAME_PACKET_POOL, GetPacketChunkSize(), timestamp);
	}
}

void onenewarm::GameServer::OnWorkerExpired(__int64 completionKey)
{
}

bool onenewarm::GameServer::Handle_EN_PACKET_CS_GAME_REQ_LOGIN(__int64 sessionId, __int64 AccountNo, char* SessionKey)
{
	/*------------------------
	// Redis서버로부터 토큰 확인
	--------------------------*/
	if (m_AuthGroup->CheckToken(AccountNo) == false)
	{
		CPacketRef resPkt = Make_EN_PACKET_CS_GAME_RES_LOGIN_Packet(this, 0);
		SendPacket(sessionId, *resPkt);
		SendDisconnect(sessionId);
		return false;
	}
	CPacketRef resPkt = Make_EN_PACKET_CS_GAME_RES_LOGIN_Packet(this, 1);
	SendPacket(sessionId, *resPkt);
	MoveGroup(m_GameGroup, sessionId, AccountNo);
	SetLogin(sessionId);
	return true;
}

bool onenewarm::GameServer::Handle_EN_PACKET_CS_GAME_REQ_MOVE_START(__int64 sessionId, BYTE direction)
{
	if (direction < dfPACKET_MOVE_DIR_LL || direction > dfPACKET_MOVE_DIR_LD)
	{
		OnSessionError(EN_PACKET_CS_GAME_REQ_MOVE_START, sessionId, L"out of sector's range.", __FILE__, __LINE__);
		Disconnect(sessionId);
		return false;
	}

	Player* curPlayer = &m_GameGroup->m_Players[(WORD)sessionId];
	curPlayer->m_MoveDirection = direction;
	curPlayer->m_Direction = direction - (direction % 2);
	curPlayer->m_IsMove = true;

	CPacketRef moveStartPkt = Make_EN_PACKET_CS_GAME_RES_MOVE_START_Packet(this, curPlayer->m_AccountNo, curPlayer->m_MoveDirection);
	m_GameGroup->SendSectorAround(curPlayer, *moveStartPkt);
	return true;
}

bool onenewarm::GameServer::Handle_EN_PACKET_CS_GAME_REQ_MOVE_STOP(__int64 sessionId, short x, short y)
{
	// 플레이어의 상태 값을 수정
	Player* player = &m_GameGroup->m_Players[(WORD)sessionId];
	player->m_IsMove = false;

	// 서버와 클라이언트의 위치의 비교
	int DiffValue = abs(x - player->m_X) + abs(y - player->m_Y);

	if (DiffValue > dfERROR_RANGE)
	{
		// 차이가 크면 Sync
		CPacketRef syncPkt = Make_EN_PACKET_CS_GAME_RES_SYNC_Packet(this, player->m_X, player->m_X);
		_LOG(LOG_LEVEL_WARNING, __FILE__, __LINE__, L"Sync Send / AccountNo : %d / SessionId : %d / Direction : %d / X : %d / Y : %d /",
			player->m_AccountNo, player->m_SessionID, player->m_MoveDirection, player->m_X, player->m_Y);
		SendPacket(sessionId, *syncPkt);
		return false;
	}
	else
	{
		// 차이가 작으면 클라이언트 값을 믿기
		player->m_X = x;
		player->m_Y = y;
	}

	// SectorAround에 있는 유저들에게 Stop 패킷을 보내기
	CPacketRef moveStopPkt = Make_EN_PACKET_CS_GAME_RES_MOVE_STOP_Packet(this, player->m_AccountNo, player->m_X, player->m_Y);
	m_GameGroup->SendSectorAround(player, *moveStopPkt);
	return true;
}

onenewarm::GameServer::GameServer()
{
	m_MonitorClient = new MonitorClient();

	unsigned int threadId;
	m_MonitorThread = (HANDLE)::_beginthreadex(NULL, 0, Monitor, this, 0, &threadId);
}
