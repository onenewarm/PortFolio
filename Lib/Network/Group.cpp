#include <Network\MMOServer.h>
#include <Network\Group.h>

unsigned WINAPI onenewarm::Group::Update(void* argv)
{
	Group* group = (Group*)argv;
	DWORD frameTime = group->m_FrameTime;

	DWORD nextFrameTime = timeGetTime() + frameTime;

	while (group->m_Server->m_IsStop == false)
	{
		DWORD curTime = timeGetTime();
		if (curTime < nextFrameTime) {
			::Sleep(1);
			continue;
		}
		nextFrameTime += frameTime;
		if (group->m_IsAttached == false) {
			::printf("Detached Group Update Thread\n");
			return 0;
		}
		InterlockedIncrement(&group->m_FPS);

		// EnterQ에 있는 Job을 처리
		group->ProcEnter();

		auto sessionsIter = group->m_Sessions.begin();
		auto sessionEndIter = group->m_Sessions.end();
		for (; sessionsIter != sessionEndIter; ) {
			MMOServer::MMOSession* session = group->m_Server->AcquireSession(*sessionsIter);
			if (session == NULL) {
				group->m_Server->OnSessionRelease(*sessionsIter);
				sessionsIter = group->m_Sessions.erase(sessionsIter);
				continue;
			}

			// Group 내 세션들에 대하여 Message처리
			group->ProcMessage(session);
			// Group 내 세션들에 대하여 Message처리
			group->ProcLeave(session);

			if (InterlockedDecrement16(&session->m_RefCnt) == 0)
				group->m_Server->ReleaseSession(session);
		}
		
		// OnUpdate
		group->OnUpdate();
	}

	::printf("Shut down Group Update Thread\n");
	return 0;
}

void onenewarm::Group::ProcEnter()
{
	DWORD enterSize = m_EnterQ.GetSize();
	while (enterSize--) {
		pair<__int64, __int64> enterJob;
		m_EnterQ.Dequeue(&enterJob);
		MMOServer::MMOSession* session = m_Server->AcquireSession(enterJob.first);
		if (session == NULL) {
			OnEnter(enterJob.first, enterJob.second);
			OnSessionRelease(enterJob.first);
			return;
		}
		m_Sessions.push_back(enterJob.first);
		session->m_IsLeft = false;
		OnEnter(enterJob.first, session->m_EnterKey);
		if (InterlockedDecrement16(&session->m_RefCnt) == 0) {
			m_Server->ReleaseSession(session);
		}
	}
}

void onenewarm::Group::ProcLeave(MMOServer::MMOSession* session)
{
	if (session->m_IsLeft == true) {
		OnLeave(session->m_Id);
		session->m_Group->m_EnterQ.Enqueue({ session->m_Id, session->m_EnterKey }); // 새롭게 이동하는 Group
	}
}

void onenewarm::Group::ProcMessage(MMOServer::MMOSession* session)
{
	if (session->m_MessageQ->GetUseSize() == 0) return;
	CPacket* pkt;
	while (session->m_MessageQ->Dequeue((char*)&pkt, sizeof(CPacket*))) {
		OnMessage(CPacketRef(pkt), session->m_Id);
		CPacket::Free(pkt);
	}
}

onenewarm::Group::Group() : m_Server(NULL), m_EnterQ(LockFreeQueue<pair<__int64, __int64>>(MAX_ENTER_Q_SIZE)), m_IsAttached(false), m_FPS(0)
{
}

onenewarm::Group::~Group()
{
}
