#include <Network\MMOServer.h>
#include <Utils\\Logger.h>
#include <Utils\\CCrashDump.h>
#include <iostream>
#include <process.h>
#include <conio.h>
#include <queue>
#include <Network/Group.h>

#pragma comment(lib, "Winmm.lib")
#pragma comment(lib, "Synchronization.lib")

using namespace onenewarm;

CRITICAL_SECTION g_ConsoleLock;

onenewarm::MMOServer::MMOSession::MMOSession() : m_MessageQ(new RingBuffer()), m_IsLeft(false), m_Group(NULL), m_EnterKey(0)
{
}

onenewarm::MMOServer::MMOSession::~MMOSession()
{
	delete m_MessageQ;
}

void onenewarm::MMOServer::MMOSession::OnClear()
{
	m_Group = NULL;
	m_IsLeft = false;
	CPacket* pkt;
	while (m_MessageQ->Dequeue((char*)&pkt, sizeof(CPacket*))) {
		CPacket::Free(pkt);
	}
	m_EnterKey = NULL;
	m_MessageQ->ClearBuffer();
}

void onenewarm::MMOServer::MMOSession::Init(const WORD idx, const SOCKET sock, const SOCKADDR_IN& addr)
{
	m_Id = (InterlockedIncrement64(&s_SessionIDCounter) << 16) | idx;
	m_Sock = sock;
	m_Addr = addr;
	m_LastRecvedTime = GetTickCount64();
}

bool onenewarm::MMOServer::MoveGroup(Group* group, __int64 sessionId, __int64 enterKey)
{
	MMOSession* session = AcquireSession(sessionId);
	if (session == NULL) return false;

	if (session->m_IsLeft == true) {
		if (InterlockedDecrement16(&session->m_RefCnt) == 0) {
			ReleaseSession(session);
		}
		return false;
	}
	if (session->m_Group == NULL) {
		session->m_Group = group;
		session->m_EnterKey = enterKey;
		group->m_EnterQ.Enqueue({ sessionId, enterKey});
	}
	else {
		session->m_EnterKey = enterKey;
		session->m_Group = group;
		session->m_IsLeft = true;
		//-------------------------------- 이 때 Group Update에서 OnLeave 처리를 했다?
		// 실제 Enqueue는 OnLeave 이후에 한다.
	}
	
	if (InterlockedDecrement16(&session->m_RefCnt) == 0) {
		ReleaseSession(session);
	}
	return true;
}

void onenewarm::MMOServer::AttachGroup(Group* group, DWORD fps)
{
	group->m_Server = this;
	group->m_IsAttached = true;
	group->m_FrameTime = 1000 / fps;

	group->OnAttach();

	unsigned int threadId;
	HANDLE hThread = (HANDLE)::_beginthreadex(NULL, 0, Group::Update, group, 0, &threadId);
	m_hThreads.push_back(hThread);
}

void onenewarm::MMOServer::DetachGroup(Group* group)
{
	group->m_Server = NULL;
	group->m_IsAttached = false;
	group->OnDetach();
}

void onenewarm::MMOServer::JobTimerItemRegister(DWORD delayTime, __int64 completionKey, BOOL isRepeat, DWORD repeatTime, __int64 jobKey)
{
	Job* job = Job::Alloc();

	if (isRepeat == true) {
		job->m_Type = ENUM_JOBTIMER::REGISTER_FRAME;
		job->m_Buffer << (int)delayTime << 0 << (__int64)completionKey << (__int64)ENUM_WORKER::CONTENTS << (int)repeatTime <<jobKey;
	}
	else {
		job->m_Type = ENUM_JOBTIMER::REGISTER_ONCE;
		job->m_Buffer << (int)delayTime << 0 << (__int64)completionKey << (__int64)ENUM_WORKER::CONTENTS;
	}

	if (m_JobTimerQ->Enqueue(job) == -1) CCrashDump::Crash();
}

void onenewarm::MMOServer::JobTimerItemRemove(__int64 jobKey)
{
	Job* job = Job::Alloc();

	job->m_Type = ENUM_JOBTIMER::EXPIRE_FRAME;
	job->m_Buffer << jobKey;

	if (m_JobTimerQ->Enqueue(job) == -1) CCrashDump::Crash();
}

Job* onenewarm::MMOServer::AllocJob()
{
	return Job::Alloc();
}

void onenewarm::MMOServer::SendStoreQuery(const char* queryForm, ...)
{
	int queryRet;

	char query[MAX_QUERY_SIZE];

	// 가변 인자를 쿼리 포맷을 파싱하여 하나의 문자열로 만듭니다.
	va_list args;
	va_start(args, queryForm);
	vsnprintf(query, sizeof(query), queryForm, args);
	va_end(args);

	char (*completeQuery)[MAX_QUERY_SIZE] = m_StoreQueryPool->Alloc();

	memcpy(*completeQuery, query, strlen(query) + 1);

	if (m_StoreQueryQ->Enqueue(*completeQuery) == 1)
	{
		InterlockedExchange8((char*) & m_StoreDBSignal, 1);
		WakeByAddressSingle(&m_StoreDBSignal);
	}
}

onenewarm::MMOServer::MMOServer() : m_Sessions(NULL), m_ListenSocket(INVALID_SOCKET), m_CoreIOCP(INVALID_HANDLE_VALUE), m_MaxSessions(0), m_SessionIdxes(NULL), m_IsStop(false), m_JobTimerQ(new LockFreeQueue<Job*>(10000)), m_StoreDBConnection(NULL), m_StoreQueryPool(new TLSObjectPool<char[MAX_QUERY_SIZE]>(5, false)), m_StoreQueryQ(new LockFreeQueue<char*>(5000)), m_StoreDBSignal(0), m_AcceptTotal(0), m_CurSessionCnt(0), m_SendTPS(0), m_RecvTPS(0), m_PQCSCnt(0)
{
	InitializeCriticalSection(&g_ConsoleLock);
}

onenewarm::MMOServer::~MMOServer()
{
	delete m_SessionIdxes;
	delete m_JobTimerQ;
	delete[] m_Sessions;
	delete m_StoreQueryPool;
	delete m_StoreQueryQ;

	// 스레드 핸들 정리
	for (HANDLE h : m_hThreads) {
		::CloseHandle(h);
	}
}

bool onenewarm::MMOServer::Start(const wchar_t* configFileName, bool SetNoDelay)
{
	::timeBeginPeriod(1);

	int stackSegment = 0;
	::srand((unsigned int)((LONG64)&stackSegment));

	int wsaStartUpRet, lingerRet, zeroSendBufferRet, nodelayRet, bindRet, listenRet, InetPtonRet;
	unsigned int threadId;

	// WSAStartUp
	WSADATA wsaData = { 0 };
	wsaStartUpRet = ::WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (wsaStartUpRet != 0) {
		int errCode = WSAGetLastError();
		_LOG(LOG_LEVEL_ERROR, __FILE__, __LINE__, L"Failed WSAStartUp (err : %d)", errCode);
		CCrashDump::Crash();
	}

	ConfigLoader* confLoader = new ConfigLoader();
	confLoader->LoadFile(configFileName);

	wstring BindAddr;
	confLoader->GetValue(&BindAddr, L"SERVER", L"BIND_IP");

	int BindPort;
	confLoader->GetValue(&BindPort, L"SERVER", L"BIND_PORT");

	int NumberOfConcurrentThreads;
	confLoader->GetValue(&NumberOfConcurrentThreads, L"SERVER", L"IOCP_ACTIVE_THREAD");

	int NumberOfWorkerThreads;
	confLoader->GetValue(&NumberOfWorkerThreads, L"SERVER", L"IOCP_WORKER_THREAD");

	int NumberOfMaxSessions;
	confLoader->GetValue(&NumberOfMaxSessions, L"SERVER", L"SESSION_MAX");
	m_MaxSessions = NumberOfMaxSessions;

	m_SessionIdxes = new LockFreeQueue<WORD>(NumberOfMaxSessions);

	int packetCode;
	confLoader->GetValue(&packetCode, L"SERVER", L"PACKET_CODE");
	m_PacketCode = (BYTE)packetCode;

	int packetKey;
	confLoader->GetValue(&packetKey, L"SERVER", L"PACKET_KEY");
	m_PacketKey = (BYTE)packetKey;

	wstring logLevel;
	confLoader->GetValue(&logLevel, L"SERVER", L"LOG_LEVEL");

	if (logLevel == L"DEBUG") {
		g_LogLevel = LOG_LEVEL_DEBUG;
	}
	else if (logLevel == L"WARNING") {
		g_LogLevel = LOG_LEVEL_WARNING;
	}
	else if (logLevel == L"ERROR") {
		g_LogLevel = LOG_LEVEL_ERROR;
	}

	confLoader->GetValue(&m_TimeOutIntervalLogin, L"SERVICE", L"TIMEOUT_DISCONNECT_LOGIN");
	confLoader->GetValue(&m_TimeOutIntervalNoLogin, L"SERVICE", L"TIMEOUT_DISCONNECT_NOLOGIN");

	OnStart(confLoader);

	// Session 초기화
	m_MaxSessions = NumberOfMaxSessions;
	m_Sessions = new MMOSession[NumberOfMaxSessions];
	for (WORD idx = 0; idx < NumberOfMaxSessions; ++idx) {
		LONG enQRet = m_SessionIdxes->Enqueue(idx);
		if (enQRet == -1) {
			CCrashDump::Crash();
		}
	}



	// Listen소켓 생성
	m_ListenSocket = ::socket(AF_INET, SOCK_STREAM, 0);
	if (m_ListenSocket == INVALID_SOCKET) {
		int errCode = WSAGetLastError();
		_LOG(LOG_LEVEL_ERROR, __FILE__, __LINE__, L"Failed Creating Listen Socket (err : %d)", errCode);
		CCrashDump::Crash();
	}



	// 링거옵션 셋팅
	LINGER linger = { 1, 0 };
	lingerRet = ::setsockopt(m_ListenSocket, SOL_SOCKET, SO_LINGER, (char*)&linger, sizeof(linger));
	if (lingerRet == SOCKET_ERROR) {
		int errCode = WSAGetLastError();
		_LOG(LOG_LEVEL_ERROR, __FILE__, __LINE__, L"Failed setsockopt linger (err : %d)", errCode);
		CCrashDump::Crash();
	}





	// 유저와 TCP 사이에 있는 SendBuffer의 크기를 0으로 만듦
	int sendBufSize = 0;
	zeroSendBufferRet = ::setsockopt(m_ListenSocket, SOL_SOCKET, SO_SNDBUF, (char*)&sendBufSize, sizeof(sendBufSize));
	if (zeroSendBufferRet == SOCKET_ERROR) {
		int errCode = WSAGetLastError();
		_LOG(LOG_LEVEL_ERROR, __FILE__, __LINE__, L"Failed setsockopt zero send buffer (err : %d)", errCode);
		CCrashDump::Crash();
	}



	// Off Nagle
	if (SetNoDelay == true) {
		int delayZeroOpt = 1;
		nodelayRet = ::setsockopt(m_ListenSocket, SOL_SOCKET, TCP_NODELAY, (const char*)&delayZeroOpt, sizeof(delayZeroOpt));

		if (nodelayRet == SOCKET_ERROR) {
			int errCode = WSAGetLastError();
			_LOG(LOG_LEVEL_ERROR, __FILE__, __LINE__, L"Failed setsockopt NoDelay (err : %d)", errCode);
			CCrashDump::Crash();
		}
	}



	// Listen Socket 바인딩 준비
	SOCKADDR_IN listenAddr;
	ZeroMemory(&listenAddr, sizeof(listenAddr));
	InetPtonRet = ::InetPtonW(AF_INET, BindAddr.c_str(), &listenAddr.sin_addr.s_addr);
	if (InetPtonRet != 1) {
		int errCode = WSAGetLastError();
		_LOG(LOG_LEVEL_ERROR, __FILE__, __LINE__, L"Failed InetPtonA (err : %d)", errCode);
		CCrashDump::Crash();
	}
	listenAddr.sin_family = AF_INET;
	listenAddr.sin_port = htons(BindPort);



	// 바인딩
	bindRet = ::bind(m_ListenSocket, (SOCKADDR*)&listenAddr, sizeof(listenAddr));
	if (bindRet == SOCKET_ERROR) {
		int errCode = WSAGetLastError();
		_LOG(LOG_LEVEL_ERROR, __FILE__, __LINE__, L"Failed bind (err : %d)", errCode);
		CCrashDump::Crash();
	}



	// Listen
	listenRet = ::listen(m_ListenSocket, SOMAXCONN);
	if (listenRet == SOCKET_ERROR) {
		int errCode = WSAGetLastError();
		_LOG(LOG_LEVEL_ERROR, __FILE__, __LINE__, L"Failed listen (err : %d)", errCode);
		CCrashDump::Crash();
	}



	// IOCP 인스턴스 생성
	m_CoreIOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, NumberOfConcurrentThreads);
	if (m_CoreIOCP == NULL) {
		int errCode = GetLastError();
		_LOG(LOG_LEVEL_ERROR, __FILE__, __LINE__, L"Failed creating IOCP (err : %d)", errCode);
		CCrashDump::Crash();
	}


	// IOCP 워커스레드 생성
	for (int cnt = 0; cnt < NumberOfWorkerThreads; ++cnt) {
		HANDLE worker = (HANDLE)::_beginthreadex(NULL, 0, GetCompletion, this, 0, &threadId);
		m_hThreads.push_back(worker);
	}

	HANDLE acceptThread = (HANDLE)::_beginthreadex(NULL, 0, Accept, this, 0, &threadId);
	m_hThreads.push_back(acceptThread);

	//HANDLE timeOutThread = (HANDLE)::_beginthreadex(NULL, 0, CheckTimeOut, this, 0, &threadId);
	//m_hThreads.push_back(timeOutThread);

	HANDLE jobTimerThread = (HANDLE)::_beginthreadex(NULL, 0, JobTimer, this, 0, &threadId);
	m_hThreads.push_back(jobTimerThread);

	HANDLE storeDBThread = (HANDLE)::_beginthreadex(NULL, 0, StoreDB, this, 0, &threadId);
	m_hThreads.push_back(storeDBThread);


	int NumberOfSender;
	confLoader->GetValue(&NumberOfSender, L"SERVER", L"SENDER_THREAD");

	int SendInterval;
	confLoader->GetValue(&SendInterval, L"SERVER", L"SEND_INTERVAL");

	int distributedCount = m_MaxSessions / NumberOfSender;
	int modVal = m_MaxSessions % NumberOfSender;

	for (int cnt = 0; cnt < NumberOfSender; ++cnt) {
		int sessionNum;

		if (cnt == NumberOfSender - 1) {
			sessionNum = distributedCount + modVal;
		}
		else {
			sessionNum = distributedCount;
		}

		int start = distributedCount * cnt;
		int end = start + sessionNum;

		vector<__int64>* senderArgvs = new vector<__int64>();

		senderArgvs->push_back((__int64)this);
		senderArgvs->push_back(start);
		senderArgvs->push_back(end);
		senderArgvs->push_back(SendInterval);

		HANDLE sendThread = (HANDLE)::_beginthreadex(NULL, 0, Sender, senderArgvs, 0, &threadId);
		m_hThreads.push_back(sendThread);
	}

	delete confLoader;

	return true;
}

void onenewarm::MMOServer::Stop()
{
	int cleanUpRet, closeListenRet;
	DWORD waitThreadsShutDownRet;

	// Accept 스레드 종료&정리
	closeListenRet = ::closesocket(m_ListenSocket);
	if (closeListenRet != 0) {
		int errCode = WSAGetLastError();
		_LOG(LOG_LEVEL_ERROR, __FILE__, __LINE__, L"Failed close listenSocket (err : %d)", errCode);
		CCrashDump::Crash();
	}

	// 연결 중인 Session 종료
	for (int cnt = 0; cnt < m_MaxSessions; ++cnt) {
		if (m_Sessions[cnt].m_Id != INVALID_ID)
			Disconnect(m_Sessions[cnt].m_Id);
	}

	// Session이 모두 종료 될 때까지 대기
	while (m_CurSessionCnt != 0) {
		Sleep(100);
	}

	// Worker 종료
	if (::PostQueuedCompletionStatus(m_CoreIOCP, 0, 0, (LPOVERLAPPED)ENUM_WORKER::SHUT_DOWN) == 0)
		CCrashDump::Crash();

	m_IsStop = true;

	waitThreadsShutDownRet = ::WaitForMultipleObjects((DWORD)m_hThreads.size(), &m_hThreads[0], true, INFINITE);
	if (waitThreadsShutDownRet == WAIT_FAILED) {
		int errCode = GetLastError();
		_LOG(LOG_LEVEL_ERROR, __FILE__, __LINE__, L"Failed shut down threads (err : %d)", errCode);
		CCrashDump::Crash();
	}

	// 모든 Update처리 대기
	OnStop();

	// WSA정리
	cleanUpRet = ::WSACleanup();
	if (cleanUpRet != 0) {
		int errCode = WSAGetLastError();
		_LOG(LOG_LEVEL_ERROR, __FILE__, __LINE__, L"Failed WSACleanup (err : %d)", errCode);
		CCrashDump::Crash();
	}
}

CPacketRef onenewarm::MMOServer::AllocPacket()
{
	CPacket* pkt = CPacket::Alloc();
	pkt->MoveWritePos(sizeof(NetHeader));

	return CPacketRef(pkt);
}

bool onenewarm::MMOServer::SendPacket(LONG64 sessionId, CPacket* pkt)
{
	DWORD errCode;

	MMOSession* session = AcquireSession(sessionId);

	if (session == NULL) return false; // 패킷의 refCnt = 1 이상

	//EnterCriticalSection(&g_ConsoleLock);
	pkt->NetEncode(pkt, m_PacketCode, m_PacketKey);
	//LeaveCriticalSection(&g_ConsoleLock);

	pkt->AddRef(); // 패킷의 refCnt = 2이상

	LONG sendBufSize = session->m_SendBuffer->Enqueue(pkt);

	if (sendBufSize == -1) {
		Disconnect(sessionId);

		if (InterlockedDecrement16(&session->m_RefCnt) == 0) {
			InterlockedIncrement(&m_PQCSCnt);
			if (::PostQueuedCompletionStatus(m_CoreIOCP, 0, (ULONG_PTR)session, (LPOVERLAPPED)ENUM_WORKER::RELEASE) == 0) {
				errCode = GetLastError();
				CCrashDump::Crash();
			}
			InterlockedIncrement(&m_PQCSCnt);
		}

		return false;
	}

	if (InterlockedDecrement16(&session->m_RefCnt) == 0) {
		InterlockedIncrement(&m_PQCSCnt);
		if (::PostQueuedCompletionStatus(m_CoreIOCP, 0, (ULONG_PTR)session,(LPOVERLAPPED)ENUM_WORKER::RELEASE) == 0) {
			errCode = GetLastError();
			CCrashDump::Crash();
		}
	}

	return true;
}

void onenewarm::MMOServer::Disconnect(LONG64 sessionId)
{
	BOOL cancelIORet;

	MMOSession* session = AcquireSession(sessionId);

	if (session == NULL) return;
	/*
	EnterCriticalSection(&g_ConsoleLock);
	::printf("Start Disconnect\n");
	LeaveCriticalSection(&g_ConsoleLock);
	*/

	if (InterlockedExchange8((char*)&session->m_IsDisconnected, 1) == 0) {
		cancelIORet = ::CancelIoEx((HANDLE)session->m_Sock, nullptr);

		if (cancelIORet == false) {
			int errCode = GetLastError();

			if (errCode != ERROR_NOT_FOUND) {
				_LOG(LOG_LEVEL_ERROR, __FILE__, __LINE__, L"Failed CancelIOEx (err : %d)", errCode);
				CCrashDump::Crash();
			}
		}
	}

	if (InterlockedDecrement16(&session->m_RefCnt) == 0) {
		ReleaseSession(session);
	}


	/*
	EnterCriticalSection(&g_ConsoleLock);
	::printf("End Disconnect\n");
	LeaveCriticalSection(&g_ConsoleLock);
	*/
}

void onenewarm::MMOServer::SendDisconnect(LONG64 sessionId)
{
	BOOL cancelIORet;

	MMOSession* session = AcquireSession(sessionId);

	if (session == NULL) return;

	/*
	EnterCriticalSection(&g_ConsoleLock);
	::printf("Start Disconnect\n");
	LeaveCriticalSection(&g_ConsoleLock);
	*/

	if (InterlockedExchange8((char*)&session->m_IsSendDisconnected, 1) == 1) {
		if (InterlockedDecrement16(&session->m_RefCnt) == 0) {
			ReleaseSession(session);
		}
		return;
	}

	Job* job = Job::Alloc();
	job->m_Type = ENUM_JOBTIMER::REGISTER_ONCE;
	job->m_Buffer << 20 << 0 << (__int64)session << (__int64)ENUM_WORKER::SEND_DISCONNECT;
	m_JobTimerQ->Enqueue(job);

	/*
	EnterCriticalSection(&g_ConsoleLock);
	::printf("End Disconnect\n");
	LeaveCriticalSection(&g_ConsoleLock);
	*/
}

void onenewarm::MMOServer::SetLogin(LONG64 sessionId)
{
	MMOSession* session = AcquireSession(sessionId);

	if (session == NULL) return;

	session->m_IsLogin = true;

	if (InterlockedDecrement16(&session->m_RefCnt) == 0)
		ReleaseSession(session);

}

unsigned __stdcall onenewarm::MMOServer::Accept(void* argv)
{
	int stackSegment = 0;
	::srand((unsigned int)((LONG64)&stackSegment));

	SOCKET clientSocket;
	SOCKADDR_IN clientAddr;
	int clientAddrLen = sizeof(clientAddr);

	MMOServer* server = (MMOServer*)argv;


	while (1)
	{
		ZeroMemory(&clientAddr, sizeof(SOCKADDR_IN));
		clientSocket = ::accept(server->m_ListenSocket, (SOCKADDR*)&clientAddr, &clientAddrLen);

		if (clientSocket == INVALID_SOCKET) {
			int errCode = WSAGetLastError();

			if (errCode == WSAEINTR) break;

			_LOG(LOG_LEVEL_ERROR, __FILE__, __LINE__, L"Failed accept ( err : %d )", errCode);
			CCrashDump::Crash();
		}

		server->m_AcceptTotal++;
		server->AcceptProc(clientSocket, clientAddr);
	}

	::printf("Accept Thread Returned ( threadId : %d )\n", GetCurrentThreadId());

	return 0;
}


unsigned __stdcall onenewarm::MMOServer::GetCompletion(void* argv)
{
	int stackSegment = 0;
	::srand((unsigned int)((LONG64)&stackSegment));

	MMOServer* server = (MMOServer*)argv;

	DWORD bytesTransferred = -1;
	LONG64 completionKey = -1;
	_OVERLAPPED* overlappedPtr;

	while (true)
	{
		bytesTransferred = -1;
		completionKey = -1;

		GetQueuedCompletionStatus(server->m_CoreIOCP, &bytesTransferred, (PULONG_PTR)&completionKey, (LPOVERLAPPED*)&overlappedPtr, INFINITE);
	
		if ((__int64)overlappedPtr < ENUM_WORKER::SYSTEM) {
			if (overlappedPtr == (_OVERLAPPED*)ENUM_WORKER::SHUT_DOWN && completionKey == 0 && bytesTransferred == 0) {
				// 워커 스레드의 종료
				if (::PostQueuedCompletionStatus(server->m_CoreIOCP, 0, 0, (LPOVERLAPPED)ENUM_WORKER::SHUT_DOWN) == 0) CCrashDump::Crash();
				::printf("Worker Thread Returned ( threadId : %d )\n", GetCurrentThreadId());
				break;
			}
			else {
				// 에러
				_LOG(LOG_LEVEL_ERROR, __FILE__, __LINE__, L"GQCS Error : %d", GetLastError());
				CCrashDump::Crash();
			}
		}
		else if ((__int64)overlappedPtr < ENUM_WORKER::NETWORK) {
			InterlockedDecrement(&server->m_PQCSCnt);
			if (overlappedPtr == (_OVERLAPPED*)ENUM_WORKER::SEND_DISCONNECT) {
				JobTimerItem* workerJob = (JobTimerItem*)completionKey;
				MMOSession* session = (MMOSession*)workerJob->completionKey;
				server->Disconnect(session->m_Id);

				server->m_JobTimerItemPool.Free(workerJob);

				if (InterlockedDecrement16(&session->m_RefCnt) == 0)
					server->ReleaseSession(session);
			}
			else {
				MMOSession* session = (MMOSession*)completionKey;
				if (overlappedPtr == (_OVERLAPPED*)ENUM_WORKER::SEND) {
					server->SendPost(session);
				}
				else if (overlappedPtr == (_OVERLAPPED*)ENUM_WORKER::RELEASE) {
					server->ReleaseSession(session);
					continue;
				}
				else {
					// 에러
					_LOG(LOG_LEVEL_ERROR, __FILE__, __LINE__, L"GQCS Error : %d", GetLastError());
					CCrashDump::Crash();
				}

				if (InterlockedDecrement16(&session->m_RefCnt) == 0)
					server->ReleaseSession(session);
			}
		}
		/*
		else if ((__int64)overlappedPtr < WORKER_JOB::GROUP) {
			InterlockedDecrement(&server->m_PQCSCnt);
			JobTimerItem* workerJob = (JobTimerItem*)completionKey;
			Group* group = (Group*)workerJob->completionKey;

			if (overlappedPtr == (_OVERLAPPED*)WORKER_JOB::GROUP_UPDATE) {
				group->Update();
				continue;
			}
			else {
				// 에러
				_LOG(LOG_LEVEL_ERROR, __FILE__, __LINE__, L"GQCS Error : %d", GetLastError());
				CCrashDump::Crash();
			}

			if (InterlockedDecrement16((SHORT*)&workerJob->refCnt) == 0) {
				server->m_JobTimerItemPool.Free(workerJob);
			}
		}
		*/
		else if ((__int64)overlappedPtr == ENUM_WORKER::CONTENTS) {
			InterlockedDecrement(&server->m_PQCSCnt);
			JobTimerItem* workerJob = (JobTimerItem*)completionKey;
			server->OnWorker(workerJob->completionKey);
			if (InterlockedDecrement16((SHORT*)&workerJob->refCnt) == 0) {
				server->OnWorkerExpired(workerJob->completionKey);
				server->m_JobTimerItemPool.Free(workerJob);
			}
		}
		else {
			MMOSession* session = (MMOSession*)completionKey;

			if (overlappedPtr == &session->m_RecvOverlapped) {
				// WSARecv 완료처리
				if (bytesTransferred != 0)
					server->HandleRecvCompletion(session, bytesTransferred);
			}
			else if (overlappedPtr == &session->m_SendOverlapped) {
				// WSASend의 완료처리
				server->HandleSendCompletion(session, bytesTransferred);
			}
			else {
				// 에러
				_LOG(LOG_LEVEL_ERROR, __FILE__, __LINE__, L"GQCS Error : %d", GetLastError());
				CCrashDump::Crash();
			}
		
			if (InterlockedDecrement16(&session->m_RefCnt) == 0)
				server->ReleaseSession(session);
		}
	}

	return 0;
}

unsigned __stdcall onenewarm::MMOServer::CheckTimeOut(void* argv)
{
	MMOServer* server = (MMOServer*)argv;

	const ULONGLONG CHECK_INTERVAL = 3000;


	while (server->m_IsStop == false) {
		::Sleep(CHECK_INTERVAL);

		WORD endCnt = server->m_MaxSessions;

		ULONGLONG curTime = GetTickCount64();

		for (WORD sessionCnt = 0; sessionCnt < endCnt; ++sessionCnt) {
			MMOSession* session = &(server->m_Sessions[sessionCnt]);

			if (session->m_Id == INVALID_ID) continue;

			do {
				// Session이 Releaee 상태인 경우
				if ((InterlockedIncrement16(&session->m_RefCnt) & RELEASE_FLAG) == RELEASE_FLAG) break;

				ULONGLONG dif;
				ULONGLONG snapLastRecvedTime = session->m_LastRecvedTime;
				if (curTime > snapLastRecvedTime) {
					dif = curTime - snapLastRecvedTime;
				}
				else {
					dif = snapLastRecvedTime - curTime;
				}

				if (session->m_IsLogin == TRUE) {
					if (dif >= server->m_TimeOutIntervalLogin) {
						server->Disconnect(session->m_Id);
					}
				}
				else {
					if (dif >= server->m_TimeOutIntervalNoLogin) {
						server->Disconnect(session->m_Id);
					}
				}
			} while (0);


			if (InterlockedDecrement16(&session->m_RefCnt) == 0)
				server->ReleaseSession(session);
		}
	}

	::printf("TimeOut Thread Returned ( threadId : %d )\n", GetCurrentThreadId());

	return 0;
}

unsigned __stdcall onenewarm::MMOServer::JobTimer(void* argv)
{
	struct Compare {
		bool operator()(const std::pair<DWORD, JobTimerItem*>& a, const std::pair<DWORD, JobTimerItem*>& b) const {
			return a.first > b.first; // first를 기준으로 오름차순 정렬
		}
	};

	// priority_queue / 실제로 PQCS를 해야하는 시간을 기준으로 정렬되도록 함
	// key만 알면된다.
	std::priority_queue<pair<DWORD, JobTimerItem*>, vector<pair<DWORD, JobTimerItem*>>, Compare> pq;
	std::unordered_map<__int64, JobTimerItem*> frameJobMap;

	MMOServer* server = (MMOServer*)argv;

	while (server->m_IsStop == false) {
		::Sleep(1);
		DWORD curTime = timeGetTime();

		LONG jobQSize = server->m_JobTimerQ->GetSize();
		// JobQ로 넘어온 것을 처리한다.
		while (jobQSize--) {
			Job* job;
			server->m_JobTimerQ->Dequeue(&job);

			// type별 처리를 합니다.
			switch (job->m_Type) {

			case ENUM_JOBTIMER::REGISTER_ONCE:
			{
				int delayTime = -1;
				int transferred = -1; __int64 completionKey = -1; __int64 overlapped = -1;

				job->m_Buffer >> delayTime >> transferred >> completionKey >> overlapped;
				if (delayTime == -1 || transferred == -1 || completionKey == -1 || overlapped == -1) {
					CCrashDump::Crash();
				}

				// 단발성 Job은 pq에만 추가를 합니다. id가 0인 것은 단발성 Job을 의미합니다.
				JobTimerItem* item = server->m_JobTimerItemPool.Alloc();
				item->transferred = transferred;
				item->completionKey = completionKey;
				item->overlapped = (LPOVERLAPPED)overlapped;
				item->isRepeat = false;
				item->refCnt = 1;
				pq.push({ curTime + delayTime, item });
				break;
			}

			// 0과 -1를 제외한 id를 사용 할 수 있습니다.
			case ENUM_JOBTIMER::REGISTER_FRAME:
			{
				int delayedTime = -1;
				int transferred = -1 ; __int64 completionKey = -1; __int64 overlapped = -1; 
				int repeatTime = -1; __int64 id = -1;
				

				job->m_Buffer >> delayedTime >> transferred >> completionKey >> overlapped >> repeatTime >> id;

				if (delayedTime == -1 || transferred == -1 || completionKey == -1 || overlapped == -1  || repeatTime == -1 || id == -1) {
					CCrashDump::Crash();
				}

				JobTimerItem* item = server->m_JobTimerItemPool.Alloc();
				item->transferred = transferred;
				item->completionKey = completionKey;
				item->overlapped = (LPOVERLAPPED)overlapped;
				item->isRepeat = true;
				item->repeatTime = repeatTime;
				item->refCnt = 2;
				pq.push({ curTime + delayedTime, item });

				// Map을 이용하여 일정 주기로 동작하는 Job은 모아둡니다.
				frameJobMap.insert({ id, item });
				break;
			}

			case ENUM_JOBTIMER::EXPIRE_FRAME:
			{
				__int64 id = -1;
				job->m_Buffer >> id;
				if (id == -1) CCrashDump::Crash();

				// frameJobMap에서 제거
				if (frameJobMap.find(id) != frameJobMap.end()) {
					frameJobMap.erase(id);
					JobTimerItem* workerJob = frameJobMap[id];
					workerJob->isRepeat = false;
					if (InterlockedDecrement16((SHORT*)&workerJob->refCnt) == 0) {
						server->m_JobTimerItemPool.Free(workerJob);
					}
				}
				break;
			}
			default:
				break;
			}

			// Job을 TLSObjectPool로 반환합니다.
			Job::Free(job);
		}


		// pq의 top을 확인하여 현재시간과 비교합니다.
		while (!pq.empty()) {
			pair<DWORD, JobTimerItem*> top = pq.top();
			if (top.first > curTime) break;
			pq.pop();

			// 현재시간보다 작거나 같다면 PQCS를 호출하여 워커스레드가 Job의 처리를 하게합니다.
			JobTimerItem* item = top.second;

			/*
			do
			{
				
				if ((__int64)workerJob->overlapped == WORKER_JOB::GROUP_UPDATE) {
					Group* group = (Group*)workerJob->completionKey;
					if (InterlockedIncrement16(&group->m_UpdateCount) != 1) {
						break;
					}
				}
				*/
				InterlockedIncrement(&server->m_PQCSCnt);
				if (::PostQueuedCompletionStatus(server->m_CoreIOCP, 0, (__int64)item, item->overlapped) == 0) {
					_LOG(LOG_LEVEL_ERROR, __FILE__, __LINE__, L"Failed PQCS : [errCode : %d]", GetLastError());
					CCrashDump::Crash();
				}
				/*
			} while (0);
			*/

			// id가 있다면 second 값을 가져와서 pq의 top.first에 더하여 pq에 다시 추가합니다.
			if (item->isRepeat == true) {
				InterlockedIncrement16((SHORT*)&item->refCnt);
				pq.push({ top.first + item->repeatTime, item });
			}
		}
	}

	::printf("FrameTimer Thread Returned ( threadId : %d )\n", GetCurrentThreadId());

	return 0;
}

unsigned __stdcall onenewarm::MMOServer::Sender(void* argv)
{
	vector<__int64>& argvs = *(vector<__int64>*)argv;
	MMOServer* server = (MMOServer*)argvs[0];
	__int64 startIdx = argvs[1];
	__int64 endIdx = argvs[2];
	DWORD sendInterval = (DWORD)argvs[3];

	while (server->m_IsStop == false)
	{
		::Sleep(sendInterval);
		for (int cnt = startIdx; cnt < endIdx; ++cnt) {
			MMOSession* session = &(server->m_Sessions[cnt]);
			if ((session->m_RefCnt & RELEASE_FLAG) == RELEASE_FLAG || session->m_IsSent == TRUE) continue;

			session = server->AcquireSession(session->m_Id);

			if (session == NULL) continue;

			if (session->m_IsDisconnected == FALSE && session->m_SendBuffer->GetSize() > 0) {
				if (InterlockedExchange8((char*)&session->m_IsSent, 1) == 0) {
					server->SendPost(session);
				}
			}
			if (InterlockedDecrement16(&session->m_RefCnt) == 0)
				server->ReleaseSession(session);
		}
	}

	return 0;
}

unsigned __stdcall onenewarm::MMOServer::StoreDB(void* argv)
{
	MMOServer* server = (MMOServer*)argv;

	char undesired = 0;

	while (!server->m_IsStop)
	{
		WaitOnAddress(&server->m_StoreDBSignal, &undesired, sizeof(undesired), INFINITE);

		InterlockedExchange8((char*) & server->m_StoreDBSignal, 0);

		int qSize = server->m_StoreQueryQ->GetSize();

		while (qSize--)
		{
			char* query;
			server->m_StoreQueryQ->Dequeue(&query);
			server->m_StoreDBConnection->SendQuery(query);
		}
	}

	return 0;
}


MMOServer::MMOSession* onenewarm::MMOServer::AcquireSession(LONG64 sessionId)
{
	if (sessionId == INVALID_ID) return NULL;

	MMOSession* session = &m_Sessions[sessionId & 0xFFFF];

	WORD refCnt = InterlockedIncrement16(&session->m_RefCnt);

	if ((refCnt & RELEASE_FLAG) == RELEASE_FLAG || session->m_Id != sessionId) {

		if (InterlockedDecrement16(&session->m_RefCnt) == 0)
			ReleaseSession(session);
		return NULL;
	}

	return session;
}

BOOL onenewarm::MMOServer::RecvPost(MMOSession* session)
{
	if (session->m_IsDisconnected == TRUE || session->m_IsSendDisconnected == TRUE) return FALSE;

	int wsaRecvRet;

	WSABUF buf;
	ZeroMemory(&buf, sizeof(buf));
	ZeroMemory(&session->m_RecvOverlapped, sizeof(session->m_RecvOverlapped));

	buf.buf = session->m_RecvBuffer->GetWritePtr();
	buf.len = session->m_RecvBuffer->GetBufferSize() - session->m_RecvBuffer->GetDataSize();

	if (buf.len == 0) {
		Disconnect(session->m_Id);
		return FALSE;
	}

	DWORD flags = 0;
	DWORD recvBytes = 0;

	InterlockedIncrement16(&session->m_RefCnt);

	//PRO_BEGIN(L"WSARecv");
	wsaRecvRet = WSARecv(session->m_Sock, &buf, 1, &recvBytes, &flags, &session->m_RecvOverlapped, NULL);
	//PRO_END(L"WSARecv");

	if (wsaRecvRet == SOCKET_ERROR) {
		int errCode = WSAGetLastError();
		if (errCode != WSA_IO_PENDING) {
			if (errCode == WSAECONNRESET || errCode == WSAECONNABORTED || errCode == WSAEHOSTDOWN || errCode == WSAEINTR) {
				InterlockedDecrement16(&session->m_RefCnt);
				return false;
			}
			else {
				_LOG(LOG_LEVEL_ERROR, __FILE__, __LINE__, L"Failed WSARecv ( err : %d )", errCode);
				CCrashDump::Crash();
			}
		}
	}
	return true;
}

BOOL onenewarm::MMOServer::SendPost(MMOSession* session)
{
	if (session->m_IsDisconnected == TRUE) return FALSE;

	int wsaSendRet;

	WSABUF wsaBufs[MAX_SENT_PACKET_CNT];
	InterlockedIncrement16(&session->m_RefCnt);

	int deqCnt = 0;

	while (true) {
		CPacket* pkt = NULL;

		if (deqCnt == MAX_SENT_PACKET_CNT) break;

		if (session->m_SendBuffer->Dequeue(&pkt) == false) break; // 0인 상태이기 때문에 탈출

		wsaBufs[deqCnt].buf = pkt->GetReadPtr();
		wsaBufs[deqCnt].len = pkt->GetDataSize();

		session->m_SendPackets[deqCnt] = pkt;
		++deqCnt;
	}

	session->m_SendPacketsSize = deqCnt;

	wsaSendRet = WSASend(session->m_Sock, wsaBufs, deqCnt, NULL, 0, &session->m_SendOverlapped, NULL);

	if (wsaSendRet == SOCKET_ERROR) {

		int errCode = WSAGetLastError();

		if (errCode != WSA_IO_PENDING) {
			if (errCode == WSAECONNRESET || errCode == WSAECONNABORTED || errCode == WSAEHOSTDOWN || errCode == WSAEINTR) {

				InterlockedDecrement16(&session->m_RefCnt);

				for (int cnt = 0; cnt < deqCnt; ++cnt) {
					CPacket::Free(session->m_SendPackets[cnt]);
				}

				return false;
			}
			else {
				_LOG(LOG_LEVEL_ERROR, __FILE__, __LINE__, L"Failed WSASend ( err : %d )", errCode);
				CCrashDump::Crash();
			}
		}
	}

	return true;
}


MMOServer::MMOSession* onenewarm::MMOServer::CreateSession(const SOCKET sock, const SOCKADDR_IN& addr)
{
	WORD sessionIdx;

	if (!m_SessionIdxes->Dequeue(&sessionIdx)) {
		_LOG(LOG_LEVEL_ERROR, __FILE__, __LINE__, L"Error : already allocated max session");
		CCrashDump::Crash();
	}

	MMOSession* session = &m_Sessions[sessionIdx];
	session->Clear();
	session->Init(sessionIdx, sock, addr);

	return session;
}

bool onenewarm::MMOServer::ReleaseSession(MMOSession* session)
{
	if (InterlockedCompareExchange16(&session->m_RefCnt, RELEASE_FLAG, 0) == 0) {
		if(session->m_Group == NULL) OnSessionRelease(session->m_Id);

		WORD idx = (WORD)session->m_Id;
		session->Clear();

		LONG enQRet = m_SessionIdxes->Enqueue(idx);
		if (enQRet == -1) {
			CCrashDump::Crash();
		}

		InterlockedDecrement16((SHORT*)&m_CurSessionCnt);

		return true;
	}

	return false;
}

void onenewarm::MMOServer::AcceptProc(const SOCKET sock, SOCKADDR_IN& addr)
{
	InterlockedIncrement16((SHORT*)&m_CurSessionCnt);

	//Session을 등록을 하는 절차
	MMOSession* session = CreateSession(sock, addr);
	
	InterlockedIncrement16(&session->m_RefCnt);

	InterlockedAnd16(&session->m_RefCnt, 0x7fff);

	//IOCP등록
	if (CreateIoCompletionPort((HANDLE)sock, m_CoreIOCP, (ULONG_PTR)session, NULL) == NULL) {
		DWORD errCode = GetLastError();

		if (errCode != WSAECONNRESET) {// 상대측에서 RST를 쏜 상황은 에러 상황이 아님	
			_LOG(LOG_LEVEL_ERROR, __FILE__, __LINE__, L"Session associated IOCP Error : %d", errCode);
			CCrashDump::Crash();
		}
	}
	else {
		if (RecvPost(session) == TRUE) {
			OnClientJoin(session->m_Id);
		}

	}

	if (InterlockedDecrement16(&session->m_RefCnt) == 0)
		ReleaseSession(session);
}

void onenewarm::MMOServer::HandleRecvCompletion(MMOSession* session, DWORD bytesTransferred)
{
	session->m_LastRecvedTime = ::GetTickCount64();

	CPacket* recvBuffer = session->m_RecvBuffer;

	recvBuffer->MoveWritePos(bytesTransferred); // TCP에 의해서 버퍼에 들어온 사이즈 만큼 포지션을 이동

	int recvCount = 0;

	while (recvBuffer->GetDataSize() >= sizeof(NetHeader)) {
		// 헤더를 Peek해서 헤더의 len을 먼저 확인하여 예외처리
		recvCount++;
		NetHeader* header = (NetHeader*)recvBuffer->GetReadPtr();
		if (header->len > (recvBuffer->GetBufferSize() - sizeof(NetHeader))) {
			Disconnect(session->m_Id);
			return;
		}

		if (recvBuffer->GetDataSize() < sizeof(NetHeader) + header->len) break;

		recvBuffer->Decode((char*)&header->checkSum, m_PacketKey, header->randKey, header->len + 1);

		if (IsInvalidPacket(recvBuffer->GetReadPtr()) == false) {
			Disconnect(session->m_Id);
			return;
		}

		recvBuffer->MoveReadPos(sizeof(NetHeader)); // Network Lib의 헤더만큼의 포지션을 이동

		CPacket* payloadOfMsg = CPacket::Alloc();

		payloadOfMsg->PutData(recvBuffer->GetReadPtr(), header->len);
		recvBuffer->MoveReadPos(header->len);


		if (session->m_Group != NULL) {
			session->m_MessageQ->Enqueue((char*)&payloadOfMsg, sizeof(CPacket*));
		}
		else {
			CPacketRef pktRef = CPacketRef(payloadOfMsg);
			OnRecv(session->m_Id, pktRef); // 콘텐츠단
		}
		
	}
	InterlockedAdd((LONG*) & m_RecvTPS, recvCount);

	int usedRecvBufferSize = recvBuffer->GetDataSize();

	::memcpy(recvBuffer->GetBufferPtr(), recvBuffer->GetReadPtr(), usedRecvBufferSize);
	recvBuffer->Clear();
	recvBuffer->MoveWritePos(usedRecvBufferSize);

	RecvPost(session);
}

void onenewarm::MMOServer::HandleSendCompletion(MMOSession* session, DWORD bytesTransferred)
{
	InterlockedAdd((LONG*)&m_SendTPS, (LONG)session->m_SendPacketsSize);

	for (SHORT cnt = 0; cnt < session->m_SendPacketsSize; ++cnt) {
		CPacket::Free(session->m_SendPackets[cnt]);
	}

	session->m_SendPacketsSize = 0;

	InterlockedExchange8((CHAR*)&session->m_IsSent, 0);
}

bool onenewarm::MMOServer::IsInvalidPacket(const char* pkt)
{
	if (*pkt != m_PacketCode) return false;

	const NetHeader* header = (NetHeader*)pkt;

	const char* payloadPtr = pkt + sizeof(NetHeader);

	DWORD sumPayloadsPerByte = 0;
	WORD sumPayloadPos = 0;

	while (sumPayloadPos < header->len) {
		sumPayloadsPerByte += payloadPtr[sumPayloadPos++];
	}

	if (header->checkSum != sumPayloadsPerByte % 256) return false;

	return true;
}
