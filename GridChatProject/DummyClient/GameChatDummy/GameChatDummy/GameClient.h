#pragma once
#include <Network/NetClient.h>


using namespace onenewarm;

class GameClient : public NetClient
{
public:
	void OnEnterJoinServer(LONG64 sessionId);
	void OnSessionRelease(LONG64 sessionId);

	void OnRecv(LONG64 sessionId, CPacket* pkt);

	void OnError(LONG64 sessionId, int errCode, const wchar_t* msg);
	void OnStart(ConfigLoader* confLoader);
	void OnStop();
public:
	void Request_CS_Login(__int64 sessionId, __int64 accountNo, const char* sessionKey);
	void Request_CS_MoveStart(__int64 sessionId, byte direction);
	void Request_CS_MoveStop(__int64 sessionId, short x, short y);

private:
	void PacketProc(__int64 sessionId, CPacket* pkt);

	void Reply_CS_Login(__int64 sessionId, CPacket* pkt);
	void Reply_CS_CreateMyCharacter(__int64 sessionId, CPacket* pkt);
	void Reply_CS_Sync(__int64 sessionId, CPacket* pkt);
	// 나 외에 다른 유저들 옮기는 것은 더미클라에서 처리 불필요

	void UpdateReplyWait(__int64 sessionId);
public:
	__int64* m_AccountNos;
	LONG* m_LastRequestedTime;
	WORD* m_LastRequestedPacket;

	__int64 m_Monitor_ConnectTotal;
	LONG m_Monitor_ConnetFail;
	LONG m_Monitor_DownClient;
	LONG m_Monitor_LoginSessionCnt;
	LONG m_Monitor_ReplyWait;
};

