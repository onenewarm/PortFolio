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

64bit 환경에서 사용가능한 유저모드의 메모리 영역은 128TB이다.(Windows 10 Pro 기준)
이를 bit로 치환을 하면 47bit이다. 즉, User mode의 주소를 식별하기 위해서 앞 쪽 17bit는 필요 없다는 말이 된다.
왜냐하면 무조건 0으로 채워지기 때문이다.
그래서 이 앞쪽 17bit에 주소와 관련된 정보들을 보관 할 수 있다.

*/

//#define _LPOOL_TEST_

#define TAG_OFFSET 0x0000800000000000ULL
#define ADDRESS_MASK 0x00007FFFFFFFFFFFULL
#define TAG_MASK 0xFFFF800000000000ULL

using namespace std;

/*

ObjectPool의 TEST에 필요한 코드입니다.
TEST와 관련된 자료는 총 2가지 입니다.
1. STATUS_FREE, STATUS_ALLOC
	이중 Free / 이중 Alloc 테스트

2. DEBUG_LOG
	멀티스레딩 상황에서 발생하는 미세한 경합 문제를 잡기위한 로그입니다.
	g_Log 배열에 로그를 남깁니다.
	최대 로그는 65536개 까지 저장 할 수 있습니다.

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
		// 생성자, 파괴자.
		//
		// Parameters:	(int) 초기 블럭 개수.
		//				(bool) Alloc 시 생성자 / Free 시 파괴자 호출 여부
		// Return:
		//////////////////////////////////////////////////////////////////////////
		ObjectPool(int initialBlocks = 0, bool PlacementNew = false);
		virtual	~ObjectPool();


		//////////////////////////////////////////////////////////////////////////
		// 블럭 하나를 할당받는다.  
		//
		// Parameters: 없음.
		// Return: (DATA *) 데이타 블럭 포인터.
		//////////////////////////////////////////////////////////////////////////
		DATA* Alloc(void);

		//////////////////////////////////////////////////////////////////////////
		// 사용중이던 블럭을 해제한다.
		//
		// Parameters: (DATA *) 블럭 포인터.
		// Return: (BOOL) TRUE, FALSE.
		//////////////////////////////////////////////////////////////////////////
		bool	Free(DATA* pData);

		//////////////////////////////////////////////////////////////////////////
		// 현재 확보 된 블럭 개수를 얻는다. (메모리풀 내부의 전체 개수)
		//
		// Parameters: 없음.
		// Return: (int) 메모리 풀 내부 전체 개수
		//////////////////////////////////////////////////////////////////////////
		int		GetCapacityCount(void) { return m_Capacity; }

		//////////////////////////////////////////////////////////////////////////
		// 현재 사용중인 블럭 개수를 얻는다.
		//
		// Parameters: 없음.
		// Return: (int) 사용중인 블럭 개수.
		//////////////////////////////////////////////////////////////////////////
		int		GetUseCount(void) { return m_UseCount; }


		// 스택 방식으로 반환된 (미사용) 오브젝트 블럭을 관리.

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
			LONG64 oldTop : 연산 전 top Node
			LONG64 newTop : 연산 후 top Node
			LONG64 oldNext : oldTop의 next
			LONG64 newNext : newTop의 next
			LONG64 tag : 새로운 top의 unique ID
			DWORD threadID : 연산을 수행한 스레드ID

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
		// 생성자, 파괴자.
		//
		// Parameters:	(int) 초기 블럭 개수.
		//				(bool) Alloc 시 생성자 / Free 시 파괴자 호출 여부
		// Return:
		//////////////////////////////////////////////////////////////////////////
		LockFreeObjectPool(int initialBlocks = 0, bool PlacementNew = false);
		virtual	~LockFreeObjectPool();


		//////////////////////////////////////////////////////////////////////////
		// 블럭 하나를 할당받는다.  
		//
		// Parameters: 없음.
		// Return: (DATA *) 데이타 블럭 포인터.
		//////////////////////////////////////////////////////////////////////////
		DATA* Alloc();

		//////////////////////////////////////////////////////////////////////////
		// 사용중이던 블럭을 해제한다.
		//
		// Parameters: (DATA *) 블럭 포인터.
		// Return: (BOOL) TRUE, FALSE.
		//////////////////////////////////////////////////////////////////////////
		bool	Free(DATA* addr);


		//////////////////////////////////////////////////////////////////////////
		// 현재 확보 된 블럭 개수를 얻는다. (메모리풀 내부의 전체 개수)
		//
		// Parameters: 없음.
		// Return: (int) 메모리 풀 내부 전체 개수
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
		// 현재 사용중인 블럭 개수를 얻는다.
		//
		// Parameters: 없음.
		// Return: (int) 사용중인 블럭 개수.
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


		// 스택 방식으로 반환된 (미사용) 오브젝트 블럭을 관리.
	private:

		/*

		LONG64 m_UniqueTag : 새롭게 Pool에 추가 된 Node의 Unique ID
		Node* m_Top : Pool의 최상위 Node
		int m_Capacity : 현재 Pool의 개수
		DWORD m_UseCount : 현재 외부에서 사용 중인 Pool의 개수
		bool m_PlacementNew : placement new 여부

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
				//노드의 밸류
				DATA m_Data;

				//Node가 속하는 Chunk를 Node에서 접근 할 수 있도록 하는 변수
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

			//현재 Chunk로부터 할당된 누적 노드의 수
			DWORD m_AllocPos;

			//현재 Chunk에 반환된 노드의 수
			DWORD m_FreeCnt;

			//Chunk에 포함되는 노드들
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

ObjectPool 안에 있는 Pool이 있고, 외부에서 사용 중인 Pool이 있을 텐데,
내부에 있는 Pool에 대해서만 메모리 할당 해제를 합니다.
외부에 있는 Pool을 메모리 할당 해제하게 된다면 외부에서 접근중인 메모리가 free상태가 돼 Access Violation 에러가 발생하거나,
같은 주소가 재사용 돼 예측 할 수 없는 문제를 야기 할 수 있다.

외부에서 ObjectPool을 사용 할 때 ObjectPool의 Alloc, Free 함수를 사용을 할 것이다.
이를 바로 사용하게 되면 ObjectPool이 소멸 했을 때 문제가 발생 할 것이다.
그렇기 떄문에 현재 ObjectPool의 수명을 관리하는 bool 변수가 필요하다고 판단이 된다.
또한, ObjectPool을 사용 할 때 Alloc, Free를 바로 호출 하는 것이 아니라 Alloc, Free를 래핑해야 된다고 생각이 된다.

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
	
	DWORD _freeNodeCnt = InterlockedIncrement(&(_freeNode->m_ChunkPointer->m_FreeCnt)); // 하나의 Chunk의 노드를 두 개의 스레드에서 동시에 Free를 한 상황의 동기화
	
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

		_LOG(LOG_LEVEL_ERROR, __FILE__, __LINE__, L"TLS_OUT_OF_INDEXES( %d ) : TLS 인덱스를 모두 소진하였습니다.", errCode);

		CCrashDump::Crash();
	}
}

template<typename DATA>
inline onenewarm::TLSObjectPool<DATA>::~TLSObjectPool()
{
	::TlsFree(m_TlsIndex);
}