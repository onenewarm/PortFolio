#pragma once
#include <Containers\ObjectPool.hpp>

namespace onenewarm
{
	template<typename DATA>
	class LockFreeStack
	{
		struct Node
		{
			LONG64 m_Next;
			DATA m_Data;
		};

	public:
		LockFreeStack();
		LockFreeStack(const LockFreeStack& rhs) = delete;
		LockFreeStack(LockFreeStack&& rhs) = delete;
		LockFreeStack& operator=(const LockFreeStack& rhs) = delete;
		LockFreeStack& operator=(LockFreeStack&& rhs) = delete;
		~LockFreeStack();

		void Push(const DATA& inData);
		bool Pop(DATA* outData);

	private:
		LONG m_Size;
		LONG64 m_Top;
		LONG m_UniqueTag;

		LockFreeObjectPool<Node> m_NodePool;
	};
	template<typename DATA>
	inline LockFreeStack<DATA>::LockFreeStack() : m_Size(0), m_Top(NULL), m_UniqueTag(0)
	{

	}
	template<typename DATA>
	inline LockFreeStack<DATA>::~LockFreeStack()
	{
		while (m_Top != NULL)
		{
			m_NodePool.Free((Node*)m_Top);
			m_Top = ((Node*)m_Top)->m_Next;
		}
	}
	template<typename DATA>
	inline void LockFreeStack<DATA>::Push(const DATA& inData)
	{
		Node* newNodeAddr = m_NodePool.Alloc();
		LONG64 newNode = (LONG64)newNodeAddr;
		newNodeAddr->m_Data = inData;

		LONG64 newTag = (LONG64)InterlockedIncrement(&m_UniqueTag);
		newTag = newTag << 47;
		newNode |= newTag;

		while (1)
		{
			LONG64 snapTop = m_Top;
			newNodeAddr->m_Next = snapTop;
			if (InterlockedCompareExchange64((LONG64*)&m_Top, newNode, snapTop) == snapTop) break;
		}

		InterlockedIncrement(&m_Size);
	}
	template<typename DATA>
	inline bool LockFreeStack<DATA>::Pop(DATA* outData)
	{
		//Size값의 느슨한 동기화
		long snapSize = InterlockedDecrement(&m_Size);
		if (snapSize < 0) {
			InterlockedIncrement(&m_Size);
			return false;
		}

		LONG64 snapTop;

		//LOCKFREE
		while (1)
		{
			snapTop = m_Top;
			LONG64 snapNext = ((Node*)(snapTop & ADDRESS_MASK))->m_Next;
			if (InterlockedCompareExchange64(&m_Top, snapNext, snapTop) == snapTop) break;
		}

		//LOCKFREE 통과 시 Data 저장 및 노드 반환
		*outData = ((Node*)(snapTop & ADDRESS_MASK))->m_Data;
		m_NodePool.Free((Node*)(snapTop & ADDRESS_MASK));

		return true;
	}
}
