#pragma once
#include <Network/NetClient.h>


using namespace onenewarm;


class LoginClient : public NetClient
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

private:
	void Reply_CS_Login(__int64 sessionId, CPacket* pkt);

public:
	__int64* m_AccountNos;
	LONG* m_LastRequestedTime;

	__int64 m_Monitor_ConnectTotal;
	LONG m_Monitor_ConnetFail;
	LONG m_Monitor_DownClient;
};

