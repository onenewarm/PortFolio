#pragma once
#include <Containers\LockFreeQueue.hpp>
#include <list>
#include <process.h>

namespace onenewarm
{
	constexpr int	MAX_ENTER_Q_SIZE = 5000;

	class MMOServer;

	class Group
	{
		friend class MMOServer;
	public:
		Group();
		virtual ~Group();

	protected:
		virtual	void	OnMessage(CPacketRef pktRef, __int64 sessionId) = 0;
		virtual	void	OnSessionRelease(__int64 sessionId) = 0;
		virtual	void	OnEnter(__int64 sessionId, __int64 enterKey) = 0;
		virtual	void	OnLeave(__int64 sessionId) = 0;
		virtual	void	OnUpdate() = 0;
		virtual void	OnAttach() = 0;
		virtual void	OnDetach() = 0;

	private:
		static unsigned WINAPI  Update(void* argv);

		void ProcEnter();
		void ProcLeave(MMOServer::MMOSession* session);
		void ProcMessage(MMOServer::MMOSession* session);

	protected:
		MMOServer*								m_Server;
		LockFreeQueue<pair<__int64, __int64>>	m_EnterQ; // sessionId, enterKey
		std::list<__int64>						m_Sessions; 
		bool									m_IsAttached; // Group은 뗏다 붙혔다가 가능합니다.

	public:
		size_t					GetPlayerCount() { return m_Sessions.size(); }
		DWORD					m_FPS;
		DWORD					m_FrameTime;
	};

}