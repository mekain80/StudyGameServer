#include "RingBuffer.h"
#include <cstring>
#include <algorithm>

RingBuffer::RingBuffer(void)
	: mBuffer(nullptr)
	, mBufferSize(RING_DEFAULT_SIZE)
	, mFront(0)
	, mRear(0)
	, mUseSize(0)
{
	mBuffer = new char[mBufferSize];
}

RingBuffer::RingBuffer(int iBufferSize)
	: mBuffer(nullptr)
	, mBufferSize(iBufferSize)
	, mFront(0)
	, mRear(0)
	, mUseSize(0)
{
	mBuffer = new char[mBufferSize];
}

RingBuffer::~RingBuffer(void)
{
	delete[] mBuffer;
}

void RingBuffer::Resize(int size)
{
	if (size <= mBufferSize)
		return;

	int used = mUseSize;
	char* newBuf = new char[size];

	if (used > 0)
	{
		if (mFront < mRear)
		{
			std::memcpy(newBuf, mBuffer + mFront, used);
		}
		else
		{
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

	mFront = 0;
	mRear = used;
	mUseSize = used;
}

int RingBuffer::GetBufferSize(void)
{
	return mBufferSize;
}

int RingBuffer::GetUseSize(void)
{
	return mUseSize;
}

int RingBuffer::GetFreeSize(void)
{
	return mBufferSize - mUseSize;
}

int RingBuffer::Enqueue(const char* data, int size)
{
	if (size <= 0)
		return 0;

	int freeSize = GetFreeSize();
	if (size > freeSize)
		return 0;

	int first = std::min(size, mBufferSize - mRear);
	std::memcpy(mBuffer + mRear, data, first);

	int remain = size - first;
	if (remain > 0)
		std::memcpy(mBuffer, data + first, remain);

	mRear = (mRear + size) % mBufferSize;
	mUseSize += size;

	return size;
}

int RingBuffer::Dequeue(char* dest, int size)
{
	if (size <= 0)
		return 0;

	if (size > mUseSize)
		return 0;

	int first = std::min(size, mBufferSize - mFront);
	std::memcpy(dest, mBuffer + mFront, first);

	int remain = size - first;
	if (remain > 0)
		std::memcpy(dest + first, mBuffer, remain);

	mFront = (mFront + size) % mBufferSize;
	mUseSize -= size;                     // ★ 사용량 감소

	return size;
}

int RingBuffer::Peek(char* dest, int size)
{
	if (size <= 0)
		return 0;

	if (size > mUseSize)
		return 0;

	int first = std::min(size, mBufferSize - mFront);
	std::memcpy(dest, mBuffer + mFront, first);

	int remain = size - first;
	if (remain > 0)
		std::memcpy(dest + first, mBuffer, remain);

	return size;
}

void RingBuffer::ClearBuffer(void)
{
	mFront = 0;
	mRear = 0;
	mUseSize = 0;
}

int RingBuffer::DirectEnqueueSize()
{
	int freeSize = GetFreeSize();
	if (freeSize <= 0)
		return 0;

	return std::min(freeSize, mBufferSize - mRear);
}

int RingBuffer::DirectDequeueSize()
{
	if (mUseSize <= 0)
		return 0;

	return std::min(mUseSize, mBufferSize - mFront);
}

int RingBuffer::MoveRear(int size)
{
	if (size <= 0)
		return 0;

	if (size > GetFreeSize())
		return 0;

	mRear = (mRear + size) % mBufferSize;
	mUseSize += size;
	return size;
}

int RingBuffer::MoveFront(int size)
{
	if (size <= 0)
		return 0;

	if (size > mUseSize)
		return 0;

	mFront = (mFront + size) % mBufferSize;
	mUseSize -= size;
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