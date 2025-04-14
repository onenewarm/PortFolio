#pragma once
#ifndef __LOCKFREE_QUEUE__
#define __LOCKFREE_QUEUE__

#include <Windows.h>
#include <Containers\ObjectPool.hpp>

using namespace std;

namespace onenewarm
{
	template <typename DATA>
	class LockFreeQueue
	{
	public:
		LockFreeQueue(LONG maxSize);
		~LockFreeQueue();

		LONG Enqueue(DATA data);
		bool Dequeue(DATA* outData);

		LONG GetSize();

		struct Node
		{
			DATA m_Data;
			LONG64 m_Next;
		};

	private:
		LONG64 m_Head;
		LONG64 m_Tail;
		LONG m_UniqueTag;
		LONG m_Size;

		LONG m_MaxSize;
		TLSObjectPool<Node>* m_NodePool;
	};
}

template<typename DATA>
inline onenewarm::LockFreeQueue<DATA>::LockFreeQueue(LONG maxSize) : m_Size(0), m_UniqueTag(0), m_MaxSize(maxSize), m_NodePool(new TLSObjectPool<Node>(1000, false))
{
	m_Head = (LONG64)m_NodePool->Alloc();
	((Node*)m_Head)->m_Next = NULL;
	m_Tail = m_Head;
}

template<typename DATA>
inline onenewarm::LockFreeQueue<DATA>::~LockFreeQueue()
{
	DATA destTmp;
	while (this->Dequeue(&destTmp) == true);

	delete m_NodePool;
}


template<typename DATA>
inline LONG onenewarm::LockFreeQueue<DATA>::Enqueue(DATA data)
{
	if (m_Size == m_MaxSize) return -1;

	LONG64 newNode = (LONG64)m_NodePool->Alloc();
	Node* newNodeAddr = (Node*)newNode;
	
	LONG64 tag = (LONG64)InterlockedIncrement(&m_UniqueTag);
	tag = tag << 47;
	newNode |= tag;

	newNodeAddr->m_Data = data;
	newNodeAddr->m_Next = NULL;

	while (true)
	{
		LONG64 snapTail = m_Tail;
		LONG64 snapNext = ((Node*)(snapTail & ADDRESS_MASK))->m_Next;

		if (snapNext == NULL)
		{
			if (InterlockedCompareExchange64(&((Node*)((LONG64)snapTail & ADDRESS_MASK))->m_Next, newNode, NULL) == NULL)
			{
				InterlockedCompareExchange64(&m_Tail, newNode, snapTail);
				LONG ret = InterlockedIncrement(&m_Size);
				return ret;
			}
		}
		else
		{
			InterlockedCompareExchange64(&m_Tail, snapNext, snapTail);
		}
	}
}


template<typename DATA>
inline bool onenewarm::LockFreeQueue<DATA>::Dequeue(DATA* outData)
{
	long _size = InterlockedDecrement(&m_Size);

	if (_size < 0)
	{
		InterlockedIncrement(&m_Size);
		return false;
	}

	while (true)
	{
		LONG64 snapHead = m_Head;
		LONG64 snapNext = ((Node*)(snapHead & ADDRESS_MASK))->m_Next;

		if (snapNext == NULL) continue;

		*outData = ((Node*)(snapNext & ADDRESS_MASK))->m_Data;

		if (InterlockedCompareExchange64(&m_Head, snapNext, snapHead) == snapHead) {
			LONG64 snapTail = m_Tail;
			LONG64 snapNext = ((Node*)(snapTail & ADDRESS_MASK))->m_Next;

			if (snapNext != NULL) {
				InterlockedCompareExchange64((LONG64*)&m_Tail, snapNext, (LONG64)snapTail);
			}

			m_NodePool->Free((Node*)(snapHead & ADDRESS_MASK));
			break;
		}
		else {
			LONG64 snapTail = m_Tail;
			LONG64 snapNext = ((Node*)(snapTail & ADDRESS_MASK))->m_Next;

			if (snapNext != NULL) {
				InterlockedCompareExchange64(&m_Tail, snapNext, snapTail);
			}
		}
	}

	return true;
}

template<typename DATA>
inline LONG onenewarm::LockFreeQueue<DATA>::GetSize()
{
	int snapSize;
	while (true)
	{
		snapSize = m_Size;
		if (snapSize >= 0) break;
	}
	return snapSize;
}

#endif

