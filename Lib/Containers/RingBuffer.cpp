#include "RingBuffer.h"
#include <iostream>

using namespace std;

onenewarm::RingBuffer::RingBuffer() : _bufferSize(10001), _writePos(0), _readPos(0)
{
	InitializeSRWLock(&_lock);
	_buffer = (char*)malloc(10001);
}

onenewarm::RingBuffer::RingBuffer(int bufferSize) : _bufferSize(bufferSize + 1), _writePos(0), _readPos(0)
{
	InitializeSRWLock(&_lock);
	_buffer = (char*)malloc(bufferSize + 1);
}

onenewarm::RingBuffer::~RingBuffer()
{
	free(_buffer);
}


int onenewarm::RingBuffer::Enqueue(const char* chpData, int iSize)
{
	//버퍼에 넣기에는 데이터가 너무 큰 경우
	if (GetFreeSize() < iSize || iSize == 0) {
		_LOG(LOG_LEVEL_ERROR, __FILE__, __LINE__, L"RingBuffer is fulled.");
		onenewarm::CCrashDump::Crash();
	}

	int directEnqueSize = DirectEnqueueSize();

	char* pWritePos = _buffer + _writePos;

	int remainDataSize = iSize;

	if (remainDataSize > directEnqueSize)
	{
		memcpy(pWritePos, chpData, directEnqueSize);
		remainDataSize -= directEnqueSize;
		pWritePos = _buffer;
		chpData += directEnqueSize;
	}

	memcpy(pWritePos, chpData, remainDataSize);
	_writePos = (_writePos + iSize ) % _bufferSize;

	return iSize;
}

int onenewarm::RingBuffer::Dequeue(char* chpDest, int iSize)
{
	if ( GetUseSize() < iSize || iSize == 0) return 0;

	int dequeBytes = 0;

	char* pReadPos = _buffer + _readPos;
	int directDequeSize = DirectDequeueSize();

	int remainDestSize = iSize;

	if (remainDestSize > directDequeSize)
	{
		memcpy(chpDest, pReadPos, directDequeSize);
		remainDestSize -= directDequeSize;
		pReadPos = _buffer;
		chpDest += directDequeSize;
	}

	memcpy(chpDest, pReadPos, remainDestSize);
	dequeBytes += iSize;

	_readPos = (_readPos + iSize) % _bufferSize;

	return dequeBytes;
}

int onenewarm::RingBuffer::Peek(char* chpDest, int iSize)
{
	if (GetUseSize() < iSize || iSize == 0) return 0;

	int peekBytes = 0;

	int directPeekSize = DirectDequeueSize();

	char* pReadPos = _buffer + _readPos;

	int remainDestSize = iSize;

	if (remainDestSize > directPeekSize)
	{
		memcpy(chpDest, pReadPos, directPeekSize);
		remainDestSize -= directPeekSize;
		pReadPos = _buffer;
		chpDest += directPeekSize;
	}

	memcpy(chpDest, pReadPos, remainDestSize);
	peekBytes += iSize;

	return peekBytes;
}

void onenewarm::RingBuffer::ClearBuffer(void)
{
	_writePos = 0;
	_readPos = 0;
}

void onenewarm::RingBuffer::MoveFront(int iSize)
{
	_readPos = (_readPos + iSize) % _bufferSize;
}

void onenewarm::RingBuffer::MoveRear(int iSize)
{
	_writePos = (_writePos + iSize) % _bufferSize;
}

int onenewarm::RingBuffer::DirectEnqueueSize(void)
{  
	int curReadPos = _readPos;

	if (curReadPos <= _writePos)
	{
		if (curReadPos == 0) return _bufferSize - 1 - _writePos;
		else return _bufferSize - _writePos;
	}
	else return (curReadPos - 1) - _writePos;
}

int onenewarm::RingBuffer::DirectDequeueSize(void)
{
	int curWritePos = _writePos;

	if (_readPos <= curWritePos) return curWritePos - _readPos;
	else return _bufferSize - _readPos;
}
