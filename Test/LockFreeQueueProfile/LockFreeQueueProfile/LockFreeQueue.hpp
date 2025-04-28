#pragma once
#ifndef __LOCKFREE_QUEUE__
#define __LOCKFREE_QUEUE__

#include <Windows.h>
#include "ObjectPool.hpp"
//#define _LQ_TEST_

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
			__int64 m_Next;
		};

#ifdef _LQ_TEST_
		struct DEBUG_LOG
		{
			int m_OperateType; // [1 : Alloc] [2 : Free]
			LONG64 m_NewTail;
			LONG64 m_OldTail;
			LONG64 m_NewHead;
			LONG64 m_OldHead;
			LONG64 m_NewNext;
			LONG64 m_OldNext;
			DATA m_OutData;
			DWORD m_ThreadID;
		};

		WORD m_LogIdx;
		DEBUG_LOG m_Logs[65536];
		DWORD m_firstCASCnt;
#endif
	private:
		LONG m_MaxSize;
		TLSObjectPool<Node>* m_ObjectPool;

		alignas(64)__int64 m_Head;
		alignas(64)__int64 m_Tail;
		alignas(64)LONG64 m_UniqueTag;
		alignas(64)LONG m_Size;
	};
}

template<typename DATA>
inline onenewarm::LockFreeQueue<DATA>::LockFreeQueue(LONG maxSize) : m_Size(0), m_UniqueTag(0), m_MaxSize(maxSize), m_ObjectPool(new TLSObjectPool<Node>(1000, false))
{
#ifdef _LQ_TEST_
	m_LogIdx = 0;
	ZeroMemory(&m_Logs, sizeof(m_Logs));
	m_firstCASCnt = 0;
#endif
	m_Head = (__int64)m_ObjectPool->Alloc();
	((Node*)m_Head)->m_Next = NULL;
	m_Tail = m_Head;
}
template<typename DATA>
inline onenewarm::LockFreeQueue<DATA>::~LockFreeQueue()
{
	DATA destTmp;
	while (this->Dequeue(&destTmp) == true);

	delete m_ObjectPool;
}


template<typename DATA>
inline LONG onenewarm::LockFreeQueue<DATA>::Enqueue(DATA data)
{
	LONG64 _newNode = (LONG64)m_ObjectPool->Alloc();
	Node* _newNodeAddr = (Node*)_newNode;
	LONG64 _tag = InterlockedIncrement64(&m_UniqueTag);
	_tag = _tag << 47;
	_newNode |= _tag;

	_newNodeAddr->m_Data = data;
	_newNodeAddr->m_Next = NULL;

#ifdef _LQ_TEST_
	DEBUG_LOG _log;
	ZeroMemory(&_log, sizeof(DEBUG_LOG));
	_log.m_OperateType = 1;
	_log.m_ThreadID = GetCurrentThreadId();
	_log.m_NewTail = _newNode;
#endif

	while (true)
	{
		__int64 _tail = m_Tail;
		__int64 _next = (LONG64)((Node*)(_tail & ADDRESS_MASK))->m_Next;

#ifdef _LQ_TEST_
		_log.m_OldTail = (LONG64)_tail;
		_log.m_OldNext = _next;
		_log.m_NewNext = (LONG64)(_newNodeAddr->m_Next);
		_log.m_NewTail = _newNode;
#endif

		if (_next == NULL)
		{
			if (InterlockedCompareExchange64(&((Node*)((LONG64)_tail & ADDRESS_MASK))->m_Next, _newNode, NULL) == NULL)
			{
				InterlockedCompareExchange64(&m_Tail, _newNode, _tail);
				LONG ret = InterlockedIncrement(&m_Size);
#ifdef _LQ_TEST_
				WORD _logIdx = InterlockedIncrement16((SHORT*)&m_LogIdx);
				m_Logs[_logIdx] = _log;
				InterlockedIncrement(&m_firstCASCnt);
#endif
				return ret;
			}
		}
		else
		{
			InterlockedCompareExchange64(&m_Tail, _next, _tail);
		}
		for (int cnt = 0; cnt < 512; ++cnt)
			YieldProcessor();
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

#ifdef _LQ_TEST_
	InterlockedDecrement(&m_firstCASCnt);
#endif


#ifdef _LQ_TEST_
	DEBUG_LOG _log;
	ZeroMemory(&_log, sizeof(DEBUG_LOG));
	_log.m_OperateType = 2;
	_log.m_ThreadID = GetCurrentThreadId();
#endif

	while (true)
	{
		LONG64 _head = (LONG64)m_Head;
		LONG64 _next = (LONG64)((Node*)(_head & ADDRESS_MASK))->m_Next;

		if (_next == NULL) continue;

#ifdef _LQ_TEST_
		_log.m_NewHead = _next;
		_log.m_OldHead = _head;
		if(_next != NULL)
			_log.m_NewNext = (LONG64)((Node*)(_next & ADDRESS_MASK))->m_Next;
		_log.m_OldNext = (LONG64)((Node*)(_head & ADDRESS_MASK))->m_Next;
		_log.m_OutData = ((Node*)(_next & ADDRESS_MASK))->m_Data;
#endif
		*outData = ((Node*)(_next & ADDRESS_MASK))->m_Data;

		if (InterlockedCompareExchange64((LONG64*)&m_Head, _next, _head) == _head) {
#ifdef _LQ_TEST_
			WORD _logIdx = InterlockedIncrement16((SHORT*)&m_LogIdx);
			m_Logs[_logIdx] = _log;
#endif

			//LONG64 _oldTail = InterlockedCompareExchange64((LONG64*)&m_Tail, _next, _head);
			//m_Logs[_logIdx].m_OldTail = _oldTail;

			LONG64 snapTail = (LONG64)m_Tail;
			LONG64 snapNext = (LONG64)((Node*)(snapTail & ADDRESS_MASK))->m_Next;

			if (snapNext != NULL) {
				InterlockedCompareExchange64((LONG64*)&m_Tail, snapNext, (LONG64)snapTail);
			}

			m_ObjectPool->Free((Node*)((LONG64)_head & ADDRESS_MASK));
			break;
		}
		else {
			LONG64 snapTail = (LONG64)m_Tail;
			LONG64 snapNext = (LONG64)((Node*)(snapTail & ADDRESS_MASK))->m_Next;

			if (snapNext != NULL) {
				InterlockedCompareExchange64((LONG64*)&m_Tail, snapNext, (LONG64)snapTail);
			}
		}
		for (int cnt = 0; cnt < 512; ++cnt)
			YieldProcessor();
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

