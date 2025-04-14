#include "LoginClient.h"
#include "Protocol.h"
#include "ClientManager.h"

void LoginClient::OnEnterJoinServer(LONG64 sessionId)
{
	// 이 때는 AccountNo를 알 수 없음
}

void LoginClient::OnSessionRelease(LONG64 sessionId)
{
	if (IsCalledDisconnect(sessionId) == false)
	{
		InterlockedIncrement(&m_Monitor_DownClient);
	}
}

void LoginClient::OnRecv(LONG64 sessionId, CPacket* pkt)
{
	Reply_CS_Login(sessionId, pkt);
	LONG actionDelayTime = timeGetTime() - m_LastRequestedTime[(WORD)sessionId];
	InterlockedAdd64(&g_Monitor_Action_Delay_Total, actionDelayTime);
	InterlockedIncrement64(&g_Monitor_Action_Count);

	g_Monitor_Action_Delay_Max = max(g_Monitor_Action_Delay_Max, actionDelayTime);
	g_Monitor_Action_Delay_Min = min(g_Monitor_Action_Delay_Min, actionDelayTime);
}

void LoginClient::OnError(LONG64 sessionId, int errCode, const wchar_t* msg)
{
	_LOG(LOG_LEVEL_ERROR, __FILE__, __LINE__, L"%s : errCode(%d)", msg, errCode);
}

void LoginClient::OnStart(ConfigLoader* confLoader)
{
	m_AccountNos = (__int64*)malloc(sizeof(__int64) * m_MaxSessions);
	m_LastRequestedTime = (LONG*)malloc(sizeof(LONG) * m_MaxSessions);
}

void LoginClient::OnStop()
{
	free(m_AccountNos);
	free(m_LastRequestedTime);
}


void LoginClient::Request_CS_Login(__int64 sessionId ,__int64 accountNo, const char* sessionKey)
{
	m_LastRequestedTime[(WORD)sessionId] = timeGetTime();

	CPacketRef pkt = AllocPacket();

	(*(*pkt)) << (WORD)en_PACKET_TYPE::en_PACKET_CS_LOGIN_REQ_LOGIN << accountNo;
	(*pkt)->PutData(sessionKey, 64);

	SendPacket(sessionId, *pkt);
}

void LoginClient::Reply_CS_Login(__int64 sessionId, CPacket* pkt)
{
	__int64 curAccountNo = m_AccountNos[(WORD)sessionId];
	Character* character = &g_Characters[CHARACTER_IDX(curAccountNo)];


	short type = -1; __int64 accountNo = -1; char status;
	wchar_t gameServerIP[16]; short gameServerPort = -1;
	wchar_t chatServerIP[16]; short chatServerPort = -1;
	char sessionKey[64];
	do
	{
		*pkt >> type >> accountNo >> status;
		if (type == -1 || accountNo == -1 || status == -1) break;
		
		if (curAccountNo != accountNo) break;
		if (status != 1) break;

		if (pkt->GetData(character->m_GameSessionKey, 64) == false) break;
		if (pkt->GetData(character->m_ChatSessionKey, 64) == false) break;

		if (pkt->GetData((char*)character->m_ID, 40) == false) break;
		if (pkt->GetData((char*)character->m_Nickname, 40) == false) break;
		if (pkt->GetData((char*)gameServerIP, 32) == false) break;
		*pkt >> gameServerPort;
		if (gameServerPort == -1) break;
		if (pkt->GetData((char*)chatServerIP, 32) == false) break;
		*pkt >> chatServerPort;
		if (chatServerPort == -1) break;


		// 원래 같으면 IP와 Port를 저장해서 그걸로 각각 서버에 연결을 해야한다.
		// 하나의 월드 서버에 여러 채널이 분산된 구조라면 이 구조가 잘 먹힐것임
		// 근데 지금은 분산이 안 돼 있으니까 넘어가자~

		g_Characters[CHARACTER_IDX(curAccountNo)].m_Status = en_CHARACTER_STATUS::GAME_CONNECT;

		Disconnect(sessionId); // Login을 했으면 로그인서버와의 접속을 종료한다.
		return;
	} while (0);

	OnError(sessionId, 0, L"Login Server / Reply_CS_Login");
	CCrashDump::Crash();
	Disconnect(sessionId);
	return;
}
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   