#include <Network\LanServer.h>
#include <Utils\Logger.h>
#include <Utils\\CCrashDump.h>
#include <Utils\ConfigLoader.hpp>
#include <iostream>
#include <process.h>
#include <queue>

using namespace onenewarm;
using namespace std;

//#define _LANSERVER_DEBUG_

#ifdef _LANSERVER_DEBUG_
struct LANSERVER_DEBUG {
	BYTE pkt[20];
	int size;
};


LANSERVER_DEBUG g_lanLog[30000];

short g_lanLogIdx = 0;
#endif

onenewarm::LanServer::LanServer() : m_Sessions(NULL), m_ListenSocket(INVALID_SOCKET), m_CoreIOCP(INVALID_HANDLE_VALUE), m_MaxSessions(0), m_SessionIdxes(NULL), m_IsStop(false), m_JobTimerQ(new LockFreeQueue<CPacket*>(10000)), m_AcceptTotal(0), m_CurSessionCnt(0), m_SendTPS(0), m_RecvTPS(0), m_PQCSCnt(0), m_PacketCode(0)
{
}

onenewarm::LanServer::~LanServer()
{
	delete m_JobTimerQ;
	delete m_JobTimerQ;
	delete[] m_Sessions;

	// ������ �ڵ� ����
	for (HANDLE h : m_hThreads) {
		::CloseHandle(h);
	}
}


bool onenewarm::LanServer::Start(const wchar_t* configFileName, bool SetNoDelay)
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


	OnStart(confLoader);

	delete confLoader;

	// Session �ʱ�ȭ
	m_MaxSessions = NumberOfMaxSessions;
	m_Sessions = new LanSession[NumberOfMaxSessions];
	for (WORD idx = 0; idx < NumberOfMaxSessions; ++idx) {
		LONG enQRet = m_SessionIdxes->Enqueue(idx);
		if (enQRet == -1) {
			CCrashDump::Crash();
		}
	}



	// Listen���� ����
	m_ListenSocket = ::socket(AF_INET, SOCK_STREAM, 0);
	if (m_ListenSocket == INVALID_SOCKET) {
		int errCode = WSAGetLastError();
		_LOG(LOG_LEVEL_ERROR, __FILE__, __LINE__, L"Failed Creating Listen Socket (err : %d)", errCode);
		CCrashDump::Crash();
	}



	// ���ſɼ� ����
	LINGER linger = { 1, 0 };
	lingerRet = ::setsockopt(m_ListenSocket, SOL_SOCKET, SO_LINGER, (char*)&linger, sizeof(linger));
	if (lingerRet == SOCKET_ERROR) {
		int errCode = WSAGetLastError();
		_LOG(LOG_LEVEL_ERROR, __FILE__, __LINE__, L"Failed setsockopt linger (err : %d)", errCode);
		CCrashDump::Crash();
	}





	// ������ TCP ���̿� �ִ� SendBuffer�� ũ�⸦ 0���� ����
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



	// Listen Socket ���ε� �غ�
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



	// ���ε�
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



	// IOCP �ν��Ͻ� ����
	m_CoreIOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, NumberOfConcurrentThreads);
	if (m_CoreIOCP == NULL) {
		int errCode = GetLastError();
		_LOG(LOG_LEVEL_ERROR, __FILE__, __LINE__, L"Failed creating IOCP (err : %d)", errCode);
		CCrashDump::Crash();
	}


	// IOCP ��Ŀ������ ����
	for (int cnt = 0; cnt < NumberOfWorkerThreads; ++cnt) {
		HANDLE worker = (HANDLE)::_beginthreadex(NULL, 0, GetCompletion, this, 0, &threadId);
		m_hThreads.push_back(worker);
	}

	// Accept ������ ����
	HANDLE acceptThread = (HANDLE)::_beginthreadex(NULL, 0, Accept, this, 0, &threadId);
	m_hThreads.push_back(acceptThread);

	// JobTimer ������ ����
	HANDLE jobTimerThread = (HANDLE)::_beginthreadex(NULL, 0, JobTimer, this, 0, &threadId);
	m_hThreads.push_back(jobTimerThread);

	// ��Ƽ� ������ Send ������ ����
	HANDLE sendThread = (HANDLE)::_beginthreadex(NULL, 0, Sender, this, 0, &threadId);
	m_hThreads.push_back(sendThread);

	return true;
}

void onenewarm::LanServer::Stop()
{
	int cleanUpRet, closeListenRet;
	DWORD waitThreadsShutDownRet;

	// Accept ������ ����&����
	closeListenRet = ::closesocket(m_ListenSocket);
	if (closeListenRet != 0) {
		int errCode = WSAGetLastError();
		_LOG(LOG_LEVEL_ERROR, __FILE__, __LINE__, L"Failed close listenSocket (err : %d)", errCode);
		CCrashDump::Crash();
	}

	// ���� ���� Session ����
	for (int cnt = 0; cnt < m_MaxSessions; ++cnt) {
		if (m_Sessions[cnt].m_Id != INVALID_ID)
			Disconnect(m_Sessions[cnt].m_Id);
	}

	// Session�� ��� ���� �� ������ ���
	while (m_CurSessionCnt != 0) {
		Sleep(1);
	}

	// Worker ����
	if (::PostQueuedCompletionStatus(m_CoreIOCP, 0, 0, (LPOVERLAPPED)WORKER_JOB::SHUT_DOWN) == 0)
		CCrashDump::Crash();

	m_IsStop = true;

	waitThreadsShutDownRet = ::WaitForMultipleObjects((DWORD)m_hThreads.size(), &m_hThreads[0], true, INFINITE);
	if (waitThreadsShutDownRet == WAIT_FAILED) {
		int errCode = GetLastError();
		_LOG(LOG_LEVEL_ERROR, __FILE__, __LINE__, L"Failed shut down threads (err : %d)", errCode);
		CCrashDump::Crash();
	}

	// ��� Updateó�� ���
	OnStop();

	// WSA����
	cleanUpRet = ::WSACleanup();
	if (cleanUpRet != 0) {
		int errCode = WSAGetLastError();
		_LOG(LOG_LEVEL_ERROR, __FILE__, __LINE__, L"Failed WSACleanup (err : %d)", errCode);
		CCrashDump::Crash();
	}
}


CPacketRef onenewarm::LanServer::AllocPacket()
{
	CPacket* pkt = CPacket::Alloc();
	pkt->MoveWritePos(sizeof(LanHeader));

	return CPacketRef(pkt);
}


LanServer::LanSession* onenewarm::LanServer::AcquireSession(LONG64 sessionId)
{
	LanSession* session = &m_Sessions[(WORD)sessionId];

	WORD refCnt = InterlockedIncrement16(&session->m_RefCnt);

	if ( (refCnt & RELEASE_FLAG) == RELEASE_FLAG || session->m_Id != sessionId) {

		if (InterlockedDecrement16(&session->m_RefCnt) == 0)
			ReleaseSession(session);
		return NULL;
	}

	return session;
}


BOOL onenewarm::LanServer::RecvPost(LanSession* session)
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
				_LOG(LOG_LEVEL_WARNING, __FILE__, __LINE__, L"Failed WSARecv ( err : %d )", errCode);

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

BOOL onenewarm::LanServer::SendPost(LanServer::LanSession* session)
{
	if (session->m_IsDisconnected == TRUE) return FALSE;

	int wsaSendRet;

	WSABUF wsaBufs[MAX_SENT_PACKET_CNT];

	InterlockedIncrement16(&session->m_RefCnt);

	int deqCnt = 0;

	while (true) {
		CPacket* pkt = NULL;

		if (deqCnt == MAX_SENT_PACKET_CNT) break;

		if (session->m_SendBuffer->Dequeue(&pkt) == false) break; // 0�� �����̱� ������ Ż��

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


bool onenewarm::LanServer::SendPacket(LONG64 sessionId, CPacket* pkt)
{
	DWORD errCode;

	LanSession* session = AcquireSession(sessionId);

	if (session == NULL) return false; // ��Ŷ�� refCnt = 1 �̻�

	//EnterCriticalSection(&g_ConsoleLock);
	pkt->LanEncode(pkt, m_PacketCode);
	//LeaveCriticalSection(&g_ConsoleLock);

	pkt->AddRef(); // ��Ŷ�� refCnt = 2�̻�

	LONG sendBufSize = session->m_SendBuffer->Enqueue(pkt);

	if (sendBufSize == -1) {
		Disconnect(sessionId);
		CPacket::Free(pkt);

		if (InterlockedDecrement16(&session->m_RefCnt) == 0) {
			InterlockedIncrement(&m_PQCSCnt);
			if (::PostQueuedCompletionStatus(m_CoreIOCP, 0, (ULONG_PTR)session, (LPOVERLAPPED)WORKER_JOB::RELEASE) == 0) {
				errCode = GetLastError();
				CCrashDump::Crash();
			}
		}

		return false;
	}

	if (InterlockedDecrement16(&session->m_RefCnt) == 0) {
		InterlockedIncrement(&m_PQCSCnt);
		if (::PostQueuedCompletionStatus(m_CoreIOCP, 0, (ULONG_PTR)session, (LPOVERLAPPED)WORKER_JOB::RELEASE) == 0) {
			errCode = GetLastError();
			CCrashDump::Crash();
		}
	}

	return true;
}

void onenewarm::LanServer::Disconnect(LONG64 sessionId)
{
	BOOL cancelIORet;

	LanSession* session = AcquireSession(sessionId);

	if (session == NULL) return;

	_LOG(LOG_LEVEL_WARNING, __FILE__, __LINE__, L"Disconnected LanClient.\n");

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

void onenewarm::LanServer::SendDisconnect(LONG64 sessionId)
{
	LanSession* session = AcquireSession(sessionId);

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

	CPacket* job = CPacket::Alloc();
	*job << TIMER_JOB::REGISTER_ONCE << 1000 << 0 << (__int64)session << (__int64)WORKER_JOB::SEND_DISCONNECT;
	if (m_JobTimerQ->Enqueue(job) == -1) {
		CCrashDump::Crash();
	}

	/*
	EnterCriticalSection(&g_ConsoleLock);
	::printf("End Disconnect\n");
	LeaveCriticalSection(&g_ConsoleLock);
	*/
}

bool onenewarm::LanServer::ReleaseSession(LanSession* session)
{
	if (InterlockedCompareExchange16(&session->m_RefCnt, RELEASE_FLAG, 0) == 0) {
		OnSessionRelease(session->m_Id);

		WORD idx = (WORD)session->m_Id;
		session->Clear();
		LONG enQRet = m_SessionIdxes->Enqueue(idx);
		if (enQRet == -1) {
			CCrashDump::Crash();
		}

		InterlockedDecrement16((SHORT*)&m_CurSessionCnt);
		_LOG(LOG_LEVEL_WARNING, __FILE__, __LINE__, L"ReleaseSession LanClient.\n");

		return true;
	}

	return false;
}


bool onenewarm::LanServer::IsInvalidPacket(const char* pkt)
{
	if (*pkt != m_PacketCode) return false;

	const LanHeader* header = (LanHeader*)pkt;

	if (header->len > CPacket::BUFFER_DEFAULT) return false;

	else return true;
}


void onenewarm::LanServer::HandleRecvCompletion(LanSession* session, DWORD bytesTransferred)
{
	session->m_LastRecvedTime = ::GetTickCount64();

	CPacket* recvBuffer = session->m_RecvBuffer;

	recvBuffer->MoveWritePos(bytesTransferred); // TCP�� ���ؼ� ���ۿ� ���� ������ ��ŭ �������� �̵�

	int recvPktCnt = 0;

	while (recvBuffer->GetDataSize() >= sizeof(LanHeader)) {
		// ����� Peek�ؼ� ����� len�� ���� Ȯ���Ͽ� ����ó��
		LanHeader* header = (LanHeader*)recvBuffer->GetReadPtr();
		if (header->len > (recvBuffer->GetBufferSize() - sizeof(LanHeader))) {
			Disconnect(session->m_Id);
			return;
		}

		if (recvBuffer->GetDataSize() < sizeof(LanHeader) + header->len) break;

		if (IsInvalidPacket(recvBuffer->GetReadPtr()) == false) {
			Disconnect(session->m_Id);
			return;
		}

		recvBuffer->MoveReadPos(sizeof(LanHeader)); // Network Lib�� �����ŭ�� �������� �̵�
		
		CPacket* pkt = CPacket::Alloc();
		CPacketRef payloadOfMsg(pkt);

		pkt->PutData(recvBuffer->GetReadPtr(), header->len);
		recvBuffer->MoveReadPos(header->len);


		/*

		::printf("Header : ");
		for (int cnt = 0; cnt < sizeof(NetHeader); ++cnt) {
			::printf("%02X ", ((unsigned char*)header)[cnt]);
		}
		printf("\n");
		printf("PayLoad : ");
		for (int cnt = 0; cnt < payloadOfMsg->GetDataSize(); ++cnt) {
			::printf("%02X ", ((unsigned char*)payloadOfMsg->GetReadPtr())[cnt]);
		}
		printf("\n");

		*/

		OnRecv(session->m_Id, payloadOfMsg); // ��������
		recvPktCnt++;
	}
	InterlockedAdd((LONG*)&m_RecvTPS, recvPktCnt);

	int usedRecvBufferSize = recvBuffer->GetDataSize();

	::memcpy(recvBuffer->GetBufferPtr(), recvBuffer->GetReadPtr(), usedRecvBufferSize);
	recvBuffer->Clear();
	recvBuffer->MoveWritePos(usedRecvBufferSize);

	RecvPost(session);
}

void onenewarm::LanServer::HandleSendCompletion(LanSession* session, DWORD bytesTransferred)
{
	InterlockedAdd((LONG*)&m_SendTPS, (LONG)session->m_SendPacketsSize);

	for (SHORT cnt = 0; cnt < session->m_SendPacketsSize; ++cnt) {
		CPacket::Free(session->m_SendPackets[cnt]);
	}

	session->m_SendPacketsSize = 0;

	InterlockedExchange8((CHAR*)&session->m_IsSent, 0);
}


void onenewarm::LanServer::LanSession::Init(const DWORD idx, const SOCKET& sock, const SOCKADDR_IN& addr)
{
	m_Id = (InterlockedIncrement64(&s_SessionIDCounter) << 16) + idx;
	m_Sock = sock;
	m_Addr = addr;
	m_LastRecvedTime = GetTickCount64();
}


// ����ȭ ������ �������� ����, Release�� ��Ÿ���� ���� 1�̴�.
LanServer::LanSession* onenewarm::LanServer::CreateSession(const SOCKET sock, const SOCKADDR_IN& addr)
{
	WORD sessionIdx;

	if (!m_SessionIdxes->Dequeue(&sessionIdx)) {
		_LOG(LOG_LEVEL_ERROR, __FILE__, __LINE__, L"Error : already allocated max session");
		CCrashDump::Crash();
	}

	LanSession* session = &m_Sessions[sessionIdx];

	session->Init(sessionIdx, sock, addr);


	return session;
}




void onenewarm::LanServer::AcceptProc(const SOCKET sock, SOCKADDR_IN& addr)
{
	InterlockedIncrement16((SHORT*)&m_CurSessionCnt);

	//Session�� ����� �ϴ� ����
	LanSession* session = CreateSession(sock, addr);

	InterlockedIncrement16(&session->m_RefCnt);

	InterlockedAnd16(&session->m_RefCnt, 0x7fff);

	//IOCP���
	if (CreateIoCompletionPort((HANDLE)sock, m_CoreIOCP, (ULONG_PTR)session, NULL) == NULL) {
		DWORD errCode = GetLastError();

		if (errCode != WSAECONNRESET) {// ��������� RST�� �� ��Ȳ�� ���� ��Ȳ�� �ƴ�	
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





unsigned __stdcall onenewarm::LanServer::Accept(void* argv)
{
	int stackSegment = 0;
	::srand((unsigned int)((LONG64)&stackSegment));

	SOCKET clientSocket;
	SOCKADDR_IN clientAddr;
	int clientAddrLen = sizeof(clientAddr);

	LanServer* server = (LanServer*)argv;

	while (1)
	{
		clientSocket = ::accept(server->m_ListenSocket, (SOCKADDR*)&clientAddr, &clientAddrLen);

		if (clientSocket == INVALID_SOCKET) {
			int errCode = WSAGetLastError();

			if (errCode == WSAEINTR) break;

			_LOG(LOG_LEVEL_ERROR, __FILE__, __LINE__, L"Failed accept ( err : %d )", errCode);
			CCrashDump::Crash();
		}

		server->AcceptProc(clientSocket, clientAddr);
	}

	::printf("Accept Thread Returned ( threadId : %d )\n", GetCurrentThreadId());

	return 0;
}


unsigned __stdcall onenewarm::LanServer::GetCompletion(void* argv)
{
	int stackSegment = 0;
	::srand((unsigned int)((LONG64)&stackSegment));

	LanServer* server = (LanServer*)argv;

	DWORD bytesTransferred = -1;
	LONG64 completionKey = -1;
	_OVERLAPPED* overlappedPtr;

	while (true)
	{
		bytesTransferred = -1;
		completionKey = -1;

		GetQueuedCompletionStatus(server->m_CoreIOCP, &bytesTransferred, (PULONG_PTR)&completionKey, (LPOVERLAPPED*)&overlappedPtr, INFINITE);

		if ((__int64)overlappedPtr < WORKER_JOB::SYSTEM) {
			if (overlappedPtr == (_OVERLAPPED*)WORKER_JOB::SHUT_DOWN && completionKey == 0 && bytesTransferred == 0) {
				// ��Ŀ �������� ����
				if (::PostQueuedCompletionStatus(server->m_CoreIOCP, 0, 0, (LPOVERLAPPED)WORKER_JOB::SHUT_DOWN) == 0) CCrashDump::Crash();
				::printf("Worker Thread Returned ( threadId : %d )\n", GetCurrentThreadId());
				break;
			}
			else {
				// ����
				_LOG(LOG_LEVEL_ERROR, __FILE__, __LINE__, L"GQCS Error : %d", GetLastError());
				CCrashDump::Crash();
			}
		}
		else if ((__int64)overlappedPtr < WORKER_JOB::NETWORK) {
			InterlockedDecrement(&server->m_PQCSCnt);
			if (overlappedPtr == (_OVERLAPPED*)WORKER_JOB::SEND_DISCONNECT) {
				WorkerJob* workerJob = (WorkerJob*)completionKey;
				LanSession* session = (LanSession*)workerJob->completionKey;
				server->Disconnect(session->m_Id);

				server->m_WorkerJobPool.Free(workerJob);

				if (InterlockedDecrement16(&session->m_RefCnt) == 0)
					server->ReleaseSession(session);
			}
			else {
				LanSession* session = (LanSession*)completionKey;
				if (overlappedPtr == (_OVERLAPPED*)WORKER_JOB::SEND) {
					server->SendPost(session);
				}
				else if (overlappedPtr == (_OVERLAPPED*)WORKER_JOB::RELEASE) {
					server->ReleaseSession(session);
					continue;
				}
				else {
					// ����
					_LOG(LOG_LEVEL_ERROR, __FILE__, __LINE__, L"GQCS Error : %d", GetLastError());
					CCrashDump::Crash();
				}

				if (InterlockedDecrement16(&session->m_RefCnt) == 0)
					server->ReleaseSession(session);
			}
		}
		else if ((__int64)overlappedPtr == WORKER_JOB::CONTENTS) {
			InterlockedDecrement(&server->m_PQCSCnt);
			WorkerJob* workerJob = (WorkerJob*)completionKey;
			server->OnWorker(workerJob->completionKey);
			if (InterlockedDecrement16((SHORT*)&workerJob->refCnt) == 0) {
				server->OnWorkerExpired(workerJob->completionKey);
				server->m_WorkerJobPool.Free(workerJob);
			}
		}
		else {
			LanSession* session = (LanSession*)completionKey;

			if (overlappedPtr == &session->m_RecvOverlapped) {
				// WSARecv �Ϸ�ó��
				if (bytesTransferred != 0)
					server->HandleRecvCompletion(session, bytesTransferred);
			}
			else if (overlappedPtr == &session->m_SendOverlapped) {
				// WSASend�� �Ϸ�ó��
				server->HandleSendCompletion(session, bytesTransferred);
			}
			else {
				// ����
				_LOG(LOG_LEVEL_ERROR, __FILE__, __LINE__, L"GQCS Error : %d", GetLastError());
				CCrashDump::Crash();
			}

			if (InterlockedDecrement16(&session->m_RefCnt) == 0)
				server->ReleaseSession(session);
		}
	}

	return 0;
}


unsigned __stdcall onenewarm::LanServer::JobTimer(void* argv)
{
	struct Compare {
		bool operator()(const std::pair<DWORD, WorkerJob*>& a, const std::pair<DWORD, WorkerJob*>& b) const {
			return a.first > b.first; // first�� �������� �������� ����
		}
	};

	// priority_queue / ������ PQCS�� �ؾ��ϴ� �ð��� �������� ���ĵǵ��� ��
	// key�� �˸�ȴ�.
	std::priority_queue<pair<DWORD, WorkerJob*>, vector<pair<DWORD, WorkerJob*>>, Compare> pq;
	std::unordered_map<__int64, WorkerJob*> frameJobMap;

	LanServer* server = (LanServer*)argv;

	while (server->m_IsStop == false) {
		::Sleep(1);
		DWORD curTime = timeGetTime();
		LONG jobQSize = server->m_JobTimerQ->GetSize();
		// JobQ�� �Ѿ�� ���� ó���Ѵ�.
		while (jobQSize--) {
			CPacket* msg;
			server->m_JobTimerQ->Dequeue(&msg);

			// Job�� type�� Ȯ���մϴ�.
			int type = -1;
			*msg >> type;
			if (type == -1) {
				CCrashDump::Crash();
			}


			// type�� ó���� �մϴ�.
			switch (type) {

			case TIMER_JOB::REGISTER_ONCE:
			{
				int delayTime = -1; int transferred = -1;
				__int64 completionKey = -1; __int64 overlapped = -1;

				*msg >> delayTime >> transferred >> completionKey >> overlapped;

				if (delayTime == -1 || transferred == -1 || completionKey == -1 || overlapped == -1) {
					CCrashDump::Crash();
				}

				// �ܹ߼� Job�� pq���� �߰��� �մϴ�. id�� 0�� ���� �ܹ߼� Job�� �ǹ��մϴ�.
				WorkerJob* workerJob = server->m_WorkerJobPool.Alloc();
				workerJob->transferred = transferred;
				workerJob->completionKey = completionKey;
				workerJob->overlapped = (LPOVERLAPPED)overlapped;
				workerJob->isRepeat = false;
				workerJob->refCnt = 1;
				pq.push({ curTime + delayTime, workerJob });
				break;
			}

			// 0�� -1�� ������ id�� ��� �� �� �ֽ��ϴ�.
			case TIMER_JOB::REGISTER_FRAME:
			{
				__int64 id = -1;  int fixedTime = -1; int transferred = -1;
				__int64 completionKey = -1; __int64 overlapped = -1;

				*msg >> id >> fixedTime >> transferred >> completionKey >> overlapped;

				if (id == -1 || fixedTime == -1 || transferred == -1 || completionKey == -1 || overlapped == -1) {
					CCrashDump::Crash();
				}

				WorkerJob* workerJob = server->m_WorkerJobPool.Alloc();
				workerJob->transferred = transferred;
				workerJob->completionKey = completionKey;
				workerJob->overlapped = (LPOVERLAPPED)overlapped;
				workerJob->isRepeat = true;
				workerJob->fixedTime = fixedTime;
				workerJob->refCnt = 2;
				pq.push({ curTime + fixedTime, workerJob });

				// Map�� �̿��Ͽ� ���� �ֱ�� �����ϴ� Job�� ��ƵӴϴ�.
				frameJobMap.insert({ id, workerJob });
				break;
			}

			case TIMER_JOB::EXPIRE_FRAME:
			{
				__int64 id = -1;
				*msg >> id;
				if (id == -1) CCrashDump::Crash();

				// frameJobMap���� ����
				if (frameJobMap.find(id) != frameJobMap.end()) {
					frameJobMap.erase(id);
					WorkerJob* workerJob = frameJobMap[id];
					workerJob->isRepeat = false;
					if (InterlockedDecrement16((SHORT*)&workerJob->refCnt) == 0) {
						server->m_WorkerJobPool.Free(workerJob);
					}
				}
				break;
			}
			default:
				break;
			}

			// Job�� TLSObjectPool�� ��ȯ�մϴ�.
			CPacket::Free(msg);
		}


		// pq�� top�� Ȯ���Ͽ� ����ð��� ���մϴ�.
		while (!pq.empty()) {
			pair<DWORD, WorkerJob*> top = pq.top();
			if (top.first > curTime) break;
			pq.pop();

			// ����ð����� �۰ų� ���ٸ� PQCS�� ȣ���Ͽ� ��Ŀ�����尡 Job�� ó���� �ϰ��մϴ�.
			WorkerJob* workerJob = top.second;

			InterlockedIncrement(&server->m_PQCSCnt);
			if ((__int64)workerJob->overlapped == WORKER_JOB::SEND_DISCONNECT) {
				if (::PostQueuedCompletionStatus(server->m_CoreIOCP, 0, (__int64)workerJob, (LPOVERLAPPED)WORKER_JOB::SEND_DISCONNECT) == 0) {
					_LOG(LOG_LEVEL_ERROR, __FILE__, __LINE__, L"Failed PQCS : [errCode : %d]", GetLastError());
					CCrashDump::Crash();
				}
			}
			else {
				if (::PostQueuedCompletionStatus(server->m_CoreIOCP, 0, (__int64)workerJob, (LPOVERLAPPED)WORKER_JOB::CONTENTS) == 0) {
					_LOG(LOG_LEVEL_ERROR, __FILE__, __LINE__, L"Failed PQCS : [errCode : %d]", GetLastError());
					CCrashDump::Crash();
				}
			}

			// id�� �ִٸ� second ���� �����ͼ� pq�� top.first�� ���Ͽ� pq�� �ٽ� �߰��մϴ�.
			if (workerJob->isRepeat == true) {
				InterlockedIncrement16((SHORT*)&workerJob->refCnt);
				pq.push({ top.first + workerJob->fixedTime, workerJob });
			}
		}
	}

	::printf("FrameTimer Thread Returned ( threadId : %d )\n", GetCurrentThreadId());

	return 0;
}

unsigned __stdcall onenewarm::LanServer::Sender(void* argv)
{
	LanServer* server = (LanServer*)argv;

	while (server->m_IsStop == false)
	{
		::Sleep(100);
		for (int cnt = 0; cnt < server->m_MaxSessions; ++cnt) {
			LanSession* session = &(server->m_Sessions[cnt]);
			if ((session->m_RefCnt & RELEASE_FLAG) == RELEASE_FLAG || session->m_IsSent == TRUE) continue;

			session = server->AcquireSession(session->m_Id);

			if (session == NULL) continue;


			if (session->m_IsDisconnected == FALSE && session->m_SendBuffer->GetSize() > 0) {
				//InterlockedIncrement(&server->m_PQCSCnt);
				if (InterlockedExchange8((char*)&session->m_IsSent, 1) == 0) {
					server->SendPost(session);
					/*
					if (::PostQueuedCompletionStatus(server->m_CoreIOCP, 0, (ULONG_PTR)session, (LPOVERLAPPED)WORKER_JOB::SEND) == 0) CCrashDump::Crash();
					continue;
					*/
				}
			}

			if (InterlockedDecrement16(&session->m_RefCnt) == 0)
				server->ReleaseSession(session);
		}
	}

	return 0;
}

void onenewarm::LanServer::WorkerJobRegister(DWORD delayTime, __int64 completionKey, BOOL isRepeat, __int64 jobKey)
{
	CPacket* timerJob = CPacket::Alloc();

	if (isRepeat == TRUE) {
		*timerJob << TIMER_JOB::REGISTER_FRAME << jobKey << (int)delayTime << 0 << (__int64)completionKey << (__int64)WORKER_JOB::CONTENTS;
	}
	else {
		*timerJob << TIMER_JOB::REGISTER_ONCE << (int)delayTime << 0 << (__int64)completionKey << (__int64)WORKER_JOB::CONTENTS;
	}

	if (m_JobTimerQ->Enqueue(timerJob) == -1) CCrashDump::Crash();
}

void onenewarm::LanServer::WorkerJobRemove(__int64 jobKey)
{
	CPacket* timerJob = CPacket::Alloc();

	*timerJob << TIMER_JOB::EXPIRE_FRAME << jobKey;

	if (m_JobTimerQ->Enqueue(timerJob) == -1) CCrashDump::Crash();
}

CPacket* onenewarm::LanServer::AllocWorkerJob()
{
	return CPacket::Alloc();
}