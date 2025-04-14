#pragma once
#include <Network\MMOServer.h>
#include "GameGroup.h"
#include "AuthGroup.h"
#include "MonitorClient.h"
#include "PacketHandler.h"

namespace onenewarm
{
	class GameServer : public MMOServer, public PacketHandler<MMOServer>
	{
	public:
		static unsigned WINAPI			Monitor(void* argv);

		bool							OnConnectionRequest(const char* IP, short port);
		void							OnClientJoin(__int64 sessionID);
		void 							OnSessionRelease(__int64 sessionID);
		void 							OnRecv(__int64 sessionID, CPacketRef& pkt);
		void							OnSessionError(__int64 sessionId, int errCode, const wchar_t* errMsg, const char* filePath, unsigned int fileLine);
		void							OnSystemError(int errCode, const wchar_t* errMsg, const char* filePath, unsigned int fileLine);
		void							OnStart(ConfigLoader* confLoader);
		void							OnStop();
		void							OnWorker(__int64 completionKey);
		void							OnWorkerExpired(__int64 completionKey);

		bool							Handle_EN_PACKET_CS_GAME_REQ_LOGIN(__int64 sessionId, __int64 AccountNo, char* SessionKey);
		bool							Handle_EN_PACKET_CS_GAME_REQ_MOVE_START(__int64 sessionId, BYTE direction);
		bool							Handle_EN_PACKET_CS_GAME_REQ_MOVE_STOP(__int64 sessionId, short x, short y);


		GameServer();

	private:
		HANDLE						    m_MonitorThread;

	public:
		MonitorClient*					m_MonitorClient;

		GameGroup*						m_GameGroup;
		AuthGroup*						m_AuthGroup;
	};
}

