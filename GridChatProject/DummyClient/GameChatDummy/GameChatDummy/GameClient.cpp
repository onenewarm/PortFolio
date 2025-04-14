#include "GameClient.h"
#include "ClientManager.h"
#include "Protocol.h"

void GameClient::OnEnterJoinServer(LONG64 sessionId)
{
}

void GameClient::OnSessionRelease(LONG64 sessionId)
{
	if (IsCalledDisconnect(sessionId) == false)
	{
		InterlockedIncrement(&m_Monitor_DownClient);
	}
	else
	{
		__int64 accountNo = m_AccountNos[(WORD)sessionId];
		if (g_Characters[CHARACTER_IDX(accountNo)].m_IsGameLogin == true)
		{
			InterlockedDecrement(&m_Monitor_LoginSessionCnt);
		}
		if (g_Characters[CHARACTER_IDX(accountNo)].m_IsGameReplyWait == true)
		{
			InterlockedDecrement(&m_Monitor_ReplyWait);
		}
		if (InterlockedDecrement16(&g_Characters[CHARACTER_IDX(accountNo)].m_ConnectCount) == 0)
		{
			g_Characters[CHARACTER_IDX(accountNo)].Clear();
		}
	}

}

void GameClient::OnRecv(LONG64 sessionId, CPacket* pkt)
{
	PacketProc(sessionId, pkt);
}


void GameClient::OnError(LONG64 sessionId, int errCode, const wchar_t* msg)
{
	_LOG(LOG_LEVEL_ERROR, __FILE__, __LINE__, L"%s : errCode(%d)", msg, errCode);
}

void GameClient::OnStart(ConfigLoader* confLoader)
{
	m_AccountNos = (__int64*)malloc(sizeof(__int64) * m_MaxSessions);
	m_LastRequestedTime = (LONG*)malloc(sizeof(LONG) * m_MaxSessions);
	m_LastRequestedPacket = (WORD*)malloc(sizeof(WORD) * m_MaxSessions);
}

void GameClient::OnStop()
{
	free(m_AccountNos);
	free(m_LastRequestedTime);
	free(m_LastRequestedPacket);
}

void GameClient::Request_CS_Login(__int64 sessionId ,__int64 accountNo, const char* sessionKey)
{
	m_LastRequestedTime[(WORD)sessionId] = timeGetTime();
	m_LastRequestedPacket[(WORD)sessionId] = en_PACKET_TYPE::en_PACKET_CS_GAME_REQ_LOGIN;
	InterlockedIncrement(&m_Monitor_ReplyWait);
	g_Characters[CHARACTER_IDX(accountNo)].m_IsGameReplyWait = true;

	CPacketRef pkt = AllocPacket();
	(*(*pkt)) << (WORD)en_PACKET_TYPE::en_PACKET_CS_GAME_REQ_LOGIN << accountNo;
	(*pkt)->PutData(sessionKey, 64);

	SendPacket(sessionId, *pkt);
}

void GameClient::Request_CS_MoveStart(__int64 sessionId, byte direction)
{
	CPacketRef pkt = AllocPacket();
	(*(*pkt)) << (WORD)en_PACKET_TYPE::en_PACKET_CS_GAME_REQ_MOVE_START << direction;

	SendPacket(sessionId, *pkt);
}

void GameClient::Request_CS_MoveStop(__int64 sessionId, short x, short y)
{
	CPacketRef pkt = AllocPacket();
	(*(*pkt)) << (WORD)en_PACKET_TYPE::en_PACKET_CS_GAME_REQ_MOVE_STOP << x << y;

	SendPacket(sessionId, *pkt);
}

void GameClient::PacketProc(__int64 sessionId ,CPacket* pkt)
{
	short type = -1;

	*pkt >> type;

	if (type == -1)
	{
		OnError(sessionId, 0, L"GameClient / PacketProc / type Error");
		Disconnect(sessionId);
		return;
	}

	__int64 accountNo = m_AccountNos[(WORD)sessionId];

	switch (type)
	{
	case en_PACKET_TYPE::en_PACKET_CS_GAME_RES_LOGIN:
		Reply_CS_Login(sessionId, pkt);
		break;
	case en_PACKET_TYPE::en_PACKET_CS_GAME_RES_CREATE_MY_CHARACTER:
		Reply_CS_CreateMyCharacter(sessionId, pkt);
		break;
	case en_PACKET_TYPE::en_PACKET_CS_GAME_RES_CREATE_OTHER_CHARACTER:
		break;
	case en_PACKET_TYPE::en_PACKET_CS_GAME_RES_DELETE_CHARACTER:
		break;
	case en_PACKET_TYPE::en_PACKET_CS_GAME_RES_MOVE_START:
		break;
	case en_PACKET_TYPE::en_PACKET_CS_GAME_RES_MOVE_STOP:
		break;
	case en_PACKET_TYPE::en_PACKET_CS_GAME_RES_SYNC:
		Reply_CS_Sync(sessionId, pkt);
		break;
	default:
		OnError(sessionId, 0, L"GameClient / Recved Invalid Type Packet");
		Disconnect(sessionId);
		break;
	}
}

void GameClient::Reply_CS_Login(__int64 sessionId, CPacket* pkt)
{
	UpdateReplyWait(sessionId);

	char status = -1;
	*pkt >> status;

	if (status == -1)
	{
		OnError(sessionId, 0, L"GameClient / Reply_CS_Login / Invalid Packet");
		Disconnect(sessionId);
		return;
	}

	if (status == 0)
	{
		OnError(sessionId, 0, L"GameClient / Reply_CS_Login / Failed Login");
		Disconnect(sessionId);
		return;
	}

	InterlockedIncrement(&m_Monitor_LoginSessionCnt);

	__int64 accountNo = m_AccountNos[(WORD)sessionId];
	Character* character = &g_Characters[CHARACTER_IDX(accountNo)];
	InterlockedExchange8((char*)&character->m_IsGameLogin, 1);
	if (character->m_IsChatLogin == true)
		character->m_Status = en_CHARACTER_STATUS::PLAY;
}

void GameClient::Reply_CS_CreateMyCharacter(__int64 sessionId, CPacket* pkt)
{
	wchar_t nickName[20];
	short x = -1; short y = -1; char direction = -1;

	do
	{
		if (pkt->GetData((char*)nickName, sizeof(nickName)) == false) break;
		*pkt >> x >> y >> direction;

		if (x == -1 || y == -1 || direction == -1) break;

		// 성공 : Character의 Game부분 초기화
		__int64 accountNo = m_AccountNos[(WORD)sessionId];
		Character* character = &g_Characters[CHARACTER_IDX(accountNo)];
		character->m_X = x;
		character->m_Y = y;
		character->m_Direction = direction;
		if (UpdateSect(character) == true)
		{
			g_ClientManager.m_ChatClient->Request_CS_SectorMove(character->m_ChatSessionId, character->m_AccountNo, character->m_SectorX, character->m_SectorY);
		}
		character->m_IsCharacterCreated = true;

		return;
	} while (0);

	OnError(sessionId, 0, L"GameClient / Reply_CS_CreateMyCharacter / Invalid Packet");
	Disconnect(sessionId);
	return;
}

void GameClient::Reply_CS_Sync(__int64 sessionId, CPacket* pkt)
{
	short x = -1; short y = -1; 
	do
	{
		*pkt >> x >> y;

		if (x == -1 || y == -1) break;

		// 성공 : Character의 Game부분 초기화
		__int64 accountNo = m_AccountNos[(WORD)sessionId];
		Character* character = &g_Characters[CHARACTER_IDX(accountNo)];
		character->m_X = x;
		character->m_Y = y;
		if (UpdateSect(character) == true)
		{
			g_ClientManager.m_ChatClient->Request_CS_SectorMove(character->m_ChatSessionId, character->m_AccountNo, character->m_SectorX, character->m_SectorY);
		}

		return;
	} while (0);

	OnError(sessionId, 0, L"GameClient / Reply_CS_Sync / Invalid Packet");
	Disconnect(sessionId);
	return;
}

void GameClient::UpdateReplyWait(__int64 sessionId)
{
	__int64 accountNo = m_AccountNos[(WORD)sessionId];
	g_Characters[CHARACTER_IDX(accountNo)].m_IsGameReplyWait = false;
	InterlockedDecrement(&m_Monitor_ReplyWait);
	DWORD actionDelayTime = timeGetTime() - m_LastRequestedTime[(WORD)sessionId];

	InterlockedAdd64(&g_Monitor_Action_Delay_Total, actionDelayTime);
	InterlockedIncrement64(&g_Monitor_Action_Count);
	g_Monitor_Action_Delay_Max = max(g_Monitor_Action_Delay_Max, actionDelayTime);
	g_Monitor_Action_Delay_Min = min(g_Monitor_Action_Delay_Min, actionDelayTime);
}
