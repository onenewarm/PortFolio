#pragma once
#include <Network\LanClient.h>
#include "PacketHandler.h"

using namespace onenewarm;

class MonitorClient : public LanClient, PacketHandler<LanClient>
{
public:
	MonitorClient() : m_SessionId(0), m_ServerNo(0), m_IsConn(false)
	{

	}

	void OnEnterJoinServer(LONG64 sessionId);
	void OnSessionRelease(LONG64 sessionId);
	void OnRecv(LONG64 sessionId, CPacket* pkt);
	void OnError(LONG64 sessionId, int errCode, const wchar_t* msg);
	void OnStart(ConfigLoader* confLoader);
	void OnStop();

	void Request_EN_PACKET_SS_MONITOR_LOGIN_Packet(int ServerNo);
	void Request_EN_PACKET_SS_MONITOR_DATA_UPDATE_Packet(byte DataType, int DataValue, int TimeStamp);

	__int64 m_SessionId;
	int	m_ServerNo;
	bool m_IsConn;
};

