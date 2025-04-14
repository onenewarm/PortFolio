#pragma once
#include <Network\LanServer.h>
#include <DB\DBConnection.h>
#include <unordered_map>
#include "MonitorNetServer.h"
#include <process.h>

using namespace onenewarm;

class MonitorLanServer : public LanServer
{
	struct CollectDataWrapper
	{
		enum MONITOR_LAN_WORKER_JOB
		{
			COLLECT_DATA,			// DB에 로그 데이터 저장
			SEND_SERVER_MONITOR,	// 외부에서 연결한 클라이언트에게 모니터링 정보를 전송
			SEND_HEARTBEAT,			// 모니터링 서버에 연결된 서버들의 On 여부를 외부 클라이언트에게 전송
		};

		CollectDataWrapper(BYTE type, __int64 data);

		BYTE type;
		__int64	data;
	};

	class CollectData
	{
	public:
		CollectData(int serverNo, BYTE type);
		void Clear();
		void Add(int data);

		int m_ServerNo;
		BYTE m_Type;
		int m_Total;
		int m_Count;
		int m_Max;
		int m_Min;

		SRWLOCK m_Lock;
	};
	public:
	static unsigned WINAPI Monitor(void* argv);

public:
	MonitorLanServer(MonitorNetServer* netServer);
	~MonitorLanServer();

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

	void		HandleLoginRequest(__int64 sessionId, CPacketRef& pkt);
	void		HandleUpdateRequest(__int64 sessionId, CPacketRef& pkt);

private:
	MonitorNetServer*					m_NetMonitorServer;
	unordered_map<BYTE, CollectData*>*	m_CollectDataMaps; // map배열, 배열의 인덱스는 serverNo, Map의 key는 모니터 데이터의 타입 
	int									m_ServerCount;
	HANDLE								m_MonitorThread;
	BOOL*								m_IsServerONs;

	DBConnectionTLS*					m_TLSDBConnections;
	SRWLOCK								m_MonitorSendLock;
	CollectData**						m_ServerMonitorCollectors;

	SRWLOCK								m_NumMapLock;
	unordered_map<__int64, int>			m_ServerNumMap; // sessionId, serverNo
};

