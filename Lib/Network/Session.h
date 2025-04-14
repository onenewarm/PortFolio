#pragma once
#pragma comment(lib, "ws2_32.lib")

#include <WinSock2.h>
#include <WS2tcpip.h>
#include <windows.h>
#include <synchapi.h>
#include <Containers\LockFreeQueue.hpp>
#include <Network\CPacket.h>
#include <vector>
#include <list>

namespace onenewarm
{
	constexpr int MAX_RECV_BUF_SIZE = 1400;
	constexpr WORD MAX_SENT_PACKET_CNT = 200;
	constexpr LONG64 INVALID_ID = -1;
	constexpr WORD RELEASE_FLAG = 0x8000;

	//IOCP에서 사용 할 수 있는 Session입니다.
	class Session
	{
		friend class MMOServer;
		friend class LanServer;
		friend class NetServer;
		friend class LanClient;
		friend class NetClient;

	public:
		virtual void OnClear() = 0;
		LONG		 Clear();
	protected :
		static LONG64 s_SessionIDCounter;
		
		virtual ~Session();
		Session();
		Session(Session& rhs) = delete;
		Session(Session&& rhs) = delete;
		Session& operator=(Session& rhs) = delete;
		Session& operator=(Session&& rhs) = delete;

	protected:
		LONG64						m_Id;
		SOCKADDR_IN					m_Addr;
		SOCKET						m_Sock;
		LockFreeQueue<CPacket*>*	m_SendBuffer;
		CPacket*					m_RecvBuffer;
		BOOL						m_IsDisconnected;
		BOOL						m_IsSendDisconnected;
		BOOL						m_IsLogin;

		SHORT						m_RefCnt; // m_RefCnt의 최상위 bit는 ReleaseFlag로 사용한다.
		_OVERLAPPED					m_RecvOverlapped;
		_OVERLAPPED					m_SendOverlapped;
		BOOL						m_IsSent;
		CPacket**					m_SendPackets;
		WORD						m_SendPacketsSize;

		__int64						m_LastRecvedTime;

		CRITICAL_SECTION			m_Lock;
	};
}
