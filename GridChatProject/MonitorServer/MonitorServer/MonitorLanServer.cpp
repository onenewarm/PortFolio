#include "MonitorLanServer.h"
#include "CommonProtocol.h"
#include <Utils\PerformanceMonitor.h>
#include <strsafe.h>

PerformanceMonitor s_PerfMonitor;
PerformanceMonitor::MONITOR_INFO s_hInfo;


MonitorLanServer::CollectDataWrapper::CollectDataWrapper(BYTE type, __int64 data) : type(type), data(data)
{
}

MonitorLanServer::CollectData::CollectData(int serverNo, BYTE type) : m_ServerNo(serverNo), m_Type(type), m_Total(0), m_Count(0), m_Max(INT_MIN), m_Min(INT_MAX)
{
	InitializeSRWLock(&m_Lock);
}

void MonitorLanServer::CollectData::Clear()
{
	m_Total = 0;
	m_Count = 0;
	m_Max = INT_MIN;
	m_Min = INT_MAX;
}

void MonitorLanServer::CollectData::Add(int data)
{
	AcquireSRWLockExclusive(&m_Lock);
	m_Total += data;
	m_Count++;
	m_Max = max(m_Max, data);
	m_Min = min(m_Min, data);
	ReleaseSRWLockExclusive(&m_Lock);
}

unsigned __stdcall MonitorLanServer::Monitor(void* argv)
{
	MonitorLanServer* server = (MonitorLanServer*)argv;

	WCHAR* mLog = (WCHAR*)malloc(sizeof(WCHAR) * 4096);
	if (mLog == NULL) {
		CCrashDump::Crash();
		return 0;
	}

	while (server->m_IsStop == false) {
		::Sleep(1000);

		LONG snapLanRecvTPS = InterlockedExchange(&server->m_RecvTPS, 0);
		LONG snapNetRecvTPS = InterlockedExchange(&server->m_NetMonitorServer->m_RecvTPS, 0);
		LONG snapLanSentTPS = InterlockedExchange(&server->m_SendTPS, 0);
		LONG snapNetSentTPS = InterlockedExchange(&server->m_NetMonitorServer->m_SendTPS, 0);

		if (GetAsyncKeyState(VK_F1)) {
			ProfileDataOutText(L"PROFILE");
			ProfileReset();
		}
		else if (GetAsyncKeyState(VK_F2)) {
			server->Stop();
			//break;
		}

		StringCchPrintf(mLog, 4096, L"==========================================================\n");
		StringCchPrintf(mLog, 4096, L"%sHardware\n", mLog);
		StringCchPrintf(mLog, 4096, L"%s==========================================================\n", mLog);
		StringCchPrintf(mLog, 4096, L"%sCPU [T : %0.1f% / U : %0.1f% / K : %0.1f%], Proc [T : %0.1f% / U : %0.1f% / K : %0.1f%]\n", mLog, s_hInfo.m_ProcessorTotalUsage, s_hInfo.m_ProcessorUserUsage, s_hInfo.m_ProcessorKernelUsage, s_hInfo.m_ProcessTotalUsage, s_hInfo.m_ProcessUserUsage, s_hInfo.m_ProcessKernelUsage);
		StringCchPrintf(mLog, 4096, L"%sMemory [Available : %0.0lf(MB) / NP : %0.0lf(KB)]\nProcMemory [Private : %0.0lf(KB) / NP : %0.0lf(KB)]\n", mLog, s_hInfo.m_AvailableMBytes, s_hInfo.m_NonPagedPoolBytes / 1000, s_hInfo.m_ProcessPrivateBytes / 1000, s_hInfo.m_ProcessNonPagedPoolBytes / 1000);
		StringCchPrintf(mLog, 4096, L"%sNet [Recved : %llu(KB) / Sent : %llu(KB)]\n\n", mLog, s_hInfo.m_NetRecvBytes >> 10, s_hInfo.m_NetSentBytes >> 10);
		StringCchPrintf(mLog, 4096, L"%s==========================================================\n", mLog);
		StringCchPrintf(mLog, 4096, L"%sLib\n", mLog);
		StringCchPrintf(mLog, 4096, L"%s==========================================================\n", mLog);
		ULONGLONG curAcceptTotal = server->m_NetMonitorServer->m_AcceptTotal;
		StringCchPrintf(mLog, 4096, L"%sLan | RecvedTPS : %d / SentTPS : %d\n", mLog, snapLanRecvTPS, snapLanSentTPS);
		StringCchPrintf(mLog, 4096, L"%sNet | RecvedTPS : %d / SentTPS : %d\n", mLog, snapLanRecvTPS, snapLanSentTPS);
		StringCchPrintf(mLog, 4096, L"%sUsePacket : %d / UseChunk : %d\n", mLog, server->GetUsePacketSize(), server->GetPacketChunkSize());
		StringCchPrintf(mLog, 4096, L"%sNet | [SessionNum : %d / InLogin : %d]\n", mLog, server->m_NetMonitorServer->m_CurSessionCnt, (int)server->m_NetMonitorServer->m_ConnSessions.size());
		StringCchPrintf(mLog, 4096, L"%s==========================================================\n", mLog);
		StringCchPrintf(mLog, 4096, L"%sContents\n", mLog);
		StringCchPrintf(mLog, 4096, L"%s==========================================================\n", mLog);
		StringCchPrintf(mLog, 4096, L"%sPQCSCnt [Lan : %d / Net : %d]\n", mLog, server->m_PQCSCnt, server->m_NetMonitorServer->m_PQCSCnt);

		::wprintf(L"%s", mLog);
	}

	::free(mLog);
	::printf("Monitor Thread Returned ( threadId : %d )\n", GetCurrentThreadId());

	return 0;
}


MonitorLanServer::MonitorLanServer(MonitorNetServer* netServer) : m_NetMonitorServer(netServer), m_CollectDataMaps(NULL), m_IsServerONs(NULL), m_ServerCount(0), m_TLSDBConnections(NULL)
{
	unsigned int threadId;

	::InitializeSRWLock(&m_NumMapLock);
	::InitializeSRWLock(&m_MonitorSendLock);

	m_MonitorThread = (HANDLE)::_beginthreadex(NULL, 0, Monitor, this, 0, &threadId);
}

MonitorLanServer::~MonitorLanServer()
{
	::CloseHandle(m_MonitorThread);
}



bool MonitorLanServer::OnConnectionRequest(const char* IP, short port)
{
	// 할 일 없음
	return false;
}

void MonitorLanServer::OnClientJoin(__int64 sessionID)
{
	// 할 일 없음
}

void MonitorLanServer::OnSessionRelease(__int64 sessionID)
{
	::printf("Monitor LanSession left. - ServerNo : %d\n", m_ServerNumMap[sessionID]);

	AcquireSRWLockExclusive(&m_NumMapLock);
	if (m_ServerNumMap.find(sessionID) != m_ServerNumMap.end()) {
		int serverNo = m_ServerNumMap[sessionID];
		m_ServerNumMap.erase(sessionID);
		ReleaseSRWLockExclusive(&m_NumMapLock);
		m_IsServerONs[serverNo] = false;
		return;
	}
	ReleaseSRWLockExclusive(&m_NumMapLock);
}

void MonitorLanServer::OnRecv(__int64 sessionID, CPacketRef& pkt)
{
	WORD packetId;
	try {
		*(*pkt) >> packetId;
	}
	catch (int e) {
		OnSessionError(sessionID, -1, L"Failed unmashalled packet type.", __FILE__, __LINE__);
		return;
	}

	switch (packetId)
	{
	case EN_PACKET_SS_MONITOR_LOGIN:
	{
		HandleLoginRequest(sessionID, pkt);
		break;
	}
	case EN_PACKET_SS_MONITOR_DATA_UPDATE:
	{
		HandleUpdateRequest(sessionID, pkt);
		break;
	}
	default:
	{
		OnSessionError(sessionID, -1, L"Recved unvalid type packet.", __FILE__, __LINE__);
		break;
	}
	}
	// 로그인
	// -> 서버 모니터링 객체 생성
	// 모니터 db 로그 1분 단위로 저장하게 RegisterJob함

	// 업데이트
	// 서버 모니터링 객체에 데이터 누적, 카운팅
	// 받은 데이터는 NetServer 쪽으로 넘김
}

void MonitorLanServer::OnSessionError(__int64 sessionId, int errCode, const wchar_t* errMsg, const char* filePath, unsigned int fileLine)
{
	_LOG(LOG_LEVEL_WARNING, filePath, fileLine, errMsg);

	switch (errCode)
	{
	default:
	{
		Disconnect(sessionId);
		break;
	}
	}
}

void MonitorLanServer::OnSystemError(int errCode, const wchar_t* errMsg, const char* filePath, unsigned int fileLine)
{
	_LOG(LOG_LEVEL_ERROR, filePath, fileLine, errMsg);

	switch (errCode)
	{
	default:
	{
		CCrashDump::Crash();
		break;
	}
	}
}


void MonitorLanServer::OnStart(ConfigLoader* confLoader)
{
	confLoader->GetValue(&m_ServerCount, L"SERVICE", L"SERVER_SIZE");
	m_CollectDataMaps = new unordered_map<BYTE, CollectData*>[m_ServerCount];
	m_IsServerONs = (BOOL*)malloc(m_ServerCount);

	if (m_IsServerONs == NULL)
	{
		OnSystemError(0, L"Failed malloc.", __FILE__, __LINE__);
		return;
	}

	ZeroMemory(m_IsServerONs, m_ServerCount);

	m_IsServerONs[EN_SERVER_NO::MONITOR_SERVER] = true;

	wstring DBIP;
	char aDBIP[64];
	confLoader->GetValue(&DBIP, L"DB", L"IP");
	ConvertWtoA(aDBIP, DBIP.c_str());

	int DBPort;
	confLoader->GetValue(&DBPort, L"DB", L"PORT");

	wstring DBId;
	char aDBId[64];
	confLoader->GetValue(&DBId, L"DB", L"ID");
	ConvertWtoA(aDBId, DBId.c_str());

	wstring DBPass;
	char aDBPass[64];
	confLoader->GetValue(&DBPass, L"DB", L"PASSWORD");
	ConvertWtoA(aDBPass, DBPass.c_str());

	wstring DBSchema;
	char aDBSchema[64];
	confLoader->GetValue(&DBSchema, L"DB", L"SCHEMA");
	ConvertWtoA(aDBSchema, DBSchema.c_str());

	int DBProfileTime;
	confLoader->GetValue(&DBProfileTime, L"DB", L"PROFILE_TIME");

	m_TLSDBConnections = new DBConnectionTLS(aDBIP, DBPort, aDBId, aDBPass, aDBSchema, DBProfileTime);


	CollectData* collectDataCpuTotal = new CollectData(EN_SERVER_NO::MONITOR_SERVER, dfMONITOR_DATA_TYPE_MONITOR_CPU_TOTAL);
	CollectData* collectDataNP = new CollectData(EN_SERVER_NO::MONITOR_SERVER, dfMONITOR_DATA_TYPE_MONITOR_NONPAGED_MEMORY);
	CollectData* collectDataRecvTPS = new CollectData(EN_SERVER_NO::MONITOR_SERVER, dfMONITOR_DATA_TYPE_MONITOR_NETWORK_RECV);
	CollectData* collectDataSentTPS = new CollectData(EN_SERVER_NO::MONITOR_SERVER, dfMONITOR_DATA_TYPE_MONITOR_NETWORK_SEND);
	CollectData* collectDataAvailMem = new CollectData(EN_SERVER_NO::MONITOR_SERVER, dfMONITOR_DATA_TYPE_MONITOR_AVAILABLE_MEMORY);

	CollectDataWrapper* wrapperCpuTotal = new CollectDataWrapper(MonitorLanServer::CollectDataWrapper::COLLECT_DATA, (__int64)collectDataCpuTotal);
	CollectDataWrapper* wrapperNP = new CollectDataWrapper(MonitorLanServer::CollectDataWrapper::COLLECT_DATA, (__int64)collectDataNP);
	CollectDataWrapper* wrapperRecvTPS = new CollectDataWrapper(MonitorLanServer::CollectDataWrapper::COLLECT_DATA, (__int64)collectDataRecvTPS);
	CollectDataWrapper* wrapperSentTPS = new CollectDataWrapper(MonitorLanServer::CollectDataWrapper::COLLECT_DATA, (__int64)collectDataSentTPS);
	CollectDataWrapper* wrapperSentAvailMem = new CollectDataWrapper(MonitorLanServer::CollectDataWrapper::COLLECT_DATA, (__int64)collectDataAvailMem);

	WorkerJobRegister(60000, (__int64)wrapperCpuTotal, true, (__int64)collectDataCpuTotal);
	WorkerJobRegister(60000, (__int64)wrapperNP, true, (__int64)collectDataNP);
	WorkerJobRegister(60000, (__int64)wrapperRecvTPS, true, (__int64)collectDataRecvTPS);
	WorkerJobRegister(60000, (__int64)wrapperSentTPS, true, (__int64)collectDataSentTPS);
	WorkerJobRegister(60000, (__int64)wrapperSentAvailMem, true, (__int64)collectDataAvailMem);

	m_ServerMonitorCollectors = (CollectData**)malloc(sizeof(CollectData*) * 5);

	if (m_ServerMonitorCollectors == NULL)
	{
		CCrashDump::Crash();
		return;
	}

	m_ServerMonitorCollectors[0] = collectDataCpuTotal;
	m_ServerMonitorCollectors[1] = collectDataNP;
	m_ServerMonitorCollectors[2] = collectDataRecvTPS;
	m_ServerMonitorCollectors[3] = collectDataSentTPS;
	m_ServerMonitorCollectors[4] = collectDataAvailMem;

	CollectDataWrapper* wrapperMonitorSend = new CollectDataWrapper(MonitorLanServer::CollectDataWrapper::SEND_SERVER_MONITOR, (__int64)m_ServerMonitorCollectors);
	WorkerJobRegister(1000, (__int64)wrapperMonitorSend, true, (__int64)m_ServerMonitorCollectors);

	CollectDataWrapper* wrapperHeartbeatSend = new CollectDataWrapper(MonitorLanServer::CollectDataWrapper::SEND_HEARTBEAT, (__int64)0);
	WorkerJobRegister(1000, (__int64)wrapperHeartbeatSend, true, (__int64)m_IsServerONs);
}

void MonitorLanServer::OnStop()
{
	delete m_CollectDataMaps;
	delete m_IsServerONs;
	delete m_ServerMonitorCollectors;
}

void MonitorLanServer::OnWorker(__int64 completionKey)
{
	CollectDataWrapper* wrapper = (CollectDataWrapper*)completionKey;

	switch (wrapper->type) {
	case MonitorLanServer::CollectDataWrapper::COLLECT_DATA :
	{
		CollectData* collectData = (CollectData*)wrapper->data;
		int serverNo = collectData->m_ServerNo;

		if (m_IsServerONs[serverNo] == TRUE) {
			int collectAvg = 0;
			int collectMin = 0;
			int collectMax = 0;
			AcquireSRWLockExclusive(&collectData->m_Lock);
			if (collectData->m_Count > 0) {
				collectAvg = collectData->m_Total / collectData->m_Count;
			}
			collectMin = collectData->m_Min;
			collectMax = collectData->m_Max;
			collectData->Clear();
			ReleaseSRWLockExclusive(&collectData->m_Lock);

			DBConnection* dbConn = m_TLSDBConnections->GetDBConnection();

			struct tm newtime;
			__time64_t long_time;
			_time64(&long_time);
			_localtime64_s(&newtime, &long_time);

			while (true)
			{
				int queryRet = dbConn->SendFormattedQuery("insert into `monitorlog_%d%d` (logtime, serverno, type, average, min, max) VALUES (FROM_UNIXTIME(%d), %d, %d, %d, %d, %d);", (newtime.tm_year + 1900), newtime.tm_mon + 1, time(NULL), collectData->m_ServerNo, collectData->m_Type, collectAvg, collectMin, collectMax);

				if (queryRet == 0) {
					dbConn->FreeRes();
					break;
				}
				else if (queryRet == 1146) {
					dbConn->SendFormattedQuery("create table `monitorlog_%d%d` LIKE monitorlog", (newtime.tm_year + 1900), newtime.tm_mon + 1);
				}
				else {
					DWORD errCode = dbConn->GetErrCode();
					_LOG(LOG_LEVEL_ERROR, __FILE__, __LINE__, L"mysql sendquery failed : errcode ( queryRet : %d / errNo : %d )", queryRet, errCode);
					CCrashDump::Crash();
				}
			}

		}
		else {
			collectData->Clear();
		}
		break;
	}
	case MonitorLanServer::CollectDataWrapper::SEND_SERVER_MONITOR :
	{
		AcquireSRWLockExclusive(&m_MonitorSendLock); // 순서가 뒤바뀌는 일이 없도록 Lock을 잡음

		CollectData** collectDatas = (CollectData**)wrapper->data;
		s_PerfMonitor.CollectMonitorData(&s_hInfo);

		int cpuTotal = s_hInfo.m_ProcessorTotalUsage;
		int NPMem = s_hInfo.m_NonPagedPoolBytes / 1000 / 1000;
		int recvKByte = s_hInfo.m_NetRecvBytes >> 10;
		int sentKByte = s_hInfo.m_NetSentBytes >> 10;
		int availMem = s_hInfo.m_AvailableMBytes;

		collectDatas[0]->Add(cpuTotal);
		collectDatas[1]->Add(NPMem);
		collectDatas[2]->Add(recvKByte);
		collectDatas[3]->Add(sentKByte);
		collectDatas[4]->Add(availMem);

		int timestamp = time(NULL);

		{
			CPacketRef pkt = m_NetMonitorServer->AllocPacket();
			*(*pkt) << (WORD)EN_PACKET_CS_MONITOR_TOOL_DATA_UPDATE << (BYTE)EN_SERVER_NO::MONITOR_SERVER << (BYTE)dfMONITOR_DATA_TYPE_MONITOR_CPU_TOTAL << (int)cpuTotal << timestamp;
			m_NetMonitorServer->SendBroadcast(*pkt);
		}

		{
			CPacketRef pkt = m_NetMonitorServer->AllocPacket();
			*(*pkt) << (WORD)EN_PACKET_CS_MONITOR_TOOL_DATA_UPDATE << (BYTE)EN_SERVER_NO::MONITOR_SERVER <<(BYTE)dfMONITOR_DATA_TYPE_MONITOR_NONPAGED_MEMORY << (int)NPMem << timestamp;
			m_NetMonitorServer->SendBroadcast(*pkt);
		}

		{
			CPacketRef pkt = m_NetMonitorServer->AllocPacket();
			*(*pkt) << (WORD)EN_PACKET_CS_MONITOR_TOOL_DATA_UPDATE << (BYTE)EN_SERVER_NO::MONITOR_SERVER << (BYTE)dfMONITOR_DATA_TYPE_MONITOR_NETWORK_RECV << (int)recvKByte << timestamp;
			m_NetMonitorServer->SendBroadcast(*pkt);
		}

		{
			CPacketRef pkt = m_NetMonitorServer->AllocPacket();
			*(*pkt) << (WORD)EN_PACKET_CS_MONITOR_TOOL_DATA_UPDATE << (BYTE)EN_SERVER_NO::MONITOR_SERVER << (BYTE)dfMONITOR_DATA_TYPE_MONITOR_NETWORK_SEND << (int)sentKByte << timestamp;
			m_NetMonitorServer->SendBroadcast(*pkt);
		}

		{
			CPacketRef pkt = m_NetMonitorServer->AllocPacket();
			*(*pkt) << (WORD)EN_PACKET_CS_MONITOR_TOOL_DATA_UPDATE << (BYTE)EN_SERVER_NO::MONITOR_SERVER << (BYTE)dfMONITOR_DATA_TYPE_MONITOR_AVAILABLE_MEMORY << (int)availMem << timestamp;
			m_NetMonitorServer->SendBroadcast(*pkt);
		}

		ReleaseSRWLockExclusive(&m_MonitorSendLock);
		break;
	}
	case MonitorLanServer::CollectDataWrapper::SEND_HEARTBEAT:
	{
		int timestamp = time(NULL);

		for (int serverNo = 0 ; serverNo < m_ServerCount ; ++serverNo) {
			if (m_IsServerONs[serverNo] == TRUE) {
				CPacketRef pkt = m_NetMonitorServer->AllocPacket();
				if (serverNo == EN_SERVER_NO::LOGIN_SERVER) {
					*(*pkt) << (WORD)EN_PACKET_CS_MONITOR_TOOL_DATA_UPDATE << (BYTE)serverNo << (BYTE)dfMONITOR_DATA_TYPE_LOGIN_SERVER_RUN << (int)1 << timestamp;
				}
				else if (serverNo == EN_SERVER_NO::GAME_SERVER) {
					*(*pkt) << (WORD)EN_PACKET_CS_MONITOR_TOOL_DATA_UPDATE << (BYTE)serverNo << (BYTE)dfMONITOR_DATA_TYPE_GAME_SERVER_RUN << (int)1 << timestamp;
				}
				else if (serverNo == EN_SERVER_NO::CHAT_SERVER) {
					*(*pkt) << (WORD)EN_PACKET_CS_MONITOR_TOOL_DATA_UPDATE << (BYTE)serverNo << (BYTE)dfMONITOR_DATA_TYPE_CHAT_SERVER_RUN << (int)1 << timestamp;
				}
				else continue;
				
				m_NetMonitorServer->SendBroadcast(*pkt);
			}
		}
		break;
	}
	default:
	{
		CCrashDump::Crash();
		return;
	}
	}
}

void MonitorLanServer::OnWorkerExpired(__int64 completionKey)
{
	CollectDataWrapper* wrapper = (CollectDataWrapper*)completionKey;
	delete wrapper;
}

void MonitorLanServer::HandleLoginRequest(__int64 sessionId, CPacketRef& pkt)
{
	int serverNo;
	
	try
	{
		*(*pkt) >> serverNo;
	}
	catch (int e)
	{
		OnSessionError(sessionId, EN_PACKET_SS_MONITOR_LOGIN, L"Failed unmarshalling.", __FILE__, __LINE__);
		return;
	}
	
	// 로그인 성공
	AcquireSRWLockExclusive(&m_NumMapLock);
	if (m_ServerNumMap.find(sessionId) == m_ServerNumMap.end()) {
		m_ServerNumMap.insert({ sessionId, serverNo });
		m_IsServerONs[serverNo] = true;
		m_NetMonitorServer->m_ServerOns[serverNo] = true;
		ReleaseSRWLockExclusive(&m_NumMapLock);

		::printf("Monitor LanSession joined. - ServerNo : %d\n", serverNo);
		return;
	}
	ReleaseSRWLockExclusive(&m_NumMapLock);
}

void MonitorLanServer::HandleUpdateRequest(__int64 sessionId, CPacketRef& pkt)
{
	BYTE dataType;
	int dataValue;
	int timeStamp;

	try
	{
		*(*pkt) >> dataType >> dataValue >> timeStamp;
	}
	catch (int e)
	{
		OnSessionError(sessionId, EN_PACKET_SS_MONITOR_DATA_UPDATE, L"Failed unmarshalling.", __FILE__, __LINE__);
		return;
	}

	if (m_ServerNumMap.find(sessionId) != m_ServerNumMap.end()) {
		int serverNo = m_ServerNumMap[sessionId];

		if (m_IsServerONs[serverNo] == false) return;

		auto collectDataMap = &(m_CollectDataMaps[serverNo]);
		if (collectDataMap->find(dataType) == collectDataMap->end()) {
			// 모니터링 데이터를 누적하기 위한 객체 생성, 항목당 최초 한 번만 생성
			CollectData* collectData = new CollectData(serverNo, dataType);
			collectDataMap->insert({ dataType, collectData });
			// 1분 마다 워커스레드가 DB에 로그를 저장하도록 예약
			::printf("Register Job - ServerNo : %d , DataType : %d\n", serverNo, dataType);
			CollectDataWrapper* wrapper = new CollectDataWrapper(MonitorLanServer::CollectDataWrapper::MONITOR_LAN_WORKER_JOB::COLLECT_DATA, (__int64)collectData);
			WorkerJobRegister(60000, (__int64)wrapper, true, (__int64)collectData);
		}
		// NetClient에게 보낼 Packet 보내기
		CPacketRef netUpdateMsg = m_NetMonitorServer->AllocPacket();
		*(*netUpdateMsg) << EN_PACKET_CS_MONITOR_TOOL_DATA_UPDATE << (BYTE)serverNo << dataType << dataValue << timeStamp;
		m_NetMonitorServer->SendBroadcast(*netUpdateMsg);

		(*collectDataMap)[dataType]->Add(dataValue);
	}
	else {
		OnSessionError(sessionId, EN_PACKET_SS_MONITOR_DATA_UPDATE, L"Requested before login.", __FILE__, __LINE__);
		return;
	}

}
