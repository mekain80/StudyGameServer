#include "RingBuffer.h"
#include <cstring>
#include <algorithm>

RingBuffer::RingBuffer(void)
	: mBuffer(nullptr)
	, mBufferSize(RING_DEFAULT_SIZE)
	, mFront(0)
	, mRear(0)
{
	mBuffer = new char[mBufferSize];
}

RingBuffer::RingBuffer(int iBufferSize)
	: mBuffer(nullptr)
	, mBufferSize(iBufferSize)
	, mFront(0)
	, mRear(0)
{
	mBuffer = new char[mBufferSize];
}

RingBuffer::~RingBuffer(void)
{
	delete[] mBuffer;
}

void RingBuffer::Resize(int size)
{
	// 더 작게 줄이는 건 허용 안 함
	if (size <= mBufferSize)
		return;

	int used = GetUseSize();

	char* newBuf = new char[size];

	// 일단 데이터가 있으면 논리 순서대로 복사
	if (used > 0)
	{
		if (mFront < mRear)
		{
			// [mFront ... mRear) 가 한 번에 이어져 있는 경우
			std::memcpy(newBuf, mBuffer + mFront, used);
		}
		else
		{
			// [mFront ... end) + [0 ... mRear) 로 두 번 나뉘어 있는 경우
			int first = mBufferSize - mFront;
			std::memcpy(newBuf, mBuffer + mFront, first);
			if (mRear > 0)
			{
				std::memcpy(newBuf + first, mBuffer, mRear);
			}
		}
	}

	delete[] mBuffer;
	mBuffer = newBuf;
	mBufferSize = size;

	if (used == 0)
	{
		mFront = 0;
		mRear = 0;
	}
	else
	{
		mFront = 0;
		mRear = used;
	}
}


int RingBuffer::GetBufferSize(void)
{
	return mBufferSize;
}

int RingBuffer::GetUseSize(void)
{
	int size = mRear - mFront;

	if (size < 0)
	{
		size += mBufferSize + 1;
	}

	return size;
}

int RingBuffer::GetFreeSize(void)
{
	return mBufferSize - GetUseSize();
}


int RingBuffer::Enqueue(const char* data, int size)
{
	if (size <= 0)
		return 0;

	// 정책: 자리가 모자라면 그냥 실패시키고 0 리턴
	int freeSize = GetFreeSize();
	if (size > freeSize)
		return 0;

	if (mRear + size > mBufferSize)
	{
		int rest = mBufferSize - mRear;
		memcpy(&mBuffer[mRear + 1], data, rest);
		memcpy(&mBuffer[0], data + rest, size - rest);
		mRear += size - mBufferSize;
	}
	else
	{
		memcpy(&mBuffer[mRear + 1], data, size);
		mRear += size;
	}

	return size;
}

int RingBuffer::Dequeue(char* dest, int size)
{
	if (size <= 0) 
		return 0;

	int useSize = GetUseSize();
	// 정책: 자리가 모자라면 그냥 실패시키고 0 리턴
	if (size > useSize)
		return 0;

	int first = std::min(size, mBufferSize - mFront);
	memcpy(dest, mBuffer + mFront, first);

	int remain = size - first;
	if (remain > 0)
		memcpy(dest + first, mBuffer, remain);

	mFront = (mFront + size) % mBufferSize;

	return size;
}

int RingBuffer::Peek(char* dest, int size)
{
	if (size <= 0)
		return 0;

	int useSize = GetUseSize();
	if (size > useSize)
		return 0;  // 정책: 모자라면 실패 (Dequeue와 맞춤)

	int first = std::min(size, mBufferSize - mFront);
	std::memcpy(dest, mBuffer + mFront, first);

	int remain = size - first;
	if (remain > 0)
		std::memcpy(dest + first, mBuffer, remain);

	return size;
}

void RingBuffer::ClearBuffer(void)
{
	mFront = mRear;
}

int RingBuffer::DirectEnqueueSize()
{
	int size = mRear - mFront;
	if (size < 0)
	{
		size = mFront - mRear;
	}
	else
	{
		size = mBufferSize - mRear;
	}

	return size;
}

int RingBuffer::DirectDequeueSize()
{
	int size = mRear - mFront;
	if (size < 0)
	{
		size = mBufferSize - mFront;
	}

	return size;
}

int RingBuffer::MoveRear(int size)
{
	if (size > 0)
	{
		if (mRear + size > mBufferSize)
		{
			mRear += size - mBufferSize;
		}
		else
		{
			mRear += size;
		}
	}

	return size;
}

int RingBuffer::MoveFront(int size)
{
	if (size > 0)
	{
		if (mFront + size > mBufferSize)
		{
			mFront += size - mBufferSize;
		}
		else
		{
			mFront += size;
		}
	}

	return size;
}

char* RingBuffer::GetFrontBufferPtr(void)
{
	return mBuffer + mFront;
}

char* RingBuffer::GetRearBufferPtr(void)
{
	return mBuffer + mRear;
}
