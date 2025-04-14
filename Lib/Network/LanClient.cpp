#include <Network\LanClient.h>
#include <process.h>

#pragma warning(push)
#pragma warning(disable: 4101)

using namespace onenewarm;

onenewarm::LanClient::LanClient() : m_Sessions(NULL), m_CoreIOCP(INVALID_HANDLE_VALUE), m_MaxSessions(0), m_SessionIdxes(NULL), m_IsStop(false), m_CurSessionCnt(0), m_SendTPS(0), m_RecvTPS(0), m_PQCSCnt(0), m_PacketCode(0)
{
	ZeroMemory(&m_ServerAddr, sizeof(m_ServerAddr));
}

onenewarm::LanClient::~LanClient()
{
}

bool onenewarm::LanClient::Start(const wchar_t* configFileName)
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

	wstring serverIP;
	confLoader->GetValue(&serverIP, L"CLIENT", L"CONNECT_IP");
	InetPtonRet = InetPtonW(AF_INET, serverIP.c_str(), &m_ServerAddr.sin_addr.s_addr);
	if (InetPtonRet != 1) {
		int errCode = WSAGetLastError();
		_LOG(LOG_LEVEL_ERROR, __FILE__, __LINE__, L"Failed InetPtonA (err : %d)", errCode);
		CCrashDump::Crash();
	}
	m_ServerAddr.sin_family = AF_INET;

	int serverPort;
	confLoader->GetValue(&serverPort, L"CLIENT", L"CONNECT_PORT");
	m_ServerAddr.sin_port = htons(serverPort);

	int NumberOfConcurrentThreads;
	confLoader->GetValue(&NumberOfConcurrentThreads, L"CLIENT", L"IOCP_ACTIVE_THREAD");

	int NumberOfWorkerThreads;
	confLoader->GetValue(&NumberOfWorkerThreads, L"CLIENT", L"IOCP_WORKER_THREAD");

	int NumberOfMaxSessions;
	confLoader->GetValue(&NumberOfMaxSessions, L"CLIENT", L"SESSION_MAX");
	m_MaxSessions = NumberOfMaxSessions;

	m_SessionIdxes = new LockFreeQueue<WORD>(NumberOfMaxSessions);

	int packetCode;
	confLoader->GetValue(&packetCode, L"CLIENT", L"PACKET_CODE");
	m_PacketCode = packetCode;

	wstring logLevel;
	confLoader->GetValue(&logLevel, L"CLIENT", L"LOG_LEVEL");

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

	// Session 초기화
	m_MaxSessions = NumberOfMaxSessions;
	m_Sessions = new LanSession[NumberOfMaxSessions];
	for (WORD idx = 0; idx < NumberOfMaxSessions; ++idx) {
		LONG enQRet = m_SessionIdxes->Enqueue(idx);
		if (enQRet == -1) {
			CCrashDump::Crash();
		}
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

	return true;
}

void onenewarm::LanClient::Stop()
{
	int cleanUpRet, closeListenRet;
	DWORD waitThreadsShutDownRet;


	// 연결 중인 Session 종료
	for (int cnt = 0; cnt < m_MaxSessions; ++cnt) {
		if (m_Sessions[cnt].m_Id != INVALID_ID)
			Disconnect(m_Sessions[cnt].m_Id);
	}

	// Session이 모두 종료 될 때까지 대기
	while (m_CurSessionCnt != 0) {
		Sleep(1);
	}

	// Worker 종료
	if (::PostQueuedCompletionStatus(m_CoreIOCP, 0, 0, (LPOVERLAPPED)WORKER_JOB::SHUT_DOWN) == 0)
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


CPacketRef onenewarm::LanClient::AllocPacket()
{
	CPacket* pkt = CPacket::Alloc();
	pkt->MoveWritePos(sizeof(LanHeader));
	CPacketRef pktRef(pkt);
	return pktRef;
}


bool onenewarm::LanClient::SendPacket(LONG64 sessionId, CPacket* pkt)
{
	DWORD errCode;

	LanSession* session = AcquireSession(sessionId);

	if (session == NULL) return false; // 패킷의 refCnt = 1 이상

	//EnterCriticalSection(&g_ConsoleLock);
	pkt->LanEncode(pkt, m_PacketCode);
	//LeaveCriticalSection(&g_ConsoleLock);

	pkt->AddRef(); // 패킷의 refCnt = 2이상

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

	SendPost(session);

	if (InterlockedDecrement16(&session->m_RefCnt) == 0) {
		InterlockedIncrement(&m_PQCSCnt);
		if (::PostQueuedCompletionStatus(m_CoreIOCP, 0, (ULONG_PTR)session, (LPOVERLAPPED)WORKER_JOB::RELEASE) == 0) {
			errCode = GetLastError();
			CCrashDump::Crash();
		}
	}

	return true;
}

void onenewarm::LanClient::Disconnect(LONG64 sessionId)
{
	BOOL cancelIORet;

	LanSession* session = AcquireSession(sessionId);

	if (session == NULL) return;

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
}

BOOL onenewarm::LanClient::Connect()
{
	int connectRet;

	InterlockedIncrement16((SHORT*)&m_CurSessionCnt);

	//Session을 등록을 하는 절차	
	WORD sessionIdx;
	if (!m_SessionIdxes->Dequeue(&sessionIdx)) {
		_LOG(LOG_LEVEL_ERROR, __FILE__, __LINE__, L"Error : already allocated max session");
		CCrashDump::Crash();
	}

	LanSession* session = &m_Sessions[sessionIdx];
	session->Clear();

	session->m_Sock = ::socket(AF_INET, SOCK_STREAM, 0);
	if (session->m_Sock == INVALID_SOCKET) {
		DWORD errCode = WSAGetLastError();
		_LOG(LOG_LEVEL_ERROR, __FILE__, __LINE__, L"Failed socket : %d", errCode);
		CCrashDump::Crash();
	}

	do {
		connectRet = ::connect(session->m_Sock, (SOCKADDR*)&m_ServerAddr, sizeof(m_ServerAddr));

		session->Init(sessionIdx);

		if (connectRet != 0) {
			DWORD errCode = WSAGetLastError();
			
			if (errCode == WSAECONNREFUSED) continue;

			_LOG(LOG_LEVEL_ERROR, __FILE__, __LINE__, L"Failed connect : %d", errCode);
			CCrashDump::Crash();
		}
		::Sleep(1);
		break;
	} while (true);

	InterlockedIncrement16(&session->m_RefCnt);
	InterlockedAnd16(&session->m_RefCnt, 0x7fff);

	//IOCP등록
	if (CreateIoCompletionPort((HANDLE)session->m_Sock, m_CoreIOCP, (ULONG_PTR)session, NULL) == NULL) {
		DWORD errCode = GetLastError();

		if (errCode != WSAECONNRESET) {// 상대측에서 RST를 쏜 상황은 에러 상황이 아님	
			_LOG(LOG_LEVEL_ERROR, __FILE__, __LINE__, L"Session associated IOCP Error : %d", errCode);
			CCrashDump::Crash();
		}
	}
	else {
		if (RecvPost(session) == TRUE) {
			OnEnterJoinServer(session->m_Id);
		}
	}

	if (InterlockedDecrement16(&session->m_RefCnt) == 0)
		ReleaseSession(session);

	return TRUE;
}

unsigned __stdcall onenewarm::LanClient::GetCompletion(void* argv)
{
	int stackSegment = 0;
	::srand((unsigned int)((LONG64)&stackSegment));

	LanClient* client = (LanClient*)argv;

	DWORD bytesTransferred = -1;
	LONG64 completionKey = -1;
	_OVERLAPPED* overlappedPtr;

	while (true)
	{
		bytesTransferred = -1;
		completionKey = -1;

		GetQueuedCompletionStatus(client->m_CoreIOCP, &bytesTransferred, (PULONG_PTR)&completionKey, (LPOVERLAPPED*)&overlappedPtr, INFINITE);

		if ((__int64)overlappedPtr < WORKER_JOB::SYSTEM) {
			if (overlappedPtr == (_OVERLAPPED*)WORKER_JOB::SHUT_DOWN && completionKey == 0 && bytesTransferred == 0) {
				// 워커 스레드의 종료
				if (::PostQueuedCompletionStatus(client->m_CoreIOCP, 0, 0, (LPOVERLAPPED)WORKER_JOB::SHUT_DOWN) == 0) CCrashDump::Crash();
				::printf("Worker Thread Returned ( threadId : %d )\n", GetCurrentThreadId());
				break;
			}
			else {
				// 에러
				_LOG(LOG_LEVEL_ERROR, __FILE__, __LINE__, L"GQCS Error : %d", GetLastError());
				CCrashDump::Crash();
			}
		}
		else if ((__int64)overlappedPtr < WORKER_JOB::NETWORK) {
			InterlockedDecrement(&client->m_PQCSCnt);
			LanSession* session = (LanSession*)completionKey;
			if (overlappedPtr == (_OVERLAPPED*)WORKER_JOB::SEND) {
				client->SendPost(session);
			}
			else if (overlappedPtr == (_OVERLAPPED*)WORKER_JOB::RELEASE) {
				client->ReleaseSession(session);
				continue;
			}
			else {
				// 에러
				_LOG(LOG_LEVEL_ERROR, __FILE__, __LINE__, L"GQCS Error : %d", GetLastError());
				CCrashDump::Crash();
			}

			if (InterlockedDecrement16(&session->m_RefCnt) == 0)
				client->ReleaseSession(session);
		}
		else {
			LanSession* session = (LanSession*)completionKey;

			if (overlappedPtr == &session->m_RecvOverlapped) {
				// WSARecv 완료처리
				if (bytesTransferred != 0)
					client->HandleRecvCompletion(session, bytesTransferred);
			}
			else if (overlappedPtr == &session->m_SendOverlapped) {
				// WSASend의 완료처리
				client->HandleSendCompletion(session, bytesTransferred);
			}
			else {
				// 에러
				_LOG(LOG_LEVEL_ERROR, __FILE__, __LINE__, L"GQCS Error : %d", GetLastError());
				CCrashDump::Crash();
			}

			if (InterlockedDecrement16(&session->m_RefCnt) == 0)
				client->ReleaseSession(session);
		}
	}

	return 0;
}

BOOL onenewarm::LanClient::RecvPost(LanSession* session)
{
	if (session->m_IsDisconnected == TRUE) return FALSE;

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

BOOL onenewarm::LanClient::SendPost(LanSession* session)
{
	if (session->m_IsDisconnected == TRUE) return FALSE;

	while (InterlockedExchange8((char*)&session->m_IsSent, 1) == 0)
	{
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

		if (deqCnt == 0)
		{
			session->m_IsSent = false;

			if (session->m_SendBuffer->GetSize() > 0) continue;

			if (InterlockedDecrement16(&session->m_RefCnt) == 0)
			{
				ReleaseSession(session);
			}
			return FALSE;
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
	}

	return true;
}

LanClient::LanSession * onenewarm::LanClient::AcquireSession(LONG64 sessionId)
{
	LanSession* session = &m_Sessions[sessionId & 0xFFFF];

	WORD refCnt = InterlockedIncrement16(&session->m_RefCnt);

	if ((refCnt & RELEASE_FLAG) == RELEASE_FLAG || session->m_Id != sessionId) {
		if (InterlockedDecrement16(&session->m_RefCnt) == 0)
			ReleaseSession(session);
		return NULL;
	}

	return session;
}

bool onenewarm::LanClient::ReleaseSession(LanSession* session)
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

		return true;
	}

	return false;
}

void onenewarm::LanClient::HandleRecvCompletion(LanSession* session, DWORD bytesTransferred)
{
	session->m_LastRecvedTime = ::GetTickCount64();

	CPacket* recvBuffer = session->m_RecvBuffer;

	recvBuffer->MoveWritePos(bytesTransferred); // TCP에 의해서 버퍼에 들어온 사이즈 만큼 포지션을 이동

	int recvPktCnt = 0;

	while (recvBuffer->GetDataSize() >= sizeof(NetHeader)) {
		// 헤더를 Peek해서 헤더의 len을 먼저 확인하여 예외처리
		NetHeader* header = (NetHeader*)recvBuffer->GetReadPtr();
		if (header->len > (recvBuffer->GetBufferSize() - sizeof(NetHeader))) {
			Disconnect(session->m_Id);
			return;
		}

		if (recvBuffer->GetDataSize() < sizeof(NetHeader) + header->len) break;

		if (IsInvalidPacket(recvBuffer->GetReadPtr()) == false) {
			Disconnect(session->m_Id);
			return;
		}

		recvBuffer->MoveReadPos(sizeof(NetHeader)); // Network Lib의 헤더만큼의 포지션을 이동

		CPacket* payloadOfMsg = CPacket::Alloc();

		payloadOfMsg->PutData(recvBuffer->GetReadPtr(), header->len);
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


		OnRecv(session->m_Id, payloadOfMsg); // 콘텐츠단
		CPacket::Free(payloadOfMsg);
		recvPktCnt++;
	}
	InterlockedAdd((LONG*) & m_RecvTPS, recvPktCnt);

	int usedRecvBufferSize = recvBuffer->GetDataSize();

	::memcpy(recvBuffer->GetBufferPtr(), recvBuffer->GetReadPtr(), usedRecvBufferSize);
	recvBuffer->Clear();
	recvBuffer->MoveWritePos(usedRecvBufferSize);

	RecvPost(session);
}

void onenewarm::LanClient::HandleSendCompletion(LanSession* session, DWORD bytesTransferred)
{
	InterlockedAdd((LONG*)&m_SendTPS, (LONG)session->m_SendPacketsSize);

	for (SHORT cnt = 0; cnt < session->m_SendPacketsSize; ++cnt) {
		CPacket::Free(session->m_SendPackets[cnt]);
	}

	session->m_SendPacketsSize = 0;

	InterlockedExchange8((CHAR*)&session->m_IsSent, 0);

	if (session->m_SendBuffer->GetSize() > 0) {
		SendPost(session);
	}
}

bool onenewarm::LanClient::IsInvalidPacket(const char* pkt)
{
	if (*pkt != m_PacketCode) return false;

	const LanHeader* header = (LanHeader*)pkt;

	if (header->len > CPacket::BUFFER_DEFAULT) return false;

	else return true;
}

void onenewarm::LanClient::LanSession::Init(const DWORD idx)
{
	m_Id = (InterlockedIncrement64(&s_SessionIDCounter) << 16) + idx;
	m_LastRecvedTime = GetTickCount64();
}
