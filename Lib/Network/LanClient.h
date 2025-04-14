#pragma once
#include <Network/Session.h>
#include <Utils/ConfigLoader.hpp>

#pragma comment(lib, "Winmm.lib")

namespace onenewarm
{
	class LanClient
	{
		enum WORKER_JOB
		{
			SHUT_DOWN,
			SYSTEM = 100,
			SEND,
			RELEASE,
			NETWORK = 200,
		};

		class LanSession : public Session
		{
			friend class LanClient;
		public:
			void Init(const DWORD idx);
			void OnClear() { }
		};

		public:
			virtual void OnEnterJoinServer(LONG64 sessionId) = 0;
			virtual void OnSessionRelease(LONG64 sessionId) = 0;
			virtual void OnRecv(LONG64 sessionId, CPacket* pkt) = 0;
			virtual void OnError(LONG64 sessionId, int errCode, const wchar_t* msg) = 0;
			virtual void OnStart(ConfigLoader* confLoader) = 0;
			virtual void OnStop() = 0;

			LanClient();
			~LanClient();

			bool Start(
				const wchar_t* configFileName
			);
			void Stop();

			CPacketRef AllocPacket();

			bool SendPacket(LONG64 sessionId, CPacket* pkt);
			void Disconnect(LONG64 sessionId);
			BOOL Connect();

			__forceinline DWORD GetPacketChunkSize() { return CPacket::GetChunkSize(); }
			__forceinline DWORD GetUsePacketSize() { return CPacket::GetUseSize(); }

		private:
			static			unsigned		__stdcall		GetCompletion(void* argv);

		private:
			BOOL RecvPost(LanSession* session);
			BOOL SendPost(LanSession* session);
			LanSession* AcquireSession(LONG64 sessionId);
			bool ReleaseSession(LanSession* session);
			void HandleRecvCompletion(LanSession* session, DWORD bytesTransferred);
			void HandleSendCompletion(LanSession* session, DWORD bytesTransferred);

			/*--------------------

				Helper Method

			--------------------*/
			bool IsInvalidPacket(const char* pkt);
		protected:
			LanSession* m_Sessions;
			HANDLE m_CoreIOCP;
			WORD m_MaxSessions;
			
			LockFreeQueue<WORD>* m_SessionIdxes;
			bool m_IsStop;
			SOCKADDR_IN m_ServerAddr;
			BYTE m_PacketCode;

		public:
			std::vector<HANDLE> m_hThreads;

			// 모니터링
			alignas(64)WORD m_CurSessionCnt;
			alignas(64)DWORD m_SendTPS; // Send 완료처리 횟수 
			alignas(64)DWORD m_RecvTPS; // Recv 완료처리 횟수
			alignas(64)DWORD m_PQCSCnt;
	};

}

