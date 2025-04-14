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
			BUFFER_DEFAULT = 512		// ��Ŷ�� �⺻ ���� ������.
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
			char* start : ���ڵ� ������ġ
			BYTE encodeKey : ���ڵ��� ��� �� Ű

		Description :
			LanEncode�� ��Ŷ�� ����� ������, ������ ���� ���ڵ��� ���� ����
			NetEncode�� ��� ������ ���Ҿ� ������ ���� ���ڵ��� ������
		---------------------------*/
		void		LanEncode(CPacket* pkt, BYTE packetCode);
		void		NetEncode(CPacket* pkt, BYTE packetCode, BYTE packetKey);

		/*------------------------------------------------------------------------------------------
		Parameters :
			char* start : ���ڵ� ������ġ
			BYTE decodeKey : ���ڵ��� ��� �� Ű (���ڵ� �� �� �־��� ���� �״�� �־��־�� �Ѵ�.)

		Description :
			NetEncode�� ���ڵ��� ��Ŷ�� ���ڵ���	
		------------------------------------------------------------------------------------------*/
		void		Decode(char* start, BYTE packetKey, BYTE decodeKey, int decodeLen);


		/*------------------------------------------------------------------------------------------
		 �ֱ�.	�� ���� Ÿ�Ը��� ��� ����.
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
		 ����.	�� ���� Ÿ�Ը��� ��� ����.
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
		 ������ ���
		
		 Parameters: (char *)Src ������ (int)SrcSize.
		 Return: (int)���� ������ ������
		------------------------------------------------------------------------------------------*/
		int		GetData(char* dest, int size);

		/*------------------------------------------------------------------------------------------
		 ������ ����
		
		 Parameters: (char *)Dest ������. (int)Size.
		 Return: (int)���� ���� ������
		------------------------------------------------------------------------------------------*/
		int		PutData(const char* src, int srcSize);


		void	AddRef(); // ���� ī������ ����

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
			�ܺο��� Packet�� ��� �� ����, 
			�����ڿ� �Ҹ��ڸ� ���ؼ� ��Ŷ ��ü�� ������ �ڵ����� �������ִ� Ŭ����
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