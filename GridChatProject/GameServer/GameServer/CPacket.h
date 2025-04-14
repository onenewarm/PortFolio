#pragma once
#include <Windows.h>
#include <Containers\ObjectPool.hpp>


namespace onenewarm
{
#pragma pack(push, 1)
	struct LanHeader {
		BYTE code;
		WORD len;
	};
#pragma pack(pop)

#pragma pack(push, 1)
	struct NetHeader {
		BYTE code;
		WORD len;
		BYTE randKey;
		BYTE checkSum;
	};
#pragma pack(pop)

	class CPacket
	{
		friend class CPacketRef;

	public:
		enum en_PACKET
		{
			PACKET_ERROR = -1,
			BUFFER_DEFAULT = 512		// 패킷의 기본 버퍼 사이즈.
		};

		static		CPacket*	Alloc();
		static		bool		Free(CPacket* pkt);
		static		DWORD		GetChunkSize();
		static		DWORD		GetUseSize();

	public:

		CPacket();
		CPacket(int bufferSize);
		virtual	~CPacket();

		void					Clear(void);
		int						GetBufferSize(void) { return m_BufferSize; }
		int						GetDataSize(void) { return m_DataSize; }
		char*					GetBufferPtr(void) { return m_Buffer; }
		char*					GetWritePtr(void) { return m_WritePos; }
		char*					GetReadPtr(void) { return m_ReadPos;  }
		int						MoveWritePos(int size);
		int						MoveReadPos(int size);


		/*---------------------------
		Parameters :
			char* start : 인코딩 시작위치
			BYTE encodeKey : 인코딩에 사용 될 키

		Description :
			LanEncode는 패킷의 헤더만 설정함, 보안을 위한 인코딩은 하지 않음
			NetEncode는 헤더 설정과 더불어 보안을 위한 인코딩을 수행함
		---------------------------*/
		void		LanEncode(CPacket* pkt, BYTE packetCode);
		void		NetEncode(CPacket* pkt, BYTE packetCode, BYTE packetKey);

		/*------------------------------------------------------------------------------------------
		Parameters :
			char* start : 디코딩 시작위치
			BYTE decodeKey : 디코딩에 사용 될 키 (인코딩 할 때 넣어준 값을 그대로 넣어주어야 한다.)

		Description :
			NetEncode로 인코딩된 패킷을 디코딩함	
		------------------------------------------------------------------------------------------*/
		void		Decode(char* start, BYTE packetKey, BYTE decodeKey, int decodeLen);


		/*------------------------------------------------------------------------------------------
		 넣기.	각 변수 타입마다 모두 만듬.
		------------------------------------------------------------------------------------------*/
		CPacket& operator << (unsigned char value);
		CPacket& operator << (char value);
		CPacket& operator << (short value);
		CPacket& operator << (unsigned short value);
		CPacket& operator << (int value);
		CPacket& operator << (long value);
		CPacket& operator << (float value);
		CPacket& operator << (__int64 value);
		CPacket& operator << (double value);


		/*------------------------------------------------------------------------------------------
		 빼기.	각 변수 타입마다 모두 만듬.
		------------------------------------------------------------------------------------------*/
		CPacket& operator >> (BYTE& value);
		CPacket& operator >> (char& value);
		CPacket& operator >> (short& value);
		CPacket& operator >> (WORD& value);
		CPacket& operator >> (int& value);
		CPacket& operator >> (DWORD& value);
		CPacket& operator >> (float& value);
		CPacket& operator >> (__int64& value);
		CPacket& operator >> (double& value);

		/*------------------------------------------------------------------------------------------
		 데이터 얻기
		
		 Parameters: (char *)Src 포인터 (int)SrcSize.
		 Return: (int)얻어온 데이터 사이즈
		------------------------------------------------------------------------------------------*/
		int		GetData(char* dest, int size);

		/*------------------------------------------------------------------------------------------
		 데이터 삽입
		
		 Parameters: (char *)Dest 포인터. (int)Size.
		 Return: (int)삽입 성공 사이즈
		------------------------------------------------------------------------------------------*/
		int		PutData(const char* src, int srcSize);


		void	AddRef(); // 참조 카운팅의 증가

	protected:
		char*	m_Buffer;
		int		m_BufferSize;
		BOOL	m_IsEncoded;
		int		m_DataSize;
		char*	m_WritePos;
		char*	m_ReadPos;
		SHORT	m_RefCnt;
	};


	class CPacketRef
	{
		/*------------------------------------------------------------------------------------------
			외부에서 Packet을 사용 할 때에, 
			생성자와 소멸자를 통해서 패킷 객체의 수명을 자동으로 관리해주는 클래스
		------------------------------------------------------------------------------------------*/
		friend class MMOServer;
		friend class NetServer;
		friend class LanServer;
		friend class LanClient;
		friend class NetClient;
		friend class Group;

	public:
		CPacketRef();
		CPacketRef(CPacketRef&& rhs);
		CPacketRef(CPacketRef& rhs);
		CPacketRef& operator=(CPacketRef&& rhs);
		CPacketRef& operator=(CPacketRef& rhs);
		~CPacketRef();
		CPacket* operator*();

	private:
		CPacketRef(CPacket* pkt);

	private:
		CPacket*		m_Pkt;
	};


}