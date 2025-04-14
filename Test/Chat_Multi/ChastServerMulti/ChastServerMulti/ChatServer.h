#pragma once
#include "..\..\Lib\Network\NetServer.h"
#include <unordered_map>
#include <list>
#include <string>

using namespace onenewarm;

class ChatServer : public NetServer
{
	static constexpr WORD INVALID_POS = -1;
	static constexpr WORD MAX_POS = 50;

	static constexpr WORD ID_MAX_LEN = 20;
	static constexpr WORD NICKNAME_MAX_LEN = 20;
	static constexpr WORD SESSIONKEY_LEN = 64;
	static constexpr WORD MESSAGE_MAX_LEN = 512;

	static constexpr LONG MAX_UPDATE_JOBQ_SIZE = 100000;

	struct Player
	{
		__int64 m_SessionId;
		__int64 m_PlayerId;
		WCHAR m_Id[ID_MAX_LEN]; // NULL 포함
		WCHAR m_NickName[NICKNAME_MAX_LEN]; // NULL포함

		bool m_IsLogin;

		WORD m_SectorX;
		WORD m_SectorY;

		//Debug용
		__int64 m_AccountNo;
	};

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

	void InitPlayer(Player* player);
	void ClearPlayer(Player* player);

	void HandleRequestLogin(__int64 sessionId, CPacket* pkt);
	void HandleRequestSectorMove(__int64 sessionId, CPacket* pkt);
	void HandleRequestChatMessage(__int64 sessionId, CPacket* pkt);
	void HandleRequestHeartbeat(__int64 sessionId);

public:
	static unsigned WINAPI Monitor(void* argv);

	ChatServer();
	~ChatServer();
	void SendSectorAround(Player* player, CPacket* pkt);

	void PacketProc(__int64 sessionID ,CPacket* pkt);

private:
	Player* m_Players; // MaxSession 길이의 배열
	list<Player*>* m_Sectors[MAX_POS][MAX_POS];
	SRWLOCK m_SectorLocks[MAX_POS][MAX_POS];

	HANDLE m_MonitorThread;
	int m_MaxPlayer;

	int m_CurPlayer;
public:
	alignas(64)LONG m_RecvMsgTPS;
	LONG m_SendMsgTPS;
};

