#include "GameServer.h"
#include "AuthGroup.h"
#include "CommonProtocol.h"

constexpr int SESSIONKEY_LEN = 64;

void AuthGroup::OnMessage(CPacketRef pktRef, __int64 sessionId)
{
	GameServer* server = (GameServer*)m_Server;

	server->PacketProc(m_Server, sessionId, pktRef);
}

void AuthGroup::OnSessionRelease(__int64 sessionId)
{
}

void AuthGroup::OnEnter(__int64 sessionId, __int64 enterKey)
{
}

void AuthGroup::OnLeave(__int64 sessionId)
{
}

void AuthGroup::OnUpdate()
{
	// 할 것 없음
}

void AuthGroup::OnAttach()
{

}

void AuthGroup::OnDetach()
{
	// 할 것 없음
}

AuthGroup::AuthGroup()
{
	m_RedisKey.resize(64);
}

AuthGroup::~AuthGroup()
{
}

bool AuthGroup::CheckToken(LONG64 accountNo)
{
	bool isGetSuccess = true;
	m_RedisClient->get(m_RedisKey, [&](cpp_redis::reply& reply) {
		if (!reply.is_null())
		{
			__int64 compVal = stoll(reply.as_string());
			if (compVal != accountNo) {
				isGetSuccess = false;
			}
		}
		});
	m_RedisClient->sync_commit();

	if (isGetSuccess == true) {
		m_RedisClient->del({ m_RedisKey }, [](cpp_redis::reply& reply) {
			if (reply.is_integer()) {
				//::printf("%d\n", reply.as_integer());
			}
			});
		m_RedisClient->sync_commit();
		return true;
	}

	return false;
}