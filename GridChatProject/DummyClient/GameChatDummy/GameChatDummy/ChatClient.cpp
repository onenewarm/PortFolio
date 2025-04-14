#include "ChatClient.h"
#include "ClientManager.h"
#include "Protocol.h"

void ChatClient::OnEnterJoinServer(LONG64 sessionId)
{
}

void ChatClient::OnSessionRelease(LONG64 sessionId)
{
	if (IsCalledDisconnect(sessionId) == false)
	{
		InterlockedIncrement(&m_Monitor_DownClient);
	}
	else
	{
		__int64 accountNo = m_AccountNos[(WORD)sessionId];
		if (g_Characters[CHARACTER_IDX(accountNo)].m_IsChatLogin == true)
		{
			InterlockedDecrement(&m_Monitor_LoginSessionCnt);
		}
		if (g_Characters[CHARACTER_IDX(accountNo)].m_IsChatReplyWait == true)
		{
			InterlockedDecrement(&m_Monitor_ReplyWait);
		}
		if (InterlockedDecrement16(&g_Characters[CHARACTER_IDX(accountNo)].m_ConnectCount) == 0)
		{
			g_Characters[CHARACTER_IDX(accountNo)].Clear();
		}
	}
}

void ChatClient::OnRecv(LONG64 sessionId, CPacket* pkt)
{
	PacketProc(sessionId, pkt);
}

void ChatClient::OnError(LONG64 sessionId, int errCode, const wchar_t* msg)
{
	_LOG(LOG_LEVEL_ERROR, __FILE__, __LINE__, L"%s : errCode(%d)", msg, errCode);
}

void ChatClient::OnStart(ConfigLoader* confLoader)
{
	m_AccountNos = (__int64*)malloc(sizeof(__int64) * m_MaxSessions);
	m_LastRequestedTime = (LONG*)malloc(sizeof(LONG) * m_MaxSessions);
	m_LastRequestedPacket = (WORD*)malloc(sizeof(WORD) * m_MaxSessions);
	m_RequestedMessages = (WCHAR**)malloc(sizeof(WCHAR*) * m_MaxSessions);

	for (int cnt = 0; cnt < m_MaxSessions; ++cnt)
	{
		m_RequestedMessages[cnt] = (WCHAR*)malloc(sizeof(WCHAR) * MAX_CHAT_SIZE);
		m_RequestedMessages[cnt][0] = L'\0';
	}
}

void ChatClient::OnStop()
{
	free(m_AccountNos);
	free(m_LastRequestedTime);
	free(m_LastRequestedPacket);

	for (int cnt = 0; cnt < m_MaxSessions; ++cnt)
	{
		free(m_RequestedMessages[cnt]);
	}

	free(m_RequestedMessages);
}

void ChatClient::Request_CS_Login(__int64 sessionId, const wchar_t* id, const wchar_t* nickname, __int64 accountNo, const char* sessionKey)
{
	m_LastRequestedTime[(WORD)sessionId] = timeGetTime();
	m_LastRequestedPacket[(WORD)sessionId] = en_PACKET_TYPE::en_PACKET_CS_CHAT_REQ_LOGIN;
	InterlockedIncrement(&m_Monitor_ReplyWait);
	g_Characters[CHARACTER_IDX(accountNo)].m_IsChatReplyWait = true;

	CPacketRef pkt = AllocPacket();
	(*(*pkt)) << (WORD)en_PACKET_TYPE::en_PACKET_CS_CHAT_REQ_LOGIN << accountNo;
	(*pkt)->PutData((char*)id, 40);
	(*pkt)->PutData((char*)nickname, 40);
	(*pkt)->PutData(sessionKey, 64);

	SendPacket(sessionId, *pkt);
}

void ChatClient::Request_CS_SectorMove(__int64 sessionId, __int64 accountNo, WORD sectorX, WORD sectorY)
{
	CPacketRef pkt = AllocPacket();
	(*(*pkt)) << (WORD)en_PACKET_TYPE::en_PACKET_CS_CHAT_REQ_SECTOR_MOVE << accountNo << sectorX << sectorY;
	SendPacket(sessionId, *pkt);
}

void ChatClient::Request_CS_Message(__int64 sessionId, __int64 accountNo, WORD messageLen, const wchar_t* message)
{
	wmemcpy_s(m_RequestedMessages[(WORD)sessionId], MAX_CHAT_SIZE, message, messageLen / 2);
	m_LastRequestedTime[(WORD)sessionId] = timeGetTime();
	m_LastRequestedPacket[(WORD)sessionId] = en_PACKET_TYPE::en_PACKET_CS_CHAT_REQ_MESSAGE;
	InterlockedIncrement(&m_Monitor_ReplyWait);
	g_Characters[CHARACTER_IDX(accountNo)].m_IsChatReplyWait = true;

	CPacketRef pkt = AllocPacket();
	(*(*pkt)) << (WORD)en_PACKET_TYPE::en_PACKET_CS_CHAT_REQ_MESSAGE << accountNo << messageLen;
	(*pkt)->PutData((char*)message, messageLen);

	SendPacket(sessionId, *pkt);
}

void ChatClient::PacketProc(__int64 sessionId, CPacket* pkt)
{
	short type = -1;

	*pkt >> type;

	if (type == -1)
	{
		OnError(sessionId, 0, L"ChatClient / PacketProc / type Error");
		Disconnect(sessionId);
		return;
	}

	switch (type)
	{
	case en_PACKET_TYPE::en_PACKET_CS_CHAT_RES_LOGIN:
		Reply_CS_Login(sessionId, pkt);
		break;
	case en_PACKET_TYPE::en_PACKET_CS_CHAT_RES_SECTOR_MOVE:
		//Reply_CS_SectorMove(sessionId, pkt);
		break;
	case en_PACKET_TYPE::en_PACKET_CS_CHAT_RES_MESSAGE:
		Reply_CS_Message(sessionId, pkt);
		break;
	default:
		OnError(sessionId, 0, L"ChatClient / Recved Invalid Type Packet");
		Disconnect(sessionId);
		break;
	}
}

void ChatClient::Reply_CS_Login(__int64 sessionId, CPacket* pkt)
{
	UpdateReplyWait(sessionId);

	char status = -1;
	__int64 accountNo = -1;

	*pkt >> status >> accountNo;

	if (status == -1 || accountNo == -1)
	{
		OnError(sessionId, 0, L"ChatClient / Reply_CS_Login / Invalid Packet");
		Disconnect(sessionId);
		return;
	}

	if (status == 0)
	{
		OnError(sessionId, 0, L"ChatClient / Reply_CS_Login / Failed Login");
		Disconnect(sessionId);
		return;
	}

	__int64 compareAccountNo = m_AccountNos[(WORD)sessionId];

	if (accountNo != compareAccountNo)
	{
		OnError(sessionId, 0, L"ChatClient / Reply_CS_Login / Different AccountNo");
		Disconnect(sessionId);
		return;
	}

	InterlockedIncrement(&m_Monitor_LoginSessionCnt);
	Character* character = &g_Characters[CHARACTER_IDX(accountNo)];
	InterlockedExchange8((char*) & character->m_IsChatLogin, 1);
	if(character->m_IsGameLogin == true)
		character->m_Status = en_CHARACTER_STATUS::PLAY;
}

void ChatClient::Reply_CS_Message(__int64 sessionId, CPacket* pkt)
{
	__int64 accountNo = -1;
	WCHAR id[20];
	WCHAR nickname[20];
	short messageLen = -1;
	WCHAR message[MAX_CHAT_SIZE];

	__int64 myAccountNo = m_AccountNos[(WORD)sessionId];

	do
	{
		*pkt >> accountNo;

		if (accountNo == -1) break;

		if (accountNo != myAccountNo) return;



		if (pkt->GetData((char*)id, sizeof(id)) == false) break;
		if (pkt->GetData((char*)nickname, sizeof(nickname)) == false) break;
		
		*pkt >> messageLen;

		if (messageLen == -1) break;

		if (pkt->GetData((char*)message, messageLen) == false) break;

		if (wmemcmp(m_RequestedMessages[(WORD)sessionId], message, messageLen / 2) != 0)
		{
			OnError(sessionId, 0, L"ChatClient / Reply_CS_Message / Message is different.");
			CCrashDump::Crash();
		}

		UpdateReplyWait(sessionId);

		return;
	} while (0);

	OnError(sessionId, 0, L"ChatClient / Reply_CS_Message / Invalid Packet");
	Disconnect(sessionId);
	return;
}

void ChatClient::Reply_CS_SectorMove(__int64 sessionId, CPacket* pkt)
{

	__int64 accountNo = -1; short sectorX = -1; short sectorY = -1;

	do
	{ 

		*pkt >> accountNo >> sectorX >> sectorY;
		if (accountNo == -1 || sectorX == -1 || sectorY == -1) break;

		Character* character = &g_Characters[CHARACTER_IDX(accountNo)];
		
		if (character->m_SectorX != sectorX || character->m_SectorY != sectorY) DebugBreak();

	} while (0);
}


void ChatClient::UpdateReplyWait(__int64 sessionId)
{
	__int64 accountNo = m_AccountNos[(WORD)sessionId];
	g_Characters[CHARACTER_IDX(accountNo)].m_IsChatReplyWait = false;
	InterlockedDecrement(&m_Monitor_ReplyWait);
	DWORD actionDelayTime = timeGetTime() - m_LastRequestedTime[(WORD)sessionId];

	InterlockedAdd64(&g_Monitor_Action_Delay_Total, actionDelayTime);
	InterlockedIncrement64(&g_Monitor_Action_Count);
	g_Monitor_Action_Delay_Max = max(g_Monitor_Action_Delay_Max, actionDelayTime);
	g_Monitor_Action_Delay_Min = min(g_Monitor_Action_Delay_Min, actionDelayTime);
}
