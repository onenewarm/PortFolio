#include "ChatServer.h"
#include "CommonProtocol.h"
#include <process.h>
#include "..\..\Lib\Utils\PerformanceMonitor.h"
#include <strsafe.h>
#include <string>
#include <conio.h>

__int64 s_PlayerIdCounter = 0;
PerformanceMonitor s_PerfMonitor;

LONG s_PerfCount = 0;
__int64 s_TotalRecvTPS = 0;
double s_TotalProcessTotalUsage = 0;

bool ChatServer::OnConnectionRequest(const char* IP, short port)
{
	return false;
}

void ChatServer::OnClientJoin(__int64 sessionID)
{
}

void ChatServer::OnSessionRelease(__int64 sessionID)
{
	WORD idx = sessionID & 0xFFFF;

	Player* player = &m_Players[idx];
	
	if (player->m_IsLogin == false) return;

	if (player->m_SectorX < 0 || player->m_SectorY < 0 || player->m_SectorX >= MAX_POS || player->m_SectorY >= MAX_POS) return;

	ClearPlayer(player);

	InterlockedDecrement((LONG*) & m_CurPlayer);
}

void ChatServer::OnRecv(__int64 sessionID, CPacketRef& pkt)
{
	PacketProc(sessionID, *pkt);
}

void ChatServer::OnSessionError(__int64 sessionId, int errCode, const wchar_t* errMsg, const char* filePath, unsigned int fileLine)
{
	_LOG(LOG_LEVEL_WARNING, filePath, fileLine, errMsg);

	switch(errCode)
	{
	default :
	{
		Disconnect(sessionId);
		break;
	}
	}
}

void ChatServer::OnSystemError(int errCode, const wchar_t* errMsg, const char* filePath, unsigned int fileLine)
{
	_LOG(LOG_LEVEL_ERROR, filePath, fileLine, errMsg);

	switch (errCode)
	{
	default:
	{
		CCrashDump::Crash();
		break;
	}
	}
}

void ChatServer::OnStart(ConfigLoader* confLoader)
{
	confLoader->GetValue(&m_MaxPlayer, L"SERVER", L"USER_MAX");

	m_Players = (Player*)malloc(sizeof(Player) * m_MaxSessions);

	if (m_Players != NULL) {
		for (int cnt = 0; cnt < m_MaxSessions; ++cnt) {
			InitPlayer(&m_Players[cnt]);
		}
	}
	else CCrashDump::Crash();
	
}

void ChatServer::OnStop()
{
	while (m_CurPlayer > 0) {
		Sleep(100);
	}
}

void ChatServer::OnWorker(__int64 completionKey)
{
	// 할 일 없음
}

void ChatServer::OnWorkerExpired(__int64 completionKey)
{
	// 할 일 없음
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

void ChatServer::ClearPlayer(Player* player)
{
	WORD x = player->m_SectorX;
	WORD y = player->m_SectorY;

	AcquireSRWLockExclusive(&m_SectorLocks[y][x]);
	list<Player*>* sector = m_Sectors[y][x];
	for (auto sectorIter = sector->begin(); sectorIter != sector->end(); ++sectorIter) {
		if ((*sectorIter) == player) {
			sector->erase(sectorIter);
			break;
		}
	}
	ReleaseSRWLockExclusive(&m_SectorLocks[y][x]);

	player->m_IsLogin = false;
	player->m_SectorX = INVALID_POS;
	player->m_SectorY = INVALID_POS;
}

void ChatServer::HandleRequestLogin(__int64 sessionId, CPacket* pkt)
{
	LONG64 accountNo;
	WCHAR id[ID_MAX_LEN];
	WCHAR nickName[NICKNAME_MAX_LEN];
	char sessionKey[SESSIONKEY_LEN];

	CPacketRef resPkt = AllocPacket();

	try
	{
		*pkt >> accountNo;
		pkt->GetData((char*)id, sizeof(WCHAR) * ID_MAX_LEN);
		pkt->GetData((char*)nickName, sizeof(WCHAR) * NICKNAME_MAX_LEN);
		pkt->GetData((char*)sessionKey, SESSIONKEY_LEN);
	}
	catch (int e)
	{
		OnSessionError(sessionId, -1, L"Failed unmarshalling.", __FILE__, __LINE__);
		return;
	}

	WORD playerIdx = sessionId & 0xFFFF;

	Player* player = &m_Players[playerIdx];
	player->m_AccountNo = accountNo;
	player->m_SessionId = sessionId;
	player->m_IsLogin = true;
	SetLogin(sessionId);

	InterlockedIncrement((LONG*)&m_CurPlayer);

	*(*resPkt) << (WORD)en_PACKET_CS_CHAT_RES_LOGIN << (BYTE)0 << player->m_AccountNo;
	SendPacket(sessionId, *resPkt);
	return;
}

void ChatServer::HandleRequestSectorMove(__int64 sessionId, CPacket* pkt)
{
	__int64 accountNo;
	WORD sectorX;
	WORD sectorY;

	WORD playerIdx = sessionId & 0xFFFF;
	Player* player = &m_Players[playerIdx];

	if (player->m_IsLogin == false)
	{
		OnSessionError(sessionId, en_PACKET_CS_CHAT_REQ_SECTOR_MOVE, L"Try SectorMove before login.", __FILE__, __LINE__);
		return;
	}

	try
	{
		*pkt >> accountNo >> sectorX >> sectorY;
	}
	catch (int e)
	{
		OnSessionError(sessionId, -1, L"Failed unmarshalling.",__FILE__, __LINE__);
		return;
	}

	if (sectorX < 0 || sectorX >= MAX_POS || sectorY < 0 || sectorY >= MAX_POS)
	{
		OnSessionError(sessionId, en_PACKET_CS_CHAT_REQ_SECTOR_MOVE, L"Out of sector's range.", __FILE__, __LINE__);
		return;
	}

	if (player->m_SectorX == INVALID_POS || player->m_SectorY == INVALID_POS) {
		player->m_SectorX = sectorX;
		player->m_SectorY = sectorY;

		AcquireSRWLockExclusive(&m_SectorLocks[sectorY][sectorX]);
		m_Sectors[sectorY][sectorX]->push_back(player);
		ReleaseSRWLockExclusive(&m_SectorLocks[sectorY][sectorX]);
	}
	else{
		WORD curX = player->m_SectorX;
		WORD curY = player->m_SectorY;

		player->m_SectorX = sectorX;
		player->m_SectorY = sectorY;

		list<Player*>* sector = m_Sectors[curY][curX];

		SRWLOCK* firstLock;
		SRWLOCK* secondLock;

		do {
			if (curY < sectorY) {
				firstLock = &m_SectorLocks[curY][curX];
				secondLock = &m_SectorLocks[sectorY][sectorX];
			}
			else if (curY > sectorY) {
				firstLock = &m_SectorLocks[sectorY][sectorX];
				secondLock = &m_SectorLocks[curY][curX];
			}
			else {
				if (curX < sectorX) {
					firstLock = &m_SectorLocks[curY][curX];
					secondLock = &m_SectorLocks[sectorY][sectorX];
				}
				else if (curX > sectorX) {
					firstLock = &m_SectorLocks[sectorY][sectorX];
					secondLock = &m_SectorLocks[curY][curX];
				}
				else break;
			}

			AcquireSRWLockExclusive(firstLock);
			AcquireSRWLockExclusive(secondLock);
			for (auto sectorIter = sector->begin(); sectorIter != sector->end(); ++sectorIter) {
				if (*sectorIter == player) {
					sector->erase(sectorIter);
					break;
				}
			}
			m_Sectors[sectorY][sectorX]->push_back(player);
			ReleaseSRWLockExclusive(secondLock);
			ReleaseSRWLockExclusive(firstLock);
		} while (0);
	}

	CPacketRef resPkt = AllocPacket();
	*(*resPkt) << (WORD)en_PACKET_CS_CHAT_RES_SECTOR_MOVE << player->m_AccountNo << player->m_SectorX << player->m_SectorY;
	SendPacket(sessionId, *resPkt);

	return;
}

void ChatServer::HandleRequestChatMessage(__int64 sessionId, CPacket* pkt)
{
	__int64 accountNo;
	WORD messageLen;
	WCHAR message[MESSAGE_MAX_LEN];

	WORD idx = sessionId & 0xFFFF;
	Player* player = &m_Players[idx];
	
	if (player->m_IsLogin == false)
	{
		OnSessionError(sessionId, en_PACKET_CS_CHAT_REQ_MESSAGE, L"Try Message before login.", __FILE__, __LINE__);
		return;
	}

	try
	{
		*pkt >> accountNo;
		*pkt >> messageLen;
		pkt->GetData((char*)message, messageLen);
	}
	catch (int e)
	{
		OnSessionError(sessionId, en_PACKET_CS_CHAT_REQ_MESSAGE, L"Failed unmarsharlling.", __FILE__, __LINE__);
		return;
	}


	CPacketRef resPkt = AllocPacket();

	*(*resPkt) << (WORD)en_PACKET_CS_CHAT_RES_MESSAGE << player->m_AccountNo;

	(*resPkt)->PutData((char*)&player->m_Id, sizeof(player->m_Id));
	(*resPkt)->PutData((char*)&player->m_NickName, sizeof(player->m_NickName));
	*(*resPkt) << messageLen;
	(*resPkt)->PutData((char*)&message, messageLen);

	SendSectorAround(player, *resPkt);
}

void ChatServer::HandleRequestHeartbeat(__int64 sessionId)
{
	WORD idx = sessionId & 0xFFFF;
	Player* player = &m_Players[idx];

	if (player->m_IsLogin == false) {
		OnSessionError(sessionId, en_PACKET_CS_CHAT_REQ_HEARTBEAT, L"Try Heartbeat before login.", __FILE__, __LINE__);
		return;
	}
}

unsigned __stdcall ChatServer::Monitor(void* argv)
{
	ChatServer* server = (ChatServer*)argv;
	PerformanceMonitor::MONITOR_INFO hInfo;

	WCHAR* mLog = (WCHAR*)malloc(sizeof(WCHAR) * 4096);
	if (mLog == NULL) {
		CCrashDump::Crash();
		return 0;
	}


	ULONGLONG prevAcceptTotal = 0;

	while (true) {
		::Sleep(1000);

		s_PerfMonitor.CollectMonitorData(&hInfo);

		if (GetAsyncKeyState(VK_F1)) {
			ProfileDataOutText(L"PROFILE");
			ProfileReset();
		}
		else if (GetAsyncKeyState(VK_F2)) {
			server->Stop();
			//break;
		}


		LONG snapRecvTPS = InterlockedExchange(&server->m_RecvTPS,0);
		LONG snapSentTPS = InterlockedExchange(&server->m_SendTPS,0);
			
		if (snapRecvTPS > 3000)
		{
			s_PerfCount++;
			s_TotalRecvTPS += snapRecvTPS;
			s_TotalProcessTotalUsage += hInfo.m_ProcessTotalUsage;
		}
		

		StringCchPrintf(mLog, 4096, L"==========================================================\n");
		StringCchPrintf(mLog, 4096, L"%sHardWare\n", mLog);
		StringCchPrintf(mLog, 4096, L"%s==========================================================\n", mLog);
		StringCchPrintf(mLog, 4096, L"%sCPU [T : %0.1f% / U : %0.1f% / K : %0.1f%], Proc [T : %0.1f% / U : %0.1f% / K : %0.1f%]\n", mLog, hInfo.m_ProcessorTotalUsage, hInfo.m_ProcessorUserUsage, hInfo.m_ProcessorKernelUsage, hInfo.m_ProcessTotalUsage, hInfo.m_ProcessUserUsage, hInfo.m_ProcessKernelUsage);
		StringCchPrintf(mLog, 4096, L"%sMemory [Available : %0.0lf(MB) / NP : %0.0lf(KB)]\nProcMemory [Private : %0.0lf(KB) / NP : %0.0lf(KB)]\n", mLog, hInfo.m_AvailableMBytes, hInfo.m_NonPagedPoolBytes / 1000, hInfo.m_ProcessPrivateBytes / 1000, hInfo.m_ProcessNonPagedPoolBytes / 1000);
		StringCchPrintf(mLog, 4096, L"%sNet [Recved : %lld(B) / Sent : %lld(B)]\n\n", mLog, hInfo.m_NetRecvBytes, hInfo.m_NetSentBytes);

		StringCchPrintf(mLog, 4096, L"%s==========================================================\n", mLog);
		StringCchPrintf(mLog, 4096, L"%sLib\n", mLog);
		StringCchPrintf(mLog, 4096, L"%s==========================================================\n", mLog);

		ULONGLONG curAcceptTotal = server->m_AcceptTotal;

		StringCchPrintf(mLog, 4096, L"%sAcceptTotal : %lld / AcceptTPS : %lld\n", mLog, curAcceptTotal, curAcceptTotal - prevAcceptTotal);
		prevAcceptTotal = curAcceptTotal;

		StringCchPrintf(mLog, 4096, L"%sRecvedTPS : %d / SentTPS : %d\n", mLog, snapRecvTPS, snapSentTPS);
		StringCchPrintf(mLog, 4096, L"%sUsePacket : %d / UseChunk : %d\n", mLog, server->GetUsePacketSize(), server->GetPacketChunkSize());
		StringCchPrintf(mLog, 4096, L"%sSessionNum : %d\n\n", mLog, server->m_CurSessionCnt);
		StringCchPrintf(mLog, 4096, L"%s==========================================================\n", mLog);
		StringCchPrintf(mLog, 4096, L"%sContents\n", mLog);
		StringCchPrintf(mLog, 4096, L"%s==========================================================\n", mLog);
		StringCchPrintf(mLog, 4096, L"%sPlayerCount : %d\n", mLog, server->m_CurPlayer);
		StringCchPrintf(mLog, 4096, L"%sPQCSCnt : %d\n", mLog, server->m_PQCSCnt);
		StringCchPrintf(mLog, 4096, L"%s==========================================================\n", mLog);
		StringCchPrintf(mLog, 4096, L"%sTotalRecvTPS : %lld / AvgRecvTPS : %lf\n", mLog, s_TotalRecvTPS, (double)s_TotalRecvTPS / s_PerfCount);
		StringCchPrintf(mLog, 4096, L"%sAvg Process CPU Usage : %lf\n", mLog, s_TotalProcessTotalUsage / s_PerfCount);

		::wprintf(L"%s", mLog);
	}

	::free(mLog);
	::printf("Monitor Thread Returned ( threadId : %d )\n", GetCurrentThreadId());

	return 0;
}

ChatServer::ChatServer() : m_MonitorThread(INVALID_HANDLE_VALUE), m_MaxPlayer(0), m_RecvMsgTPS(0), m_SendMsgTPS(0),  m_CurPlayer(0), m_Players(NULL)
{
	for (int row = 0; row < MAX_POS; ++row) {
		for (int col = 0; col < MAX_POS; ++col) {
			m_Sectors[row][col] = new list<Player*>();
			InitializeSRWLock(&m_SectorLocks[row][col]);
		}
	}

	unsigned int threadId;
	m_MonitorThread = (HANDLE)::_beginthreadex(NULL, 0, Monitor, this, 0, &threadId);
}

ChatServer::~ChatServer()
{
	CloseHandle(m_MonitorThread);

	for (int row = 0; row < MAX_POS; ++row) {
		for (int col = 0; col < MAX_POS; ++col) {
			delete m_Sectors[row][col];
		}
	}
}

void ChatServer::SendSectorAround(Player* player, CPacket* pkt)
{
	short row = player->m_SectorY;
	short col = player->m_SectorX;


	short startRow, endRow, startCol, endCol;

	if (row == 0) startRow = row;
	else startRow = row - 1;

	if (row == MAX_POS - 1) endRow = row;
	else endRow = row + 1;

	if (col == 0) startCol = col;
	else startCol = col - 1;

	if (col == MAX_POS - 1) endCol = col;
	else endCol = col + 1;

	/*----------------------------------
	// Lock을 잡는 규칙을 만든다.
	// 왼쪽위 -> 오른쪽아래
	// --------------------------------*/
	for (short aroundRow = startRow; aroundRow <= endRow; ++aroundRow) {
		for (short aroundCol = startCol; aroundCol <= endCol; ++aroundCol){
			auto sector = m_Sectors[aroundRow][aroundCol];
			AcquireSRWLockShared(&m_SectorLocks[aroundRow][aroundCol]);
		}
	}

	for (short aroundRow = startRow; aroundRow <= endRow; ++aroundRow) {
		for (short aroundCol = startCol; aroundCol <= endCol; ++aroundCol) {

			auto sector = m_Sectors[aroundRow][aroundCol];

			for (auto playerIter = sector->begin(); playerIter != sector->end(); ++playerIter) {
				SendPacket((*playerIter)->m_SessionId, pkt);
				//m_MsgResTPS++;
			}
		}
	}


	for (short aroundRow = startRow; aroundRow <= endRow; ++aroundRow) {
		for (short aroundCol = startCol; aroundCol <= endCol; ++aroundCol) {
			auto sector = m_Sectors[aroundRow][aroundCol];
			ReleaseSRWLockShared(&m_SectorLocks[aroundRow][aroundCol]);
		}
	}
}

void ChatServer::PacketProc(__int64 sessionID, CPacket* pkt)
{
	WORD type;

	try {
		(*pkt) >> type;
	}
	catch (int e)
	{
		OnSessionError(sessionID, -1, L"Failed unmarhsalling.", __FILE__, __LINE__);
		return;
	}

	switch (type) {
	case en_PACKET_CS_CHAT_REQ_LOGIN:
		HandleRequestLogin(sessionID, pkt);
		break;
	case en_PACKET_CS_CHAT_REQ_SECTOR_MOVE:
		HandleRequestSectorMove(sessionID, pkt);
		break;
	case en_PACKET_CS_CHAT_REQ_MESSAGE:
		HandleRequestChatMessage(sessionID, pkt);
		break;
	case en_PACKET_CS_CHAT_REQ_HEARTBEAT:
		HandleRequestHeartbeat(sessionID);
		break;
	default:
		OnSessionError(sessionID, -1, L"Recveed unvalid type packet.", __FILE__, __LINE__);
		break;
	}

}
