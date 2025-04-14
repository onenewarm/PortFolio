#pragma once
#include <Network\NetServer.h>
#include <unordered_map>
#include <set>
#include <string>
#include <cpp_redis/cpp_redis>
#include "MonitorClient.h"
#include "PacketHandler.h"

#pragma comment(lib, "cpp_redis\\cpp_redis.lib")
#pragma comment(lib, "tacopie\\tacopie.lib")

using namespace onenewarm;

class ChatServer : public NetServer, PacketHandler<NetServer>
{
	static constexpr WORD INVALID_POS = -1;
	static constexpr WORD MAX_POS = 50;
	static constexpr WORD ID_MAX_LEN = 20;
	static constexpr WORD NICKNAME_MAX_LEN = 20;
	static constexpr WORD SESSIONKEY_LEN = 64;
	static constexpr WORD MESSAGE_MAX_LEN = 512;
	static constexpr LONG MAX_JOBQ_SIZE = 100000;

	enum CHAT_ERROR
	{
		SYSTEM_ERROR = 0,
		SESSION_ERROR = 10,
		LOGIN_FAILED,
	};

	struct Player
	{
		__int64 m_SessionId;
		__int64 m_PlayerId;

		bool m_IsLogin;

		WORD m_SectorX;
		WORD m_SectorY;

		WCHAR m_Id[ID_MAX_LEN]; // NULL 포함
		WCHAR m_NickName[NICKNAME_MAX_LEN]; // NULL포함

		//Debug용
		__int64 m_AccountNo;

	};

	struct Job
	{
		enum JOB_TYPE
		{
			PLAYER_RELEASE = 100,
			PLAYER_CREATE
		};

		WORD type;
		__int64 sessionId;
		Player* player;
		CPacketRef pktRef;
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

	bool Handle_EN_PACKET_CS_CHAT_REQ_LOGIN(__int64 sessionId, __int64 AccountNo, char* ID, char* Nickname, char* SessionKey);
	bool Handle_EN_PACKET_CS_CHAT_REQ_SECTOR_MOVE(__int64 sessionId, __int64 AccountNo, WORD SectorX, WORD SectorY);
	bool Handle_EN_PACKET_CS_CHAT_REQ_MESSAGE(__int64 sessionId, __int64 AccountNo, WORD MessageLen, char* Message);
	bool Handle_EN_PACKET_CS_CHAT_REQ_HEARTBEAT(__int64 sessionId);
	void Handle_PlayerCreate(Player* player);
	void Handle_PlayerRelease(__int64 sessionId);


public:
	static unsigned WINAPI Update(void* argv);
	static unsigned WINAPI Monitor(void* argv);
	static unsigned WINAPI Token(void* argv);
	
	ChatServer();
	~ChatServer();
	void SendSectorAround(Player* player, CPacket* pkt);

	void JobProc(Job* job);
protected:
	bool CheckToken(LONG64 accountNo);

protected:
	HANDLE m_UpdateEvent;
	HANDLE m_TokenEvent;
	HANDLE m_UpdateThread;
	HANDLE m_MonitorThread;
	HANDLE m_CheckTokenThread;
	int m_MaxPlayer;

	TLSObjectPool<Player>* m_PlayerPool;
	unordered_map<__int64, Player*>* m_PlayerMap;
	set<Player*>* m_Sectors[MAX_POS][MAX_POS];

	TLSObjectPool<Job>* m_JobPool;
	LockFreeQueue<Job*>* m_UpdateJobQ;
	LockFreeQueue<Job*>* m_TokenJobQ;
	cpp_redis::client* m_RedisClient;
	string m_RedisKey;


public:
	MonitorClient* m_MonitorClient;

public:
	alignas(64)LONG m_RecvMsgTPS;
	LONG m_SendMsgTPS;
	LONG m_UpdateTPS;

	LONG m_MsgReqTPS = 0;
	LONG m_MsgResTPS = 0;
	double m_AvgSector = 0;

};

