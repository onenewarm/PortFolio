#include "MonitorClient.h"
#include "CommonProtocol.h"

void MonitorClient::OnEnterJoinServer(LONG64 sessionId)
{
	m_SessionId = sessionId;
	Request_EN_PACKET_SS_MONITOR_LOGIN_Packet(m_ServerNo);
}

void MonitorClient::OnSessionRelease(LONG64 sessionId)
{
	m_SessionId = NULL;
}

void MonitorClient::OnRecv(LONG64 sessionId, CPacket* pkt)
{
}

void MonitorClient::OnError(LONG64 sessionId, int errCode, const wchar_t* msg)
{
}

void MonitorClient::OnStart(ConfigLoader* confLoader)
{
	int serverNo;
	confLoader->GetValue(&serverNo, L"SERVICE", L"SERVER_TYPE");
	m_ServerNo = serverNo;
}

void MonitorClient::OnStop()
{
}


void MonitorClient::Request_EN_PACKET_SS_MONITOR_LOGIN_Packet(int ServerNo)
{
	CPacketRef pktRef = Make_EN_PACKET_SS_MONITOR_LOGIN_Packet(this, ServerNo);
	SendPacket(m_SessionId, *pktRef);
}

void MonitorClient::Request_EN_PACKET_SS_MONITOR_DATA_UPDATE_Packet(byte DataType, int DataValue, int TimeStamp)
{
	CPacketRef pktRef = Make_EN_PACKET_SS_MONITOR_DATA_UPDATE_Packet(this, DataType, DataValue, TimeStamp);
	SendPacket(m_SessionId, *pktRef);
}