#pragma once
#include <Network/NetClient.h>

using namespace onenewarm;

class ChatClient : public NetClient
{
public:
	void OnEnterJoinServer(LONG64 sessionId);
	void OnSessionRelease(LONG64 sessionId);

	void OnRecv(LONG64 sessionId, CPacket* pkt);

	void OnError(LONG64 sessionId, int errCode, const wchar_t* msg);
	void OnStart(ConfigLoader* confLoader);
	void OnStop();

public:
	void Request_CS_Login(__int64 sessionId, const wchar_t* id, const wchar_t* nickname, __int64 accountNo, const char* sessionKey);
	void Request_CS_SectorMove(__int64 sessionId, __int64 accountNo, WORD sectorX, WORD sectorY);
	void Request_CS_Message(__int64 sessionId, __int64 accountNo, WORD messageLen, const wchar_t* message);

private:
	void PacketProc(__int64 sessionId, CPacket* pkt);

	void Reply_CS_Login(__int64 sessionId, CPacket* pkt);
	void Reply_CS_Message(__int64 sessionId, CPacket* pkt);
	void Reply_CS_SectorMove(__int64 sessionId, CPacket* pkt);

	void UpdateReplyWait(__int64 sessionId);
public:
	__int64* m_AccountNos;
	LONG* m_LastRequestedTime;
	WORD* m_LastRequestedPacket;
	WCHAR** m_RequestedMessages;

	__int64 m_Monitor_ConnectTotal;
	LONG m_Monitor_ConnetFail;
	LONG m_Monitor_DownClient;
	LONG m_Monitor_LoginSessionCnt;
	LONG m_Monitor_ReplyWait;
};

