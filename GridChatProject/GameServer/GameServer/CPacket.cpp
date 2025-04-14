#include "CPacket.h"

using namespace onenewarm;

TLSObjectPool<CPacket> s_PacketPool(0, false);

CPacket* CPacket::Alloc() {

	CPacket* ret = s_PacketPool.Alloc();
	ret->Clear();
	ret->m_RefCnt = 1;

	return ret;
}


bool onenewarm::CPacket::Free(CPacket* pkt)
{
	if (InterlockedDecrement16(&pkt->m_RefCnt) == 0) {
		s_PacketPool.Free(pkt);
		return true;
	}

	return false;
}

DWORD onenewarm::CPacket::GetChunkSize()
{
	return s_PacketPool.GetChunkSize();
}

DWORD onenewarm::CPacket::GetUseSize()
{
	return s_PacketPool.m_UseNodeCount;
}

onenewarm::CPacket::CPacket() : m_Buffer((char*)malloc(en_PACKET::BUFFER_DEFAULT)), m_WritePos(m_Buffer), m_ReadPos(m_Buffer), m_BufferSize(en_PACKET::BUFFER_DEFAULT), m_DataSize(0), m_RefCnt(0), m_IsEncoded(false)
{
}

onenewarm::CPacket::CPacket(int bufferSize) : m_Buffer((char*)malloc(bufferSize)), m_WritePos(m_Buffer), m_ReadPos(m_Buffer), m_BufferSize(bufferSize), m_DataSize(0), m_RefCnt(0), m_IsEncoded(false)
{
}

onenewarm::CPacket::~CPacket()
{
	m_Buffer -= m_DataSize;
	free(m_Buffer);
}

void onenewarm::CPacket::Clear(void)
{
	m_IsEncoded = false;
	m_WritePos = m_Buffer;
	m_ReadPos = m_Buffer;
	m_DataSize = 0;
}

int onenewarm::CPacket::MoveWritePos(int size)
{
	if ((LONG64)m_WritePos - (LONG64)m_Buffer + size > m_BufferSize) return en_PACKET::PACKET_ERROR;

	m_WritePos += size;
	m_DataSize += size;

	return size;
}

int onenewarm::CPacket::MoveReadPos(int size)
{
	if ((LONG64)m_ReadPos - (LONG64)m_Buffer + size > m_BufferSize) return en_PACKET::PACKET_ERROR;

	m_ReadPos += size;
	m_DataSize -= size;

	return size;
}

void onenewarm::CPacket::LanEncode(CPacket* pkt, BYTE packetCode)
{
	if (InterlockedOr8((char*)&pkt->m_IsEncoded, 1) == 1) return;

	LanHeader* header = (LanHeader*)pkt->GetBufferPtr();

	header->code = packetCode;

	WORD payloadSize = (WORD)pkt->GetDataSize() - sizeof(LanHeader);

	header->len = payloadSize;
}

void onenewarm::CPacket::NetEncode(CPacket* pkt, BYTE packetCode, BYTE packetKey)
{
	if (InterlockedOr8((char*)&pkt->m_IsEncoded, 1) == 1) return;

	NetHeader* header = (NetHeader*)pkt->GetBufferPtr();

	WORD payloadSize = (WORD)pkt->GetDataSize() - sizeof(NetHeader);

	header->code = packetCode;
	header->len = payloadSize;

	char* payloadPtr = pkt->GetBufferPtr() + sizeof(NetHeader);

	DWORD sumPayloadsPerByte = 0;
	WORD sumPayloadPos = 0;

	while (sumPayloadPos < payloadSize) {
		sumPayloadsPerByte += payloadPtr[sumPayloadPos++];
	}

	header->checkSum = sumPayloadsPerByte % 256;
	header->randKey = (BYTE)(rand() % 256);


	char* encodePtr = (char*)&header->checkSum;
	int counter = 1;

	BYTE prevP = 0;
	BYTE prevE = 0;

	while (encodePtr < m_WritePos) {
		BYTE p = *encodePtr ^ (prevP + header->randKey + counter);
		BYTE e = p ^ (prevE + packetKey + counter);

		*encodePtr = e;

		prevP = p;
		prevE = e;

		++encodePtr;
		++counter;
	}
}

void onenewarm::CPacket::Decode(char* start, BYTE packetKey, BYTE decodeKey, int decodeLen)
{
	char* decodePtr = start;
	char* endPtr = start + decodeLen;
	int counter = 1;

	BYTE prevP = 0;
	BYTE prevE = 0;

	while (decodePtr < endPtr) {
		BYTE p = *decodePtr ^ (prevE + packetKey + counter);
		BYTE d = p ^ (prevP + decodeKey + counter);

		prevP = p;
		prevE = *decodePtr;

		*decodePtr = d;

		++decodePtr;
		++counter;
	}
}

CPacket& onenewarm::CPacket::operator<<(unsigned char value)
{
	if (m_WritePos - m_Buffer + sizeof(value) > m_BufferSize) return *this;

	*m_WritePos = value;
	m_WritePos += sizeof(value);

	m_DataSize += sizeof(value);

	return *this;
}

CPacket& onenewarm::CPacket::operator<<(char value)
{
	if (m_WritePos - m_Buffer + sizeof(value) > m_BufferSize) return *this;

	*m_WritePos = value;
	m_WritePos += sizeof(value);

	m_DataSize += sizeof(value);

	return *this;
}

CPacket& onenewarm::CPacket::operator<<(short value)
{
	if (m_WritePos - m_Buffer + sizeof(value) > m_BufferSize)  return *this;

	*((short*)m_WritePos) = value;
	m_WritePos += sizeof(value);

	m_DataSize += sizeof(value);

	return *this;
}

CPacket& onenewarm::CPacket::operator<<(unsigned short value)
{
	if (m_WritePos - m_Buffer + sizeof(value) > m_BufferSize)  return *this;

	*((unsigned short*)m_WritePos) = value;
	m_WritePos += sizeof(value);

	m_DataSize += sizeof(value);

	return *this;
}

CPacket& onenewarm::CPacket::operator<<(int value)
{
	if (m_WritePos - m_Buffer + sizeof(value) > m_BufferSize)  return *this;

	*((int*)m_WritePos) = value;
	m_WritePos += sizeof(value);

	m_DataSize += sizeof(value);

	return *this;
}

CPacket& onenewarm::CPacket::operator<<(long value)
{
	if (m_WritePos - m_Buffer + sizeof(value) > m_BufferSize)  return *this;

	*((long*)m_WritePos) = value;
	m_WritePos += sizeof(value);

	m_DataSize += sizeof(value);

	return *this;
}

CPacket& onenewarm::CPacket::operator<<(float value)
{
	if (m_WritePos - m_Buffer + sizeof(value) > m_BufferSize)  return *this;

	*((float*)m_WritePos) = value;
	m_WritePos += sizeof(value);

	m_DataSize += sizeof(value);

	return *this;
}

CPacket& onenewarm::CPacket::operator<<(__int64 value)
{
	if (m_WritePos - m_Buffer + sizeof(value) > m_BufferSize)  return *this;

	*((__int64*)m_WritePos) = value;
	m_WritePos += sizeof(value);

	m_DataSize += sizeof(value);

	return *this;
}

CPacket& onenewarm::CPacket::operator<<(double value)
{
	if (m_WritePos - m_Buffer + sizeof(value) >= m_BufferSize)  return *this;

	*((double*)m_WritePos) = value;
	m_WritePos += sizeof(value);

	m_DataSize += sizeof(value);

	return *this;
}

CPacket& onenewarm::CPacket::operator>>(BYTE& value)
{
	if (sizeof(value) > m_DataSize)
	{
		throw en_PACKET::PACKET_ERROR;
		return *this; // 사용불가 상황
	}

	value = *m_ReadPos;
	m_ReadPos += sizeof(value);

	m_DataSize -= sizeof(value);

	return *this;
}

CPacket& onenewarm::CPacket::operator>>(char& value)
{
	if (sizeof(value) > m_DataSize)
	{
		throw en_PACKET::PACKET_ERROR;
		return *this; // 사용불가 상황
	}

	value = *m_ReadPos;
	m_ReadPos += sizeof(value);

	m_DataSize -= sizeof(value);

	return *this;
}

CPacket& onenewarm::CPacket::operator>>(short& value)
{
	if (sizeof(value) > m_DataSize)
	{
		throw en_PACKET::PACKET_ERROR;
		return *this; // 사용불가 상황
	}

	value = *((short*)m_ReadPos);
	m_ReadPos += sizeof(value);

	m_DataSize -= sizeof(value);

	return *this;
}

CPacket& onenewarm::CPacket::operator>>(WORD& value)
{
	if (sizeof(value) > m_DataSize)
	{
		throw en_PACKET::PACKET_ERROR;
		return *this; // 사용불가 상황
	}
	value = *((WORD*)m_ReadPos);
	m_ReadPos += sizeof(value);

	m_DataSize -= sizeof(value);

	return *this;
}

CPacket& onenewarm::CPacket::operator>>(int& value)
{
	if (sizeof(value) > m_DataSize)
	{
		throw en_PACKET::PACKET_ERROR;
		return *this; // 사용불가 상황
	}
	value = *((int*)m_ReadPos);
	m_ReadPos += sizeof(value);

	m_DataSize -= sizeof(value);

	return *this;
}

CPacket& onenewarm::CPacket::operator>>(DWORD& value)
{
	if (sizeof(value) > m_DataSize)
	{
		throw en_PACKET::PACKET_ERROR;
		return *this; // 사용불가 상황
	}
	value = *((DWORD*)m_ReadPos);
	m_ReadPos += sizeof(value);

	m_DataSize -= sizeof(value);

	return *this;
}

CPacket& onenewarm::CPacket::operator>>(float& value)
{
	if (sizeof(value) > m_DataSize)
	{
		throw en_PACKET::PACKET_ERROR;
		return *this; // 사용불가 상황
	}
	value = *((float*)m_ReadPos);
	m_ReadPos += sizeof(value);

	m_DataSize -= sizeof(value);

	return *this;
}

CPacket& onenewarm::CPacket::operator>>(__int64& value)
{
	if (sizeof(value) > m_DataSize)
	{
		throw en_PACKET::PACKET_ERROR;
		return *this; // 사용불가 상황
	}
	value = *((__int64*)m_ReadPos);
	m_ReadPos += sizeof(value);

	m_DataSize -= sizeof(value);

	return *this;
}

CPacket& onenewarm::CPacket::operator>>(double& value)
{
	if (sizeof(value) > m_DataSize)
	{
		throw en_PACKET::PACKET_ERROR;
		return *this; // 사용불가 상황
	}
	value = *((double*)m_ReadPos);
	m_ReadPos += sizeof(value);

	m_DataSize -= sizeof(value);

	return *this;
}

int onenewarm::CPacket::GetData(char* dest, int size)
{
	if (size > m_DataSize)
	{
		throw en_PACKET::PACKET_ERROR;
		return en_PACKET::PACKET_ERROR;
	}

	::memcpy(dest, m_ReadPos, size);

	m_ReadPos += size;
	m_DataSize -= size;

	return size;
}

int onenewarm::CPacket::PutData(const char* src, int srcSize)
{
	if (m_WritePos - m_Buffer + srcSize > m_BufferSize) return en_PACKET::PACKET_ERROR;

	::memcpy(m_WritePos, src, srcSize);

	m_WritePos += srcSize;
	m_DataSize += srcSize;

	return srcSize;
}

void onenewarm::CPacket::AddRef()
{
	InterlockedIncrement16(&m_RefCnt);
}

onenewarm::CPacketRef::CPacketRef() : m_Pkt(nullptr)
{
}

onenewarm::CPacketRef::CPacketRef(CPacketRef&& rhs) : m_Pkt(rhs.m_Pkt)
{
}

onenewarm::CPacketRef::CPacketRef(CPacketRef& rhs) : m_Pkt(rhs.m_Pkt)
{
	m_Pkt->AddRef();
}

CPacketRef& onenewarm::CPacketRef::operator=(CPacketRef&& rhs)
{
	this->m_Pkt = rhs.m_Pkt;
	return *this;
}

CPacketRef& onenewarm::CPacketRef::operator=(CPacketRef& rhs)
{
	this->m_Pkt = rhs.m_Pkt;
	m_Pkt->AddRef();
	return *this;
}

onenewarm::CPacketRef::~CPacketRef()
{
	if (m_Pkt != nullptr)
	{
		CPacket::Free(m_Pkt);
	}
}


CPacket* onenewarm::CPacketRef::operator*()
{
	return m_Pkt;
}

onenewarm::CPacketRef::CPacketRef(CPacket* pkt) : m_Pkt(pkt)
{
}



