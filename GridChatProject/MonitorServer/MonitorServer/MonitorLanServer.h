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
			COLLECT_DATA,			// DB�� �α� ������ ����
			SEND_SERVER_MONITOR,	// �ܺο��� ������ Ŭ���̾�Ʈ���� ����͸� ������ ����
			SEND_HEARTBEAT,			// ����͸� ������ ����� �������� On ���θ� �ܺ� Ŭ���̾�Ʈ���� ����
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
	unordered_map<BYTE, CollectData*>*	m_CollectDataMaps; // map�迭, �迭�� �ε����� serverNo, Map�� key�� ����� �������� Ÿ�� 
	int									m_ServerCount;
	HANDLE								m_MonitorThread;
	BOOL*								m_IsServerONs;

	DBConnectionTLS*					m_TLSDBConnections;
	SRWLOCK								m_MonitorSendLock;
	CollectData**						m_ServerMonitorCollectors;

	SRWLOCK								m_NumMapLock;
	unordered_map<__int64, int>			m_ServerNumMap; // sessionId, serverNo
};

