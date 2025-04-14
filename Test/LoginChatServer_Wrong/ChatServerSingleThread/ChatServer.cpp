#include "ChatServer.h"
#include "CommonProtocol.h"
#include <process.h>
#include <Utils\PerformanceMonitor.h>
#include <strsafe.h>
#include <string>
#include <conio.h>

__int64 s_PlayerIdCounter = 0;
PerformanceMonitor s_PerfMonitor;
PerformanceMonitor::MONITOR_INFO s_hInfo;
LONG s_SnapRecvTPS, s_SnapSentTPS, s_SnapUpdateTPS, s_SnapMsgReqTPS, s_SnapMsgResTPS, s_SnapAuthTPS;
__int64 s_TotalRecvTPS = 0;
LONG s_PerfCount = 0;

bool ChatServer::OnConnectionRequest(const char* IP, short port)
{
	// 아직 할 일 없음
	return false;
}

void ChatServer::OnClientJoin(__int64 sessionID)
{
	// 할 일 없음
}

void ChatServer::OnSessionRelease(__int64 sessionID)
{
	Job* job = m_JobPool->Alloc();
	job->type = Job::JOB_TYPE::PLAYER_RELEASE;
	job->sessionId = sessionID;

	LONG jobQSize = m_UpdateJobQ->Enqueue(job);
	if (jobQSize == -1) {
		Stop();
		return;
	}

	if (jobQSize == 1)
		::SetEvent(m_UpdateEvent);
}

void ChatServer::OnRecv(__int64 sessionID, CPacketRef& pkt)
{
	Job* job = m_JobPool->Alloc();
	job->type = *((WORD*)(*pkt)->GetBufferPtr());
	job->sessionId = sessionID;
	job->pktRef = pkt;

	LONG jobQSize = m_UpdateJobQ->Enqueue(job);
	if (jobQSize == -1) {
		OnSystemError(-1, L"UpdateJobQ is fulled.", __FILE__, __LINE__);
		return;
	}
	if (jobQSize == 1)
		::SetEvent(m_UpdateEvent);
}

void ChatServer::OnSessionError(__int64 sessionId, int errCode, const wchar_t* errMsg, const char* filePath, unsigned int fileLine)
{
	_LOG(LOG_LEVEL_WARNING, filePath, fileLine, L"%s (%d)", errMsg, errCode);

	switch (errCode)
	{
	case CHAT_ERROR::LOGIN_FAILED:
	{
		char id[20];
		char nick[20];
		CPacketRef pkt = Make_EN_PACKET_CS_CHAT_RES_LOGIN_Packet(this, en_PACKET_CS_LOGIN_RES_LOGIN::dfLOGIN_STATUS_FAIL, id, nick, 0);
		SendPacket(sessionId, *pkt);
		SendDisconnect(sessionId);
		break;
	}
	default:
	{
		Disconnect(sessionId);
		break;
	}
	}
}

void ChatServer::OnSystemError(int errCode, const wchar_t* errMsg, const char* filePath, unsigned int fileLine)
{
	_LOG(LOG_LEVEL_WARNING, filePath, fileLine, L"%s (%d)", errMsg, errCode);

	CCrashDump::Crash();

	/*
	switch (errCode)
	{
	default:
	{
		
		break;
	}
	}
	*/
}

void ChatServer::OnStart(ConfigLoader* confLoader)
{
	confLoader->GetValue(&m_MaxPlayer, L"SERVER", L"USER_MAX");

	if (m_DBConnection->Connect() == false) CCrashDump::Crash();

	MonitorClient* client = m_MonitorClient;
	client->Start(L"MonitorClient.cnf");
	printf("connecting monitor server...\n");
	client->Connect();
	printf("success connected!\n");

	WorkerJobRegister(1000, 0, true, (__int64)m_MonitorClient);
}

void ChatServer::OnStop()
{
	DWORD waitUpdateThreadRet, waitCheckTokenThreadRet;
	SetEvent(m_UpdateEvent);
	waitUpdateThreadRet = ::WaitForSingleObject(m_UpdateThread, INFINITE);
	if (waitUpdateThreadRet == WAIT_FAILED) {
		int errCode = GetLastError();
		_LOG(LOG_LEVEL_ERROR, __FILE__, __LINE__, L"Failed calling WaitForSingleObject to close updateThread (err : %d)", errCode);
		CCrashDump::Crash();
	}

	SetEvent(m_TokenEvent);
	waitCheckTokenThreadRet = ::WaitForSingleObject(m_TokenEvent, INFINITE);
	if (waitCheckTokenThreadRet == WAIT_FAILED) {
		int errCode = GetLastError();
		_LOG(LOG_LEVEL_ERROR, __FILE__, __LINE__, L"Failed calling WaitForSingleObject to close checkTokenThread (err : %d)", errCode);
		CCrashDump::Crash();
	}


	while (m_PlayerMap->size() > 0) {
		Sleep(100);
	}
}

void ChatServer::OnWorker(__int64 completionKey)
{
	// 1초마다 OnWorker에서 모니터링 데이터 셋팅
	s_SnapRecvTPS = InterlockedExchange(&m_RecvTPS, 0);
	s_SnapSentTPS = InterlockedExchange(&m_SendTPS, 0);
	s_SnapUpdateTPS = InterlockedExchange(&m_UpdateTPS, 0);
	s_SnapMsgReqTPS = InterlockedExchange(&m_MsgReqTPS, 0);
	s_SnapMsgResTPS = InterlockedExchange(&m_MsgResTPS, 0);
	s_SnapAuthTPS = InterlockedExchange(&m_AuthTPS, 0);

	if (s_SnapRecvTPS > 5000)
	{
		InterlockedIncrement(&s_PerfCount);
		InterlockedAdd64(&s_TotalRecvTPS, s_SnapRecvTPS);
	}

	s_PerfMonitor.CollectMonitorData(&s_hInfo);
	int timestamp = (int)time(NULL);

	// 모니터링 서버에게 패킷 보내기
	while (true) {
		if (m_MonitorClient->m_SessionId != NULL) {
			m_MonitorClient->Request_EN_PACKET_SS_MONITOR_DATA_UPDATE_Packet(dfMONITOR_DATA_TYPE_CHAT_SERVER_CPU, (int)s_hInfo.m_ProcessTotalUsage, timestamp);
			m_MonitorClient->Request_EN_PACKET_SS_MONITOR_DATA_UPDATE_Packet(dfMONITOR_DATA_TYPE_CHAT_SERVER_MEM, (int)(s_hInfo.m_ProcessPrivateBytes / 1000 / 1000), timestamp);
			m_MonitorClient->Request_EN_PACKET_SS_MONITOR_DATA_UPDATE_Packet(dfMONITOR_DATA_TYPE_CHAT_SESSION, m_CurSessionCnt, timestamp);
			m_MonitorClient->Request_EN_PACKET_SS_MONITOR_DATA_UPDATE_Packet(dfMONITOR_DATA_TYPE_CHAT_PLAYER, m_PlayerPool->GetUseCount(), timestamp);
			m_MonitorClient->Request_EN_PACKET_SS_MONITOR_DATA_UPDATE_Packet(dfMONITOR_DATA_TYPE_CHAT_UPDATE_TPS, s_SnapUpdateTPS, timestamp);
			m_MonitorClient->Request_EN_PACKET_SS_MONITOR_DATA_UPDATE_Packet(dfMONITOR_DATA_TYPE_CHAT_PACKET_POOL, GetPacketChunkSize(), timestamp);
			m_MonitorClient->Request_EN_PACKET_SS_MONITOR_DATA_UPDATE_Packet(dfMONITOR_DATA_TYPE_CHAT_UPDATEMSG_POOL, m_UpdateJobQ->GetSize(), timestamp);
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

void ChatServer::OnWorkerExpired(__int64 completionKey)
{
}

void ChatServer::InitPlayer(Player* player)
{
	player->m_PlayerId = s_PlayerIdCounter++;
	player->m_IsLogin = false;
	player->m_SectorX = INVALID_POS;
	player->m_SectorY = INVALID_POS;
	player->m_Id[0] = L'\0';
	player->m_NickName[0] = L'\0';
	player->m_AccountNo = 0;
}

void ChatServer::ReleasePlayer(Player& player)
{
	WORD x = player.m_SectorX;
	WORD y = player.m_SectorY;

	m_Sectors[y][x]->erase(&player);
	m_PlayerMap->erase(player.m_SessionId);

	m_PlayerPool->Free(&player);
}

bool ChatServer::Handle_EN_PACKET_CS_CHAT_REQ_LOGIN(__int64 sessionId, __int64 AccountNo, char* SessionKey)
{
	InterlockedIncrement(&m_AuthTPS);

	/*------------------------
	// 토큰 확인
	--------------------------*/
	if (CheckToken(AccountNo, SessionKey) == false)
	{
		OnSessionError(sessionId, EN_PACKET_CS_CHAT_REQ_LOGIN, L"Token was wrong.", __FILE__, __LINE__);
		return false;
	}

	Player* player = m_PlayerPool->Alloc();
	m_PlayerMap->insert({ sessionId, player });
	InitPlayer(player);

	player->m_AccountNo = AccountNo;
	player->m_SessionId = sessionId;
	player->m_IsLogin = true;

	// DB에서 ID와 NickName 데이터를 가져온다.
	MYSQL_ROW resRow;
	m_DBConnection->SendFormattedQuery("select userid, usernick from account where accountno = %d;", AccountNo);
	m_DBConnection->FetchRow(&resRow);
	m_DBConnection->FreeRes();

	wchar_t accountId[40];
	wchar_t accountNick[40];

	// DB로부터 받아온 결과는 utf8이기 떄문에 utf-16으로 변환한다.
	ConvertAtoW(accountId, resRow[0]);
	ConvertAtoW(accountNick, resRow[1]);

	wmemcpy(player->m_Id, (wchar_t*)accountId, 20);
	wmemcpy(player->m_NickName, (wchar_t*)accountNick, 20);
	SetLogin(sessionId);

	CPacketRef resPkt = Make_EN_PACKET_CS_CHAT_RES_LOGIN_Packet(this, en_PACKET_CS_LOGIN_RES_LOGIN::dfLOGIN_STATUS_OK, (char*)accountId, (char*)accountNick, player->m_AccountNo);
	SendPacket(player->m_SessionId, *resPkt);

	return true;
}

bool ChatServer::Handle_EN_PACKET_CS_CHAT_REQ_SECTOR_MOVE(__int64 sessionId, __int64 AccountNo, WORD SectorX, WORD SectorY)
{
	if (SectorX < 0 || SectorX >= MAX_POS || SectorY < 0 || SectorY >= MAX_POS)
	{
		OnSessionError(sessionId, EN_PACKET_CS_CHAT_REQ_SECTOR_MOVE, L"Out of sector's bound.", __FILE__, __LINE__);
		return false;
	}

	if (m_PlayerMap->find(sessionId) == m_PlayerMap->end())
	{
		OnSessionError(sessionId, EN_PACKET_CS_CHAT_REQ_SECTOR_MOVE, L"Requested packet before created player.", __FILE__, __LINE__);
		return false;
	}

	Player* player = (*m_PlayerMap)[sessionId];

	if (player->m_IsLogin == false)
	{
		OnSessionError(sessionId, EN_PACKET_CS_CHAT_REQ_SECTOR_MOVE, L"Requested packet before login.", __FILE__, __LINE__);
		return false;
	}

	if (player->m_SectorX != INVALID_POS) {
		WORD curX = player->m_SectorX;
		WORD curY = player->m_SectorY;

		m_Sectors[curY][curX]->erase(player);
	}

	player->m_SectorX = SectorX;
	player->m_SectorY = SectorY;
	m_Sectors[SectorY][SectorX]->insert(player);

	CPacketRef resPkt = Make_EN_PACKET_CS_CHAT_RES_SECTOR_MOVE_Packet(this, player->m_AccountNo, player->m_SectorX, player->m_SectorY);
	SendPacket(sessionId, *resPkt);

	return true;
}

bool ChatServer::Handle_EN_PACKET_CS_CHAT_REQ_MESSAGE(__int64 sessionId, __int64 AccountNo, WORD MessageLen, char* Message)
{
	if (m_PlayerMap->find(sessionId) == m_PlayerMap->end())
	{
		OnSessionError(sessionId, EN_PACKET_CS_CHAT_REQ_SECTOR_MOVE, L"Requested packet before created player.", __FILE__, __LINE__);
		return false;
	}

	Player* player = (*m_PlayerMap)[sessionId];

	if (player->m_IsLogin == false)
	{
		OnSessionError(sessionId, EN_PACKET_CS_CHAT_REQ_SECTOR_MOVE, L"Requested packet before login.", __FILE__, __LINE__);
		return false;
	}

	m_MsgReqTPS++;
	CPacketRef resPkt = Make_EN_PACKET_CS_CHAT_RES_MESSAGE_Packet(this, player->m_AccountNo, (char*)player->m_Id, (char*)player->m_NickName, MessageLen, Message);
	SendSectorAround(player, *resPkt);

	return true;
}

bool ChatServer::Handle_EN_PACKET_CS_CHAT_REQ_HEARTBEAT(__int64 sessionId)
{
	// 할 일 없음
	return true;
}


void ChatServer::Handle_PlayerRelease(__int64 sessionId)
{
	if (m_PlayerMap->find(sessionId) == m_PlayerMap->end()) return;

	Player* player = (*m_PlayerMap)[sessionId];
	m_PlayerMap->erase(sessionId);

	WORD sectorX = player->m_SectorX;
	WORD sectorY = player->m_SectorY;

	if (sectorX != INVALID_POS && sectorY != INVALID_POS) {
		m_Sectors[sectorY][sectorX]->erase(player);
	}

	m_PlayerPool->Free(player);
}


unsigned __stdcall ChatServer::Update(void* argv)
{
	ChatServer* server = (ChatServer*)argv;
	DWORD waitRet, errCode;

	while (server->m_IsStop == false) {
		if (server->m_UpdateJobQ->GetSize() == 0) {
			waitRet = WaitForSingleObject(server->m_UpdateEvent, INFINITE);
			if (waitRet != WAIT_OBJECT_0) {
				errCode = GetLastError();
				server->OnSystemError(errCode, L"Failed wait update event.", __FILE__, __LINE__);
			}
		}

		int remainJob = server->m_UpdateJobQ->GetSize();
		while (remainJob--) {
			Job* job;
			server->m_UpdateJobQ->Dequeue(&job);
			server->JobProc(job);
			server->m_JobPool->Free(job);
			InterlockedIncrement(&server->m_UpdateTPS);
		}
	}

	::printf("UpdateThread was returned. ( threadId : %d )\n", GetCurrentThreadId());
	return 0;
}

unsigned __stdcall ChatServer::Monitor(void* argv)
{
	ChatServer* server = (ChatServer*)argv;

	WCHAR* mLog = (WCHAR*)malloc(sizeof(WCHAR) * 4096);
	if (mLog == NULL) {
		CCrashDump::Crash();
		return 0;
	}

	ULONGLONG prevAcceptTotal = 0;
	while (server->m_IsStop == false) {
		::Sleep(1000);

		if (GetAsyncKeyState(VK_F1)) {
			ProfileDataOutText(L"PROFILE");
			ProfileReset();
		}
		else if (GetAsyncKeyState(VK_F2)) {
			server->Stop();
			continue;
		}

		ULONGLONG adjUserTotal = 0, zeroCount = 0, curAcceptTotal = 0;

		for (short row = 0; row < MAX_POS; ++row) {
			for (short col = 0; col < MAX_POS; ++col) {
				
				ULONGLONG adjUserCnt = 0;
				short startRow, endRow, startCol, endCol;
				startRow = (row == 0) ? row : row - 1;
				endRow = (row == MAX_POS - 1) ? row + 1 : row + 2;
				startCol = (col == 0) ? col : col - 1;
				endCol = (col == MAX_POS - 1) ? col + 1 : col + 2;

				for (short adjRow = startRow; adjRow < endRow; ++adjRow) {
					for (short adjCol = startCol; adjCol < endCol; ++adjCol) {
						adjUserCnt += server->m_Sectors[adjRow][adjCol]->size();
					}
				}

				if (adjUserCnt == 0) zeroCount++;
				adjUserTotal += adjUserCnt;
			}
		}
		if (adjUserTotal > 0) {
			server->m_AvgSector = (double)adjUserTotal / (2500 - zeroCount);
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

		curAcceptTotal = server->m_AcceptTotal;

		StringCchPrintf(mLog, 4096, L"%sAcceptTotal : %llu / AcceptTPS : %llu\n", mLog, curAcceptTotal, curAcceptTotal - prevAcceptTotal);
		prevAcceptTotal = curAcceptTotal;

		StringCchPrintf(mLog, 4096, L"%sRecvedTPS : %d / SentTPS : %d\n", mLog, s_SnapRecvTPS, s_SnapSentTPS);
		StringCchPrintf(mLog, 4096, L"%sUsePacket : %d / UseChunk : %d\n", mLog, server->GetUsePacketSize(), server->GetPacketChunkSize());
		StringCchPrintf(mLog, 4096, L"%sSessionNum : %d\n\n", mLog, server->m_CurSessionCnt);
		StringCchPrintf(mLog, 4096, L"%s==========================================================\n", mLog);
		StringCchPrintf(mLog, 4096, L"%sContents\n", mLog);
		StringCchPrintf(mLog, 4096, L"%s==========================================================\n", mLog);
		StringCchPrintf(mLog, 4096, L"%sUpdateTPS : %d\n", mLog, s_SnapUpdateTPS);
		StringCchPrintf(mLog, 4096, L"%sUpdateMsgQ : %d / PlayerPool : %d / PlayerCount : %d / PlayerMapSize : %d\n", mLog, server->m_UpdateJobQ->GetSize(), server->m_PlayerPool->GetUseCount() + server->m_PlayerPool->GetCapacityCount(), (LONG)server->m_PlayerMap->size(), (int)server->m_PlayerMap->bucket_count());
		StringCchPrintf(mLog, 4096, L"%sJobChunk : %d / JobPool : %d\n", mLog, server->m_JobPool->GetChunkSize(), server->m_JobPool->m_UseNodeCount);
		StringCchPrintf(mLog, 4096, L"%sMsgReqTPS : %d / MsgResTPS : %d / AvgAdjUsers : %0.1lf\n", mLog, s_SnapMsgReqTPS, s_SnapMsgResTPS, server->m_AvgSector);
		StringCchPrintf(mLog, 4096, L"%sPQCSCnt : %d\n", mLog, server->m_PQCSCnt);
		StringCchPrintf(mLog, 4096, L"%sTotalRecvTPS : %lld / AvgRecvTPS : %lf\n", mLog, s_TotalRecvTPS, (double)s_TotalRecvTPS / s_PerfCount);
		StringCchPrintf(mLog, 4096, L"%sAvgAuthTPS : %lf\n", mLog, (double)curAcceptTotal / s_PerfCount);
		::wprintf(L"%s", mLog);
	}
	::free(mLog);
	::printf("MonitorThread was returned. ( threadId : %d )\n", GetCurrentThreadId());
	return 0;
}


ChatServer::ChatServer() : m_UpdateEvent(CreateEvent(NULL, false, false, NULL)), m_TokenEvent(CreateEvent(NULL, false, false, NULL)), m_UpdateThread(INVALID_HANDLE_VALUE), m_MonitorThread(INVALID_HANDLE_VALUE), m_CheckTokenThread(INVALID_HANDLE_VALUE), m_MaxPlayer(0), m_PlayerPool(new ObjectPool<Player>()), m_PlayerMap(new unordered_map<__int64, Player*>()), m_JobPool(new TLSObjectPool<Job>(0, true)), m_UpdateJobQ(new LockFreeQueue<Job*>(MAX_JOBQ_SIZE)), m_TokenJobQ(new LockFreeQueue<Job*>(MAX_JOBQ_SIZE)), m_RecvMsgTPS(0), m_SendMsgTPS(0), m_UpdateTPS(0), m_MonitorClient((MonitorClient*)_aligned_malloc(sizeof(MonitorClient), 64)), m_DBConnection(new DBConnection("10.0.2.1", 3306, "onenewarm", "password", "accountdb", 1000))
{
	DWORD errCode;

	new(m_MonitorClient) MonitorClient;

	if (m_UpdateEvent == NULL) {
		errCode = GetLastError();
		CCrashDump::Crash();
	}

	for (int row = 0; row < MAX_POS; ++row) {
		for (int col = 0; col < MAX_POS; ++col) {
			m_Sectors[row][col] = new set<Player*>();
		}
	}

	unsigned int threadId;
	m_UpdateThread = (HANDLE)::_beginthreadex(NULL, 0, Update, this, 0, &threadId);
	m_MonitorThread = (HANDLE)::_beginthreadex(NULL, 0, Monitor, this, 0, &threadId);
}

ChatServer::~ChatServer()
{
	m_MonitorClient->~MonitorClient();
	CloseHandle(m_UpdateEvent);
	CloseHandle(m_TokenEvent);
	CloseHandle(m_UpdateThread);
	CloseHandle(m_MonitorThread);
	CloseHandle(m_CheckTokenThread);
	
	for (int row = 0; row < MAX_POS; ++row) {
		for (int col = 0; col < MAX_POS; ++col) {
			delete m_Sectors[row][col];
		}
	}
	delete m_PlayerPool;
	delete m_PlayerMap;
	delete m_JobPool;
	delete m_UpdateJobQ;
	delete m_TokenJobQ;
	delete m_DBConnection;
	_aligned_free(m_MonitorClient);
}

void ChatServer::SendSectorAround(Player* player, CPacket* pkt)
{
	short row = player->m_SectorY;
	short col = player->m_SectorX;

	short startRow, endRow, startCol, endCol;
	startRow = (row == 0) ? row : row - 1;
	endRow = (row == MAX_POS - 1) ? row + 1 : row + 2;
	startCol = (col == 0) ? col : col - 1;
	endCol = (col == MAX_POS - 1) ? col + 1 : col + 2;

	for (short adjRow = startRow; adjRow < endRow; ++adjRow) {
		for (short adjCol = startCol; adjCol < endCol; ++adjCol) {
			auto sector = m_Sectors[adjRow][adjCol];

			for (auto playerIter = sector->begin(); playerIter != sector->end(); ++playerIter) {
				SendPacket((*playerIter)->m_SessionId, pkt);
			}
		}
	}
}

void ChatServer::JobProc(Job* job)
{
	switch (job->type) {
	case Job::JOB_TYPE::PLAYER_RELEASE:
	{
		Handle_PlayerRelease(job->sessionId);
		break;
	}
	default:
	{
		PacketProc(this, job->sessionId, job->pktRef);
		break;
	}
	}
}

bool ChatServer::CheckToken(LONG64 accountNo, const char* sessionKey)
{
	/*--------------------------------------------------------------
	// DB에서 토큰을 가져와서 클라이언트에서 받은 토큰과 비교하는 로직
	----------------------------------------------------------------*/
	m_DBConnection->SendFormattedQuery("select sessionkey from sessionkey where accountno = %d;", accountNo);
	MYSQL_ROW row;
	m_DBConnection->FetchRow(&row);

	bool ret = IsSameKey(sessionKey, row[0]);

	m_DBConnection->FreeRes();

	return ret;
}

bool ChatServer::IsSameKey(const char* aKey, const char* bKey)
{
	return true;
}

