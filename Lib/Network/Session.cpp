#include <Network\Session.h>


constexpr LONG MAX_SENDBUF_SIZE = 20000;

LONG64 onenewarm::Session::s_SessionIDCounter = 0;




onenewarm::Session::Session() : m_Id(INVALID_ID), m_Sock(INVALID_SOCKET), m_SendBuffer(new LockFreeQueue<CPacket*>(MAX_SENDBUF_SIZE)), m_RecvBuffer(new CPacket(MAX_RECV_BUF_SIZE)), m_IsLogin(false), m_IsDisconnected(false), m_IsSendDisconnected(false), m_RefCnt(RELEASE_FLAG), m_IsSent(false), m_LastRecvedTime(0), m_SendPacketsSize(0)
{
	ZeroMemory(&m_Addr, sizeof(m_Addr));
	ZeroMemory(&m_SendOverlapped, sizeof(m_SendOverlapped));
	ZeroMemory(&m_RecvOverlapped, sizeof(m_RecvOverlapped));

	m_SendPackets = (CPacket**)malloc(sizeof(CPacket*) * MAX_SENT_PACKET_CNT);

	InitializeCriticalSection(&m_Lock);
}

onenewarm::Session::~Session()
{
	DeleteCriticalSection(&m_Lock);

	free(m_SendPackets);
	delete m_SendBuffer;
	delete m_RecvBuffer;
}


LONG onenewarm::Session::Clear()
{
	::closesocket(m_Sock);

	m_Sock = INVALID_SOCKET;
	ZeroMemory(&m_SendOverlapped, sizeof(m_SendOverlapped));
	ZeroMemory(&m_RecvOverlapped, sizeof(m_RecvOverlapped));

	LONG ret = 0;

	while (m_SendBuffer->GetSize() > 0)
	{
		CPacket* pkt;
		m_SendBuffer->Dequeue(&pkt);

		if (CPacket::Free(pkt) == true) ++ret;
	}

	m_RecvBuffer->Clear();

	m_IsLogin = false;
	m_IsDisconnected = false;
	m_IsSendDisconnected = false;
	m_IsSent = false;
	m_LastRecvedTime = 0;

	OnClear();

	return ret;
}