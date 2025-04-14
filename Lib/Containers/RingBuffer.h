#pragma once
#include <windows.h>
#include <Utils\CCrashDump.h>
#include <Utils\Logger.h>

namespace onenewarm
{
	class RingBuffer
	{
	public:
		RingBuffer();
		RingBuffer(int bufferSize);
		~RingBuffer();

		int			Enqueue(const char* chpData, int iSize);
		int			Dequeue(char* chpDest, int iSize);
		int			Peek(char* chpDest, int iSize);
		void		ClearBuffer(void);
		void		MoveFront(int iSize);
		void		MoveRear(int iSize);

		__forceinline int	GetBufferSize(void)
		{
			return _bufferSize - 1;
		}

		__forceinline int	GetUseSize(void)
		{
			return ((_writePos + _bufferSize) - _readPos) % _bufferSize;
		}

		__forceinline int	GetFreeSize(void)
		{
			return (_bufferSize - 1) - GetUseSize();
		}
		__forceinline char* GetFrontBufferPtr(void)
		{
			return _buffer + _readPos;
		}
		__forceinline char* GetRearBufferPtr(void)
		{
			return _buffer + _writePos;
		}

		void		Lock() { AcquireSRWLockExclusive(&_lock); }
		void		UnLock() { ReleaseSRWLockExclusive(&_lock); }

	private:
		int			DirectEnqueueSize(void);
		int			DirectDequeueSize(void);

	private:
		char*		_buffer;
		int			_bufferSize;
		int			_writePos; // 쓰기 가능 지점 (byte 단위)
		int			_readPos; // 쓰기 가능 지점 (byte 단위):
		SRWLOCK		_lock;
	};
}

