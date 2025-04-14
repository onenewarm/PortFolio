#pragma once
#include <Network\NetServer.h>
#include <DB\DBConnection.h>
#include <cpp_redis/cpp_redis>
#include "MonitorClient.h"
#include "PacketHandler.h"

#pragma comment(lib, "cpp_redis\\cpp_redis.lib")
#pragma comment(lib, "tacopie\\tacopie.lib")

using namespace onenewarm;

class LoginServer : public NetServer, PacketHandler<NetServer>
{
public:
	static unsigned __stdcall Monitor(void* argv);

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

	bool		Handle_EN_PACKET_CS_LOGIN_REQ_LOGIN(__int64 sessionId, __int64 AccountNo, char* SessionKey);
	
	LoginServer();
	~LoginServer();
private:
	void StoreToken(const char* key, LONG64 value);

private:
	HANDLE m_MonitorThread;
	DBConnectionTLS* m_TLSDBConnections;
	wchar_t m_ChatServerIP[16];
	WORD m_ChatServerPort;

	wchar_t m_GameServerIP[16];
	WORD m_GameServerPort;

	cpp_redis::client* m_RedisClient;
	
	DWORD m_TlsTokenIdx;
	std::vector<std::string> m_TlsTokenKeys;
	std::vector<std::string> m_TlsTokenValues;
	SRWLOCK m_TokenLock;

public:
	MonitorClient* m_MonitorClient;
	DWORD			m_AuthTPS;
};

