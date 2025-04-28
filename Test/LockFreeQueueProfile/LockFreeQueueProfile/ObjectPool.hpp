#pragma once
#ifndef __OBJECT_POOL__
#define __OBJECT_POOL__
//#define TEST

#include <new.h>
#include <Windows.h>
#include "Logger.h"
#include "CCrashDump.h"
//#define _LPOOL_TEST_

/*

64bit ȯ�濡�� ��밡���� ��������� �޸� ������ 128TB�̴�.(Windows 10 Pro ����)
�̸� bit�� ġȯ�� �ϸ� 47bit�̴�. ��, User mode�� �ּҸ� �ĺ��ϱ� ���ؼ� �� �� 17bit�� �ʿ� ���ٴ� ���� �ȴ�.
�ֳ��ϸ� ������ 0���� ä������ �����̴�.
�׷��� �� ���� 17bit�� �ּҿ� ���õ� �������� ���� �� �� �ִ�.

*/

//#define _LPOOL_TEST_

#define TAG_OFFSET 0x0000800000000000ULL
#define ADDRESS_MASK 0x00007FFFFFFFFFFFULL
#define TAG_MASK 0xFFFF800000000000ULL

using namespace std;

/*

ObjectPool�� TEST�� �ʿ��� �ڵ��Դϴ�.
TEST�� ���õ� �ڷ�� �� 2���� �Դϴ�.
1. STATUS_FREE, STATUS_ALLOC
	���� Free / ���� Alloc �׽�Ʈ

2. DEBUG_LOG
	��Ƽ������ ��Ȳ���� �߻��ϴ� �̼��� ���� ������ ������� �α��Դϴ�.
	g_Log �迭�� �α׸� ����ϴ�.
	�ִ� �α״� 65536�� ���� ���� �� �� �ֽ��ϴ�.

*/

namespace onenewarm
{

	template <class DATA>
	class ObjectPool
	{
	public:
		struct Node
		{
			DATA m_Data;
			Node* m_Next;
		};

		//////////////////////////////////////////////////////////////////////////
		// ������, �ı���.
		//
		// Parameters:	(int) �ʱ� �� ����.
		//				(bool) Alloc �� ������ / Free �� �ı��� ȣ�� ����
		// Return:
		//////////////////////////////////////////////////////////////////////////
		ObjectPool(int initialBlocks = 0, bool PlacementNew = false);
		virtual	~ObjectPool();


		//////////////////////////////////////////////////////////////////////////
		// �� �ϳ��� �Ҵ�޴´�.  
		//
		// Parameters: ����.
		// Return: (DATA *) ����Ÿ �� ������.
		//////////////////////////////////////////////////////////////////////////
		DATA* Alloc(void);

		//////////////////////////////////////////////////////////////////////////
		// ������̴� ���� �����Ѵ�.
		//
		// Parameters: (DATA *) �� ������.
		// Return: (BOOL) TRUE, FALSE.
		//////////////////////////////////////////////////////////////////////////
		bool	Free(DATA* pData);

		//////////////////////////////////////////////////////////////////////////
		// ���� Ȯ�� �� �� ������ ��´�. (�޸�Ǯ ������ ��ü ����)
		//
		// Parameters: ����.
		// Return: (int) �޸� Ǯ ���� ��ü ����
		//////////////////////////////////////////////////////////////////////////
		int		GetCapacityCount(void) { return m_Capacity; }

		//////////////////////////////////////////////////////////////////////////
		// ���� ������� �� ������ ��´�.
		//
		// Parameters: ����.
		// Return: (int) ������� �� ����.
		//////////////////////////////////////////////////////////////////////////
		int		GetUseCount(void) { return m_UseCount; }


		// ���� ������� ��ȯ�� (�̻��) ������Ʈ ���� ����.

		bool m_IsPlacementNew;
		int m_Capacity;
		int m_UseCount;
		Node* m_Top;
	};


	template <class DATA>
	class LockFreeObjectPool
	{
	public:
		struct Node
		{
			DATA m_Data;
			Node* m_Next;
#ifdef _LPOOL_TEST_
			DWORD m_NodeStatus;
#endif
		};

#ifdef _LPOOL_TEST_

#define STATUS_FREE 0xBB
#define STATUS_ALLOC 0xBC

		struct DEBUG_LOG
		{
			/*

			int operateType : Alloc = 1 / Free = 2
			LONG64 oldTop : ���� �� top Node
			LONG64 newTop : ���� �� top Node
			LONG64 oldNext : oldTop�� next
			LONG64 newNext : newTop�� next
			LONG64 tag : ���ο� top�� unique ID
			DWORD threadID : ������ ������ ������ID

			*/

			int operateType;
			LONG64 oldTop;
			LONG64 newTop;
			LONG64 oldNext;
			LONG64 newNext;
			LONG64 tag;
			DWORD threadID;
		};

		DEBUG_LOG m_Log[65536];
		WORD m_LogIdx = -1;

#endif

	public:
		//////////////////////////////////////////////////////////////////////////
		// ������, �ı���.
		//
		// Parameters:	(int) �ʱ� �� ����.
		//				(bool) Alloc �� ������ / Free �� �ı��� ȣ�� ����
		// Return:
		//////////////////////////////////////////////////////////////////////////
		LockFreeObjectPool(int initialBlocks = 0, bool PlacementNew = false);
		virtual	~LockFreeObjectPool();


		//////////////////////////////////////////////////////////////////////////
		// �� �ϳ��� �Ҵ�޴´�.  
		//
		// Parameters: ����.
		// Return: (DATA *) ����Ÿ �� ������.
		//////////////////////////////////////////////////////////////////////////
		DATA* Alloc();

		//////////////////////////////////////////////////////////////////////////
		// ������̴� ���� �����Ѵ�.
		//
		// Parameters: (DATA *) �� ������.
		// Return: (BOOL) TRUE, FALSE.
		//////////////////////////////////////////////////////////////////////////
		bool	Free(DATA* addr);


		//////////////////////////////////////////////////////////////////////////
		// ���� Ȯ�� �� �� ������ ��´�. (�޸�Ǯ ������ ��ü ����)
		//
		// Parameters: ����.
		// Return: (int) �޸� Ǯ ���� ��ü ����
		//////////////////////////////////////////////////////////////////////////
		int		GetCapacityCount(void)
		{
			int snapCapacityCount;
			while (true)
			{
				snapCapacityCount = m_Capacity;
				if (snapCapacityCount >= 0) break;
			}
			return snapCapacityCount;
		}

		//////////////////////////////////////////////////////////////////////////
		// ���� ������� �� ������ ��´�.
		//
		// Parameters: ����.
		// Return: (int) ������� �� ����.
		//////////////////////////////////////////////////////////////////////////
		int		GetUseCount(void)
		{
			int snapUseCount;
			while (true)
			{
				snapUseCount = m_UseCount;
				if (snapUseCount >= 0) break;
			}
			return snapUseCount;
		}


		// ���� ������� ��ȯ�� (�̻��) ������Ʈ ���� ����.
	private:

		/*

		LONG64 m_UniqueTag : ���Ӱ� Pool�� �߰� �� Node�� Unique ID
		Node* m_Top : Pool�� �ֻ��� Node
		int m_Capacity : ���� Pool�� ����
		DWORD m_UseCount : ���� �ܺο��� ��� ���� Pool�� ����
		bool m_PlacementNew : placement new ����

		*/

		LONG64 m_UniqueTag;
		Node* m_Top;
		LONG m_Capacity;
		DWORD m_UseCount;
		bool m_PlacementNew;
	};


	constexpr DWORD MAX_CHUNK_SIZE = 400;

	template<typename DATA>
	class TLSObjectPool
	{
		class Chunk
		{
			friend TLSObjectPool;
			friend LockFreeObjectPool<TLSObjectPool::Chunk>;
			struct Node
			{
				//����� ���
				DATA m_Data;

				//Node�� ���ϴ� Chunk�� Node���� ���� �� �� �ֵ��� �ϴ� ����
				Chunk* m_ChunkPointer;
				
			};

			Chunk() : m_AllocPos(0), m_FreeCnt(0)
			{
				for (int cnt = 0; cnt < MAX_CHUNK_SIZE; ++cnt)
					m_Nodes[cnt].m_ChunkPointer = this;
			}

			~Chunk() { }

			void Clear()
			{
				m_AllocPos = 0;
				m_FreeCnt = 0;
			}

			//���� Chunk�κ��� �Ҵ�� ���� ����� ��
			DWORD m_AllocPos;

			//���� Chunk�� ��ȯ�� ����� ��
			DWORD m_FreeCnt;

			//Chunk�� ���ԵǴ� ����
			Node m_Nodes[MAX_CHUNK_SIZE];
		};

	public:
		TLSObjectPool(DWORD InitChunkCnt, bool PlacementNew);
		~TLSObjectPool();
		DATA* Alloc();
		bool Free(DATA* addr);
		DWORD GetChunkSize()
		{
			return m_ChunkPool.GetUseCount() + m_ChunkPool.GetCapacityCount();
		}

	public:
		DWORD m_UseNodeCount;

	private:

		//TLS Index
		int m_TlsIndex;

		//Chunk Pool
		LockFreeObjectPool<Chunk> m_ChunkPool;
	};

}


template<class DATA>
inline onenewarm::ObjectPool<DATA>::ObjectPool(int initialBlocks, bool PlacementNew) : m_IsPlacementNew(PlacementNew), m_Capacity(initialBlocks), m_UseCount(0), m_Top(NULL)
{
	for (int cnt = 0; cnt < initialBlocks; ++cnt) {
		Node* newNode = (Node*)malloc(sizeof(Node));

		if (newNode != NULL) {
			if (PlacementNew != false) {
				new(newNode) Node;
			}

			newNode->m_Next = m_Top;
			m_Top = newNode;
		}
		else CCrashDump::Crash();
		
	}
}

template<class DATA>
inline onenewarm::ObjectPool<DATA>::~ObjectPool()
{
	for (int cnt = 0; cnt < m_Capacity; ++cnt) {
		Node* copiedTop = m_Top;
		
		if(copiedTop == NULL) continue;

		m_Top = copiedTop->m_Next;

		if (m_IsPlacementNew == false) {
			copiedTop->~Node();
		}

		free(copiedTop);
	}
}

template<class DATA>
inline DATA* onenewarm::ObjectPool<DATA>::Alloc(void)
{
	if (m_Capacity > 0) {
		Node* copiedTop = m_Top;

		if (m_IsPlacementNew == true) {
			new(copiedTop) Node;
		}

		m_Top = m_Top->m_Next;
		m_Capacity--;
		m_UseCount++;

		return (DATA*)copiedTop;
	}
	else {
		Node* ret = (Node*)malloc(sizeof(Node));

		if (ret != NULL) {
			if (m_IsPlacementNew == true) new(ret) Node;
			m_UseCount++;
			return (DATA*)ret;
		}
		else {
			CCrashDump::Crash();
			return nullptr;
		}
	}
}

template<class DATA>
inline bool onenewarm::ObjectPool<DATA>::Free(DATA* pData)
{
	Node* freeNode = (Node*)pData;

	if (m_IsPlacementNew == true) freeNode->~Node();

	freeNode->m_Next = m_Top;
	m_Top = freeNode;
	m_Capacity++;
	m_UseCount--;

	return true;
}



template<class DATA>
inline onenewarm::LockFreeObjectPool<DATA>::LockFreeObjectPool(int initialBlocks, bool PlacementNew) : m_UniqueTag(0), m_Top(new Node()), m_Capacity(initialBlocks), m_UseCount(0), m_PlacementNew(PlacementNew)
{
	if (PlacementNew == false)
	{
		while (initialBlocks--)
		{
			Node* newNode = new Node();
			
			newNode->m_Next = m_Top;
			
			m_Top = (Node*)((LONG64)newNode + m_UniqueTag);
			m_UniqueTag += TAG_OFFSET;

#ifdef _LPOOL_TEST_
			newNode->m_NodeStatus = STATUS_FREE;
#endif
		}
	}
	else
	{
		while (initialBlocks--)
		{
			Node* newNode = (Node*)malloc(sizeof(Node));

			if (newNode != NULL) {
				newNode->m_Next = m_Top;

				m_Top = (Node*)((LONG64)newNode + m_UniqueTag);
				m_UniqueTag += TAG_OFFSET;

#ifdef _LPOOL_TEST_
				newNode->m_NodeStatus = STATUS_FREE;
#endif
			}
			else CCrashDump::Crash();
			
		}
	}
}

/*

ObjectPool �ȿ� �ִ� Pool�� �ְ�, �ܺο��� ��� ���� Pool�� ���� �ٵ�,
���ο� �ִ� Pool�� ���ؼ��� �޸� �Ҵ� ������ �մϴ�.
�ܺο� �ִ� Pool�� �޸� �Ҵ� �����ϰ� �ȴٸ� �ܺο��� �������� �޸𸮰� free���°� �� Access Violation ������ �߻��ϰų�,
���� �ּҰ� ���� �� ���� �� �� ���� ������ �߱� �� �� �ִ�.

�ܺο��� ObjectPool�� ��� �� �� ObjectPool�� Alloc, Free �Լ��� ����� �� ���̴�.
�̸� �ٷ� ����ϰ� �Ǹ� ObjectPool�� �Ҹ� ���� �� ������ �߻� �� ���̴�.
�׷��� ������ ���� ObjectPool�� ������ �����ϴ� bool ������ �ʿ��ϴٰ� �Ǵ��� �ȴ�.
����, ObjectPool�� ��� �� �� Alloc, Free�� �ٷ� ȣ�� �ϴ� ���� �ƴ϶� Alloc, Free�� �����ؾ� �ȴٰ� ������ �ȴ�.

*/

template<class DATA>
inline onenewarm::LockFreeObjectPool<DATA>::~LockFreeObjectPool()
{
	LONG _Capacity = m_Capacity;

	if (m_PlacementNew == false)
	{
		Node* _top = (Node*)((LONG64)m_Top & ADDRESS_MASK);
		Node* _next = _top->m_Next;

		while (_Capacity--)
		{
			delete _top;
			_top = (Node*)((LONG64)_next & ADDRESS_MASK);
			_next = _top->m_Next;
		}
	}
	else
	{
		Node* _top = (Node*)((LONG64)m_Top & ADDRESS_MASK);
		Node* _next = _top->m_Next;

		while (_Capacity--)
		{
			free(_top);
			_top = (Node*)((LONG64)_next & ADDRESS_MASK);
			_next = _top->m_Next;
		}
	}
}

template<class DATA>
inline DATA* onenewarm::LockFreeObjectPool<DATA>::Alloc()
{
	LONG  _capacity = InterlockedDecrement(&m_Capacity);

#ifdef _LPOOL_TEST_
	DEBUG_LOG _log;
	_log.operateType = 1;
	_log.threadID = GetCurrentThreadId();
#endif
	if (_capacity < 0)
	{
		InterlockedIncrement(&m_Capacity);
		InterlockedIncrement(&m_UseCount);
		Node* _ret = new Node();

#ifdef _LPOOL_TEST_
		InterlockedExchange(&_ret->m_NodeStatus, STATUS_ALLOC);
#endif
		return (DATA*)_ret;
	}
	else
	{
		while (1)
		{
			Node* _top = m_Top;
			Node* _topAddress = (Node*)((LONG64)_top & ADDRESS_MASK);
			Node* _topNext = _topAddress->m_Next;


			DATA* _ret = (DATA*)_topAddress;

#ifdef _LPOOL_TEST_
			_log.oldTop = (LONG64)_topAddress;
			_log.oldNext = (LONG64)_topNext;
			_log.newTop = (LONG64)_topNext;
			_log.newNext = (LONG64)(((Node*)((LONG64)_topNext & ADDRESS_MASK))->m_Next);
			_log.tag = ((LONG64)_topAddress & TAG_MASK);
#endif

			
			if (InterlockedCompareExchange64((LONG64*)&m_Top, (LONG64)_topNext, (LONG64)_top) == (LONG64)_top)
			{
#ifdef _LPOOL_TEST_
				if (_topAddress->m_NodeStatus != STATUS_FREE) DebugBreak();

				InterlockedExchange(&_topAddress->m_NodeStatus, STATUS_ALLOC);
				unsigned short _logIdx = (unsigned short)InterlockedIncrement16((SHORT*)&m_LogIdx);
				m_Log[_logIdx] = _log;

				
#endif

				if (m_PlacementNew)
					new (_ret) DATA();

				InterlockedIncrement(&m_UseCount);

				return _ret;
			}
		}
	}

	
}

template<class DATA>
inline bool onenewarm::LockFreeObjectPool<DATA>::Free(DATA* addr)
{
	if (m_PlacementNew)
		addr->~DATA();

	LONG64 _tag = InterlockedAdd64(&m_UniqueTag, TAG_OFFSET);
	LONG64 _node = (LONG64)addr | _tag;
	Node* _nodeAddr = (Node*)addr;

#ifdef _LPOOL_TEST_
	DEBUG_LOG _log;
	_log.operateType = 2;
	_log.threadID = GetCurrentThreadId();
	_log.newTop = (LONG64)_nodeAddr;
	_log.tag = _tag;

	if (_nodeAddr->m_NodeStatus != STATUS_ALLOC) DebugBreak();
	InterlockedExchange(&_nodeAddr->m_NodeStatus, STATUS_FREE);
	//_nodeAddr->m_NodeStatus = STATUS_FREE;
#endif

	while (1)
	{
		Node* _oldTop = m_Top;
		_nodeAddr->m_Next = _oldTop;

#ifdef _LPOOL_TEST_
		_log.oldTop = (LONG64)_oldTop;
		_log.oldNext = (LONG64)((Node*)((LONG64)_oldTop & ADDRESS_MASK))->m_Next;
		_log.newNext = (LONG64)_nodeAddr->m_Next;
#endif

		if (InterlockedCompareExchange64((LONG64*)&m_Top, _node, (LONG64)_oldTop) == (LONG64)_oldTop)
		{
#ifdef _LPOOL_TEST_
			unsigned short _logIdx = (unsigned short)InterlockedIncrement16((SHORT*)&m_LogIdx);
			m_Log[_logIdx] = _log;
#endif
			InterlockedDecrement(&m_UseCount);
			InterlockedIncrement(&m_Capacity);
			

			break;
		}
	}

	return true;
}
#endif

template<typename DATA>
inline DATA* onenewarm::TLSObjectPool<DATA>::Alloc()
{
	Chunk* _chunk = (Chunk*)TlsGetValue(m_TlsIndex);
	if (_chunk == NULL) {
		Chunk* _newChunk = m_ChunkPool.Alloc();
		TlsSetValue(m_TlsIndex, _newChunk);
		_chunk = _newChunk;
	}
		
	DATA* ret = (DATA*)&(_chunk->m_Nodes[_chunk->m_AllocPos++]);

	if (_chunk->m_AllocPos == MAX_CHUNK_SIZE) {
		Chunk* _newChunk = m_ChunkPool.Alloc();
		TlsSetValue(m_TlsIndex, _newChunk);
	}

	return ret;
}

template<typename DATA>
inline bool onenewarm::TLSObjectPool<DATA>::Free(DATA* addr)
{
	auto _freeNode = (typename Chunk::Node*)addr;
	
	DWORD _freeNodeCnt = InterlockedIncrement(&(_freeNode->m_ChunkPointer->m_FreeCnt)); // �ϳ��� Chunk�� ��带 �� ���� �����忡�� ���ÿ� Free�� �� ��Ȳ�� ����ȭ
	
	if (_freeNodeCnt == MAX_CHUNK_SIZE)
	{
		_freeNode->m_ChunkPointer->Clear();
		m_ChunkPool.Free(_freeNode->m_ChunkPointer);
	}

	return true;
}

template<typename DATA>
inline onenewarm::TLSObjectPool<DATA>::TLSObjectPool(DWORD InitChunkCnt, bool PlacementNew) : m_UseNodeCount(0), m_ChunkPool(LockFreeObjectPool<typename TLSObjectPool<DATA>::Chunk>(InitChunkCnt, PlacementNew))
{
	m_TlsIndex = ::TlsAlloc();

	if (m_TlsIndex == TLS_OUT_OF_INDEXES)
	{
		DWORD errCode = GetLastError();

		_LOG(LOG_LEVEL_ERROR, __FILE__, __LINE__, L"TLS_OUT_OF_INDEXES( %d ) : TLS �ε����� ��� �����Ͽ����ϴ�.", errCode);

		CCrashDump::Crash();
	}
}

template<typename DATA>
inline onenewarm::TLSObjectPool<DATA>::~TLSObjectPool()
{
	::TlsFree(m_TlsIndex);
}