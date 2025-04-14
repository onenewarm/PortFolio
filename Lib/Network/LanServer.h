#pragma once
#ifndef __LAN_SERVER__
#define __LAN_SERVER__

#include <Utils\ConfigLoader.hpp>
#include <Network\Session.h>
#include <Containers\LockFreeQueue.hpp>
#include <Containers\LockFreeStack.hpp>
#include <Utils\Profiler.h>
#include <unordered_set>
#include <vector>

namespace onenewarm
{
	/*-----------------
	
		LanServer
	
	-----------------*/
	class LanServer
	{
		class LanSession : public Session
		{
			friend LanServer;
		public:
			void Init(const DWORD idx, const SOCKET& sock, const SOCKADDR_IN& addr);
			void OnClear() { }
		};

	public:

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

		/*
		// JobTimer의 Job은 세 종류입니다.
		// 1. 일회성으로 워커스레드에게 할 일을 주는 경우
		//    ex) type, Job, afterTime ( 워커스레드 Job / 그대로 전달할 것이다.)
		// 2. 특정 프레임마다 반복적으로 워커스레드에게 할 일을 주는 경우
		//    ex) type, id, Job, fixedTime
		// 3. 프레임마다 도는 로직을 끝내려는 경우
		//    ex) type, id
		*/
		enum TIMER_JOB
		{
			REGISTER_ONCE,
			REGISTER_FRAME,
			EXPIRE_FRAME
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

		LanServer();
		~LanServer();

		bool Start(
			const wchar_t* configFileName,
			bool SetNoDelay = false
		);
		void Stop();

		CPacketRef AllocPacket();

		bool SendPacket(LONG64 sessionId, CPacket* pkt);
		void Disconnect(LONG64 sessionId);
		void SendDisconnect(LONG64 sessionId);
		
		void WorkerJobRegister(DWORD delayTime, __int64 completionKey, BOOL isRepeat = false, __int64 jobKey = NULL);
		void WorkerJobRemove(__int64 jobKey);
		CPacket* AllocWorkerJob();


		__forceinline DWORD GetPacketChunkSize() { return CPacket::GetChunkSize(); }
		__forceinline DWORD GetUsePacketSize() { return CPacket::GetUseSize(); }
	private:
		static			unsigned		__stdcall		Accept(void* argv);
		static			unsigned		__stdcall		GetCompletion(void* argv);
		static			unsigned		__stdcall		JobTimer(void* argv);
		static			unsigned		__stdcall		Sender(void* argv);

	private:
		void AcceptProc(const SOCKET sock, SOCKADDR_IN& addr);
		BOOL RecvPost(LanSession* session);
		BOOL SendPost(LanSession* session);
		LanSession* CreateSession(const SOCKET sock, const SOCKADDR_IN& addr);
		LanSession* AcquireSession(LONG64 sessionId);
		bool ReleaseSession(LanSession* session);
		void HandleRecvCompletion(LanSession* session, DWORD bytesTransferred);
		void HandleSendCompletion(LanSession* session, DWORD bytesTransferred);

		/*--------------------

			Helper Method

		--------------------*/
		bool IsInvalidPacket(const char* pkt);

	private:
		//읽기전용
		LanSession* m_Sessions;
		SOCKET m_ListenSocket; // listen을 하는 소켓
		HANDLE m_CoreIOCP;
		WORD m_MaxSessions;
		std::vector<HANDLE> m_hThreads;
		LockFreeQueue<WORD>* m_SessionIdxes;
		
		LockFreeQueue<CPacket*>* m_JobTimerQ;
		BYTE	m_PacketCode;
		LockFreeObjectPool<WorkerJob> m_WorkerJobPool;

	protected:
		bool m_IsStop;

	public:
		//모니터링
		alignas(64)ULONGLONG m_AcceptTotal;
		WORD m_CurSessionCnt;

		alignas(64)DWORD m_SendTPS; // Send 완료처리 횟수 
		alignas(64)DWORD m_RecvTPS; // Recv 완료처리 횟수
		alignas(64)DWORD m_PQCSCnt;
	};
}
#endif

