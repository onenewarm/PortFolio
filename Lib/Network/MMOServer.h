#pragma once

#include <Network\Session.h>
#include <Containers\LockFreeQueue.hpp>
#include <vector>
#include <Containers\LockFreeStack.hpp>
#include <Utils\Profiler.h>
#include <unordered_set>
#include <Containers\RingBuffer.h>
#include <synchapi.h>
#include <Containers/Job.h>
#include <DB\DBConnection.h>
#include <Utils\ConfigLoader.hpp>

namespace onenewarm
{
	class Group;

	class MMOServer
	{
		friend class Group;

		class MMOSession : public Session
		{
			friend			MMOServer;
			friend			Group;
		public:
			virtual void	OnClear();
			void			Init(const WORD idx, const SOCKET sock, const SOCKADDR_IN& addr);

			MMOSession();
			~MMOSession();
		private:
			RingBuffer*		m_MessageQ;
			bool			m_IsLeft;
			Group*			m_Group;
			__int64			m_EnterKey;
		};

		struct JobTimerItem
		{
			int transferred;
			ULONG_PTR completionKey;
			LPOVERLAPPED overlapped;
			bool	isRepeat;
			int     repeatTime;
			WORD	refCnt;
		};

		enum ENUM_WORKER
		{
			SHUT_DOWN,
			SYSTEM = 100,
			SEND,
			RELEASE,
			SEND_DISCONNECT,
			NETWORK = 200,
			GROUP_UPDATE,
			GROUP = 300,
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
		enum ENUM_JOBTIMER
		{
			REGISTER_ONCE,
			REGISTER_FRAME,
			EXPIRE_FRAME
		};


	public:
		virtual			bool							OnConnectionRequest(const char* IP, short port) = 0;
		virtual			void							OnClientJoin(__int64 sessionID) = 0;
		virtual			void 							OnSessionRelease(__int64 sessionID) = 0;
		virtual			void 							OnRecv(__int64 sessionID, CPacketRef& pkt) = 0;
		virtual			void							OnSessionError(__int64 sessionId, int errCode, const wchar_t* errMsg, const char* filePath, unsigned int fileLine) = 0;
		virtual			void							OnSystemError(int errCode, const wchar_t* errMsg, const char* filePath, unsigned int fileLine) = 0;
		virtual			void							OnStart(ConfigLoader* confLoader) = 0;
		virtual			void							OnStop() = 0;
		virtual			void							OnWorker(__int64 completionKey) = 0;
		virtual			void							OnWorkerExpired(__int64 completionKey) = 0;
	public:
						bool							Start(const wchar_t* configFileName,bool SetNoDelay = false);
						void							Stop();
						CPacketRef						AllocPacket();
						bool							FreePacket(CPacket* pkt);
						bool							SendPacket(LONG64 sessionId, CPacket* pkt);
						void							Disconnect(LONG64 sessionId);
						void							SendDisconnect(LONG64 sessionId);
						void							SetLogin(LONG64 sessionId);
		__forceinline	DWORD							GetPacketChunkSize() { return CPacket::GetChunkSize(); }
		__forceinline	DWORD							GetUsePacketSize() { return CPacket::GetUseSize(); }
		__forceinline	DWORD							GetJobTimerQSize() { return m_JobTimerQ->GetSize(); }
		__forceinline	WORD							GetMaxSessions() { return m_MaxSessions; }

		/*----------------------------------------------------
		// MoveGroup의 enterKey는 IOCP의 completion Key같은 느낌이다.
		// OnEnter에서 이 enterKey값을 받을 수 있다.
		------------------------------------------------------*/
		bool											MoveGroup(Group* group, __int64 sessionId, __int64 enterKey = NULL); 
		void											AttachGroup(Group* group, DWORD fps);
		void											DetachGroup(Group* group); // 서버의 관리 대상이냐? 아니냐?

		void											JobTimerItemRegister(DWORD delayTime, __int64 completionKey, BOOL isRepeat = false, DWORD repeatTime = 0, __int64 jobKey = NULL);
		void											JobTimerItemRemove(__int64 jobKey);
		Job*											AllocJob();
		void											SendStoreQuery(const char* queryForm, ...);

		MMOServer();
		~MMOServer();
	private:
		static			unsigned		__stdcall		Accept(void* argv);
		static			unsigned		__stdcall		GetCompletion(void* argv);
		static			unsigned		__stdcall		CheckTimeOut(void* argv);
		static			unsigned		__stdcall		JobTimer(void* argv);
		static			unsigned		__stdcall		Sender(void* argv);
		static			unsigned		__stdcall		StoreDB(void* argv);

		MMOSession*										AcquireSession(LONG64 sessionId);
		void											AcceptProc(const SOCKET sock, SOCKADDR_IN& addr);
		BOOL											RecvPost(MMOSession* session);
		BOOL											SendPost(MMOSession* session);
		bool											ReleaseSession(MMOSession* session);
		MMOSession*										CreateSession(const SOCKET sock, const SOCKADDR_IN& addr);
		void											HandleRecvCompletion(MMOSession* session, DWORD bytesTransferred);
		void											HandleSendCompletion(MMOSession* session, DWORD bytesTransferred);
		bool											IsInvalidPacket(const char* pkt);

	private:
		MMOSession*										m_Sessions;
		SOCKET											m_ListenSocket; // listen을 하는 소켓
		HANDLE											m_CoreIOCP;
		WORD											m_MaxSessions;
		LockFreeQueue<WORD>*							m_SessionIdxes;
		std::vector<HANDLE>								m_hThreads;
		bool											m_IsStop;

		BYTE											m_PacketCode;
		BYTE											m_PacketKey;
		int												m_TimeOutIntervalLogin;
		int												m_TimeOutIntervalNoLogin;

		// DB Store
		DBConnection*									m_StoreDBConnection;
		TLSObjectPool<char[MAX_QUERY_SIZE]>*			m_StoreQueryPool;
		LockFreeQueue<char*>*							m_StoreQueryQ; // 문자열
		bool											m_StoreDBSignal;


		// JobTimer
		LockFreeQueue<Job*>*							m_JobTimerQ;
		LockFreeObjectPool<JobTimerItem>				m_JobTimerItemPool;

	public:												 
		//모니터링										 
		alignas(64)ULONGLONG							m_AcceptTotal;
		alignas(64)WORD									m_CurSessionCnt;
														
		alignas(64)DWORD								m_SendTPS; // Send 완료처리 횟수 
		alignas(64)DWORD								m_RecvTPS; // Recv 완료처리 횟수
		alignas(64)DWORD								m_PQCSCnt;

	};


};