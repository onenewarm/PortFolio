#pragma once

#include <new.h>
#include <Windows.h>
#include <Utils\Logger.h>
#include <Utils\CCrashDump.h>

/*----------------------------------------------------------------------------------------------------------------
						태그 포인터
//----------------------------------------------------------------------------------------------------------------
	64bit 환경에서 사용가능한 유저모드의 메모리 영역은 128TB이다.(Windows 10 Pro 기준)
	이를 bit로 치환을 하면 47bit이다. 
	즉, User mode의 주소를 식별하기 위해서 앞 쪽 17bit는 필요 없다는 말이 된다.
	왜냐하면 무조건 0으로 채워지기 때문이다.
	그래서 이 앞쪽 17bit에 주소와 관련된 정보들을 보관 할 수 있다.
-----------------------------------------------------------------------------------------------------------------*/

#define ADDRESS_MASK 0x00007FFFFFFFFFFFULL
#define TAG_MASK 0xFFFF800000000000ULL
using namespace std;

namespace onenewarm
{
	template <class DATA>
	class ObjectPool
	{
	public:
		struct Node
		{
			DATA  data;
			Node* next;
		};

		ObjectPool(int initialBlocks = 0, bool PlacementNew = false);
		~ObjectPool();

		DATA*		Alloc(void);
		bool		Free(DATA* pData);
		int			GetCapacityCount(void) { return m_Capacity; }
		int			GetUseCount(void) { return m_UseCount; }

	public:
		bool		m_IsPlacementNew;
		int			m_Capacity;
		int			m_UseCount;
		Node*		m_Top;
	};


	template <class DATA>
	class LockFreeObjectPool
	{
	public:
		struct Node
		{
			DATA	data;
			LONG64  next;
		};

	public:
		LockFreeObjectPool(int initialBlocks = 0, bool PlacementNew = false);
		~LockFreeObjectPool();
		DATA*	Alloc();
		bool	Free(DATA* addr);
		int		GetCapacityCount(void);
		int		GetUseCount(void);

	private:
		bool	m_PlacementNew;
		LONG	m_UniqueTag;
		LONG64	m_Top;
		LONG	m_Capacity;
		DWORD	m_UseCount;
	};


	constexpr DWORD MAX_CHUNK_SIZE = 1000;
	template<typename DATA>
	class TLSObjectPool
	{
		class Chunk
		{
			friend TLSObjectPool;
			friend LockFreeObjectPool<TLSObjectPool::Chunk>;
			struct Node
			{
				DATA	data;
				Chunk*	chunkPtr; //Node가 속하는 Chunk를 Node에서 접근 할 수 있도록 하는 변수
			};

			Chunk();
			~Chunk() { }

			void	Clear();
			DWORD	m_AllocPos; //현재 Chunk로부터 할당된 누적 노드의 수
			DWORD	m_FreeCnt; //현재 Chunk에 반환된 노드의 수
			Node	m_Nodes[MAX_CHUNK_SIZE]; //Chunk에 포함되는 노드들
		};

	public:
		TLSObjectPool(DWORD InitChunkCnt, bool PlacementNew);
		~TLSObjectPool();
		DATA* Alloc();
		bool Free(DATA* addr);
		DWORD GetChunkSize();

	public:
		DWORD m_UseNodeCount;

	private:
		int							m_TlsIndex;
		LockFreeObjectPool<Chunk>	m_ChunkPool;
	};

	template<typename DATA>
	inline TLSObjectPool<DATA>::Chunk::Chunk() : m_AllocPos(0), m_FreeCnt(0)
	{
		for (int cnt = 0; cnt < MAX_CHUNK_SIZE; ++cnt)
			m_Nodes[cnt].chunkPtr = this;
	}

	template<typename DATA>
	inline void TLSObjectPool<DATA>::Chunk::Clear()
	{
		m_AllocPos = 0;
		m_FreeCnt = 0;
	}

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

			newNode->next = m_Top;
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
		if (copiedTop == NULL) continue;
		m_Top = copiedTop->next;
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
		m_Top = m_Top->next;
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
		else CCrashDump::Crash();
	}
}

template<class DATA>
inline bool onenewarm::ObjectPool<DATA>::Free(DATA* pData)
{
	Node* freeNode = (Node*)pData;
	if (m_IsPlacementNew == true) freeNode->~Node();
	freeNode->next = m_Top;
	m_Top = freeNode;
	m_Capacity++;
	m_UseCount--;
	return true;
}


template<class DATA>
inline onenewarm::LockFreeObjectPool<DATA>::LockFreeObjectPool(int initialBlocks, bool PlacementNew) : m_UniqueTag(0), m_Top((LONG64)new Node()), m_Capacity(initialBlocks), m_UseCount(0), m_PlacementNew(PlacementNew)
{
	while (initialBlocks--)
	{
		Node* newNodeAddr = (Node*)malloc(sizeof(Node));
		if (PlacementNew == false) new(newNodeAddr) Node();

		LONG64 newNode = (LONG64)newNodeAddr;
		if (newNodeAddr != NULL) {
			newNodeAddr->next = m_Top;
			m_Top = newNode | (m_UniqueTag << 47);
			m_UniqueTag++;
		}
		else CCrashDump::Crash();
	}
}

template<class DATA>
inline onenewarm::LockFreeObjectPool<DATA>::~LockFreeObjectPool()
{
	LONG capacity = m_Capacity;

	Node* top = (Node*)(m_Top & ADDRESS_MASK);
	LONG64 next = top->next;

	while (capacity--)
	{
		if (m_PlacementNew == false) top->~Node();
		free(top);
		top = (Node*)(next & ADDRESS_MASK);
		next = top->next;
	}
}

template<class DATA>
inline DATA* onenewarm::LockFreeObjectPool<DATA>::Alloc()
{
	LONG  capacity = InterlockedDecrement(&m_Capacity);
	if (capacity < 0)
	{
		InterlockedIncrement(&m_Capacity);
		InterlockedIncrement(&m_UseCount);
		Node* ret = new Node();
		return (DATA*)ret;
	}
	else
	{
		while (1)
		{
			LONG64 snapTop = m_Top;
			Node* snapTopAddr = (Node*)(snapTop & ADDRESS_MASK);
			LONG64 snapNext = snapTopAddr->next;
			DATA* ret = (DATA*)snapTopAddr;

			if (InterlockedCompareExchange64(&m_Top, snapNext, snapTop) == snapTop)
			{
				if (m_PlacementNew)
					new (ret) DATA();

				InterlockedIncrement(&m_UseCount);
				return ret;
			}
		}
	}
}

template<class DATA>
inline bool onenewarm::LockFreeObjectPool<DATA>::Free(DATA* addr)
{
	if (m_PlacementNew)
		addr->~DATA();

	LONG64 tag = (LONG64)InterlockedIncrement(&m_UniqueTag);
	tag = tag << 47;
	LONG64 freeNode = (LONG64)addr | tag;
	Node* freeNodeAddr = (Node*)addr;

	while (1)
	{
		LONG64 snapTop = m_Top;
		freeNodeAddr->next = snapTop;

		if (InterlockedCompareExchange64(&m_Top, freeNode, snapTop) == snapTop)
		{
			InterlockedDecrement(&m_UseCount);
			InterlockedIncrement(&m_Capacity);
			break;
		}
	}
	return true;
}

template<class DATA>
inline int onenewarm::LockFreeObjectPool<DATA>::GetCapacityCount(void)
{
	int snapCapacityCount;
	while (true)
	{
		snapCapacityCount = m_Capacity;
		if (snapCapacityCount >= 0) break;
	}
	return snapCapacityCount;
}

template<class DATA>
inline int onenewarm::LockFreeObjectPool<DATA>::GetUseCount(void)
{
	int snapUseCount;
	while (true)
	{
		snapUseCount = m_UseCount;
		if (snapUseCount >= 0) break;
	}
	return snapUseCount;
}


template<typename DATA>
inline DATA* onenewarm::TLSObjectPool<DATA>::Alloc()
{
	Chunk* curChunk = (Chunk*)TlsGetValue(m_TlsIndex);
	if (curChunk == NULL) {
		Chunk* newChunk = m_ChunkPool.Alloc();
		TlsSetValue(m_TlsIndex, newChunk);
		curChunk = newChunk;
	}
		
	DATA* ret = (DATA*)&(curChunk->m_Nodes[curChunk->m_AllocPos++]);

	if (curChunk->m_AllocPos == MAX_CHUNK_SIZE) {
		Chunk* newChunk = m_ChunkPool.Alloc();
		TlsSetValue(m_TlsIndex, newChunk);
	}

	InterlockedIncrement(&m_UseNodeCount);

	return ret;
}

template<typename DATA>
inline bool onenewarm::TLSObjectPool<DATA>::Free(DATA* addr)
{
	auto freeNode = (typename Chunk::Node*)addr;
	
	DWORD freeNodeCnt = InterlockedIncrement(&(freeNode->chunkPtr->m_FreeCnt)); // 하나의 Chunk의 노드를 두 개의 스레드에서 동시에 Free를 한 상황의 동기화
	
	if (freeNodeCnt == MAX_CHUNK_SIZE)
	{
		freeNode->chunkPtr->Clear();
		m_ChunkPool.Free(freeNode->chunkPtr);
	}
	InterlockedDecrement(&m_UseNodeCount);
	return true;
}

template<typename DATA>
inline DWORD onenewarm::TLSObjectPool<DATA>::GetChunkSize()
{
	return m_ChunkPool.GetUseCount() + m_ChunkPool.GetCapacityCount();
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