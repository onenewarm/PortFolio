#pragma once
#include <Utils\ConfigLoader.hpp>
#include <Network\Session.h>
#include <Containers\LockFreeQueue.hpp>
#include <Containers\LockFreeStack.hpp>
#include <Utils\Profiler.h>
#include <unordered_set>
#include <vector>

namespace onenewarm
{
	class NetServer
	{
		struct WorkerJob
		{
			int transferred;
			ULONG_PTR completionKey;
			LPOVERLAPPED overlapped;
			bool	isRepeat;
			int		fixedTime;
			WORD	refCnt;
		};

		enum WORKER_JOB
		{
			SHUT_DOWN,
			SYSTEM = 100,
			SEND,
			RELEASE,
			SEND_DISCONNECT,
			NETWORK = 200,
			CONTENTS,
		};

		//////////////////////////////////////////////////////////////////
		// JobTimer의 Job은 세 종류입니다.
		// 1. 일회성으로 워커스레드에게 할 일을 주는 경우
		//    ex) type, Job, afterTime ( 워커스레드 Job / 그대로 전달할 것이다.)
		// 2. 특정 프레임마다 반복적으로 워커스레드에게 할 일을 주는 경우
		//    ex) type, id, Job, fixedTime
		// 3. 프레임마다 도는 로직을 끝내려는 경우
		//    ex) type, id
		/////////////////////////////////////////////////////////////////
		enum TIMER_JOB
		{
			REGISTER_ONCE,
			REGISTER_FRAME,
			EXPIRE_FRAME
		};

		class NetSession : public Session
		{
			friend NetServer;
		public:
			void Init(const WORD idx, const SOCKET sock, const SOCKADDR_IN& addr);
			void OnClear() { }
		};

	public:
		virtual bool		OnConnectionRequest(const char* IP, short port) = 0;
		virtual void		OnClientJoin(__int64 sessionID) = 0;
		virtual void		OnSessionRelease(__int64 sessionID) = 0;
		virtual void		OnRecv(__int64 sessionID, CPacketRef& pkt) = 0;
		virtual void		OnSessionError(__int64 sessionId, int errCode, const wchar_t* errMsg, const char* filePath, unsigned int fileLine) = 0;
		virtual void		OnSystemError(int errCode, const wchar_t* errMsg, const char* filePath, unsigned int fileLine) = 0;
		virtual void		OnStart(ConfigLoader* confLoader) = 0;
		virtual void		OnStop() = 0;
		virtual void		OnWorker(__int64 completionKey) = 0;
		virtual void		OnWorkerExpired(__int64 completionKey) = 0;

		NetServer();
		~NetServer();
	
		bool Start(
			const wchar_t* configFileName,
			bool SetNoDelay = false
		);
		void Stop();
	
		CPacketRef AllocPacket();
	
		bool SendPacket(LONG64 sessionId, CPacket* pkt);
		void Disconnect(LONG64 sessionId);
		void SendDisconnect(LONG64 sessionId);
	
		void SetLogin(LONG64 sessionId);

		__forceinline DWORD GetPacketChunkSize() { return CPacket::GetChunkSize(); }
		__forceinline DWORD GetUsePacketSize() { return CPacket::GetUseSize(); }

		void WorkerJobRegister(DWORD delayTime, __int64 completionKey, BOOL isRepeat = false, __int64 jobKey = NULL);
		void WorkerJobRemove(__int64 jobKey);
		CPacket* AllocWorkerJob();

	protected:
		static			unsigned		__stdcall		Accept(void* argv);
		static			unsigned		__stdcall		GetCompletion(void* argv);
		static			unsigned		__stdcall		CheckTimeOut(void* argv);
		static			unsigned		__stdcall		JobTimer(void* argv);
		static			unsigned		__stdcall		Sender(void* argv);

		void AcceptProc(const SOCKET sock, SOCKADDR_IN& addr);
		BOOL RecvPost(NetSession* session);
		BOOL SendPost(NetSession* session);
		NetSession* CreateSession(const SOCKET sock, const SOCKADDR_IN& addr);
		NetSession* AcquireSession(LONG64 sessionId);
		bool ReleaseSession(NetSession* session);
		void HandleRecvCompletion(NetSession* session, DWORD bytesTransferred);
		void HandleSendCompletion(NetSession* session, DWORD bytesTransferred);

		/*--------------------
			
			Helper Method		
		
		--------------------*/
	
		bool IsInvalidPacket(const char* pkt);

	protected:
		//읽기전용
		NetSession* m_Sessions;
		SOCKET m_ListenSocket; // listen을 하는 소켓
		HANDLE m_CoreIOCP;
		WORD m_MaxSessions;
		LockFreeQueue<WORD>* m_SessionIdxes;
		bool m_IsStop;
		LockFreeQueue<CPacket*>* m_JobTimerQ;
		BYTE	m_PacketCode;
		BYTE	m_PacketKey;
		int		m_TimeOutIntervalLogin;
		int		m_TimeOutIntervalNoLogin;
		LockFreeObjectPool<WorkerJob> m_WorkerJobPool;

	public:
		std::vector<HANDLE> m_hThreads;

		//모니터링
		alignas(64)ULONGLONG m_AcceptTotal;
		WORD m_CurSessionCnt;

		alignas(64)DWORD m_SendTPS; // Send 완료처리 횟수 
		alignas(64)DWORD m_RecvTPS; // Recv 완료처리 횟수
		alignas(64)DWORD m_PQCSCnt;

	};


};