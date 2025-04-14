#pragma once
#include <Network\Group.h>
#include <unordered_map>
#include <cpp_redis/cpp_redis>

#pragma comment(lib, "cpp_redis\\cpp_redis.lib")
#pragma comment(lib, "tacopie\\tacopie.lib")

using namespace onenewarm;

class AuthGroup : public Group
{
public:
	virtual	void	OnMessage(CPacketRef pktRef, __int64 sessionId);
	virtual	void	OnSessionRelease(__int64 sessionId);
	virtual	void	OnEnter(__int64 sessionId, __int64 enterKey);
	virtual	void	OnLeave(__int64 sessionId);
	virtual	void	OnUpdate();
	virtual void	OnAttach();
	virtual void	OnDetach();

	AuthGroup();
	~AuthGroup();

public:
	bool CheckToken(LONG64 accountNo);

public:
	cpp_redis::client* m_RedisClient;
	string m_RedisKey;
};

