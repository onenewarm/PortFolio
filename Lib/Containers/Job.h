#pragma once
#include <Network/CPacket.h>
#include <Containers/LockFreeQueue.hpp>


namespace onenewarm
{
	class Job
	{
	public:
		static Job* Alloc();
		static void Free(Job* job);

		Job();

	public:
		int m_Type;
		CPacket m_Buffer;
	};
}

