#pragma once
#include <Network/Session.h>
#include <Utils/ConfigLoader.hpp>

#pragma comment(lib, "Winmm.lib")



namespace onenewarm
{
	class NetClient
	{
		enum WORKER_JOB
		{
			SHUT_DOWN,
			SYSTEM = 100,
			SEND,
			RELEASE,
			NETWORK = 200,
		};

		class NetSession : public Session
		{
			friend class NetClient;
		public:
			void Init(const DWORD idx);
			void OnClear() { }
		};

	public:
		virtual void OnEnterJoinServer(LONG64 sessionId) = 0;
		virtual void OnSessionRelease(LONG64 sessionId) = 0;

		virtual void OnRecv(LONG64 sessionId, CPacket* pkt) = 0;
		//virtual void OnSend(LONG64 sessionId) = 0;
		//	virtual void OnWorkerThreadBegin() = 0;
		//	virtual void OnWorkerThreadEnd() = 0;
		virtual void OnError(LONG64 sessionId, int errCode, const wchar_t* msg) = 0;
		virtual void OnStart(ConfigLoader* confLoader) = 0;
		virtual void OnStop() = 0;

		NetClient();
		~NetClient();

		bool Start(
			const wchar_t* configFileName
		);
		void Stop();

		CPacketRef AllocPacket();
		void FreePacket(CPacket* pkt);

		bool SendPacket(LONG64 sessionId, CPacket* pkt);
		void Disconnect(LONG64 sessionId);
		bool Connect(__int64* outSessionId);

		__forceinline DWORD GetPacketChunkSize() { return CPacket::GetChunkSize(); }
		__forceinline DWORD GetUsePacketSize() { return CPacket::GetUseSize(); }
		__forceinline bool IsCalledDisconnect(__int64 sessionId) { return m_Sessions[(WORD)sessionId].m_IsDisconnected; }

	private:
		static unsigned __stdcall GetCompletion(void* argv);

	private:
		BOOL RecvPost(NetSession* session);
		BOOL SendPost(NetSession* session);
		NetSession* AcquireSession(LONG64 sessionId);
		bool ReleaseSession(NetSession* session);
		void HandleRecvCompletion(NetSession* session, DWORD bytesTransferred);
		void HandleSendCompletion(NetSession* session, DWORD bytesTransferred);

		/*--------------------

			Helper Method

		--------------------*/
		bool IsInvalidPacket(const char* pkt);
	protected:
		NetSession* m_Sessions;
		BYTE		m_PacketCode;
		BYTE		m_PacketKey;
		HANDLE m_CoreIOCP;
		WORD m_MaxSessions;
		std::vector<HANDLE> m_hThreads;
		LockFreeQueue<WORD>* m_SessionIdxes;
		bool m_IsStop;
		SOCKADDR_IN m_ServerAddr;


	public:
		// 모니터링
		alignas(64)WORD m_CurSessionCnt;
		alignas(64)DWORD m_SendTPS; // Send 완료처리 횟수 
		alignas(64)DWORD m_RecvTPS; // Recv 완료처리 횟수
		alignas(64)DWORD m_PQCSCnt;
	};

}

