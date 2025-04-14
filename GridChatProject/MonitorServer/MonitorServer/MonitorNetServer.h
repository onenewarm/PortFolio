#pragma once
#include <Network\NetServer.h>
#include <unordered_set>
#include <Containers/RingBuffer.h>
#include <synchapi.h>

using namespace onenewarm;

class MonitorNetServer : public NetServer
{
public:
	MonitorNetServer();
	~MonitorNetServer();

	bool		OnConnectionRequest(const char* IP, short port);
	void		OnClientJoin(__int64 sessionID);
	void		OnSessionRelease(__int64 sessionID);
	void		OnRecv(__int64 sessionID, CPacketRef& pkt);
	void		OnSessionError(__int64 sessionId, int errCode, const wchar_t* errMsg, const char* filePath, unsigned int fileLine);
	void		OnSystemError(int errCode, const wchar_t* errMsg, const char* filePath, unsigned int fileLine);
	void		OnStart(ConfigLoader* confLoader);
	void		OnStop();
	void		OnWorker(__int64 completionKey);
	void		OnWorkerExpired(__int64 completionKey);

	void	SendBroadcast(CPacket* pkt);
	void	HandleMonitorToolLogin(__int64 sessionID, CPacketRef& pkt);

public:
	std::unordered_set<__int64> m_ConnSessions;
	SRWLOCK	m_ConnSessionLock;
	char	m_LoginSessionKey[32];
public:
	bool	m_ServerOns[5];
};

