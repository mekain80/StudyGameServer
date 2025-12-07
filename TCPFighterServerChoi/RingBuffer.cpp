#include "RingBuffer.h"
#include <cstring>
#include <algorithm>

RingBuffer::RingBuffer(void)
	: mBuffer(nullptr)
	, mBufferSize(DEFAULT_SIZE)
	, mFront(0)
	, mRear(0)
	, mIsEmpty(true)
{
	mBuffer = new char[DEFAULT_SIZE];
}

RingBuffer::RingBuffer(int iBufferSize)
	: mBuffer(nullptr)
	, mBufferSize(iBufferSize)
	, mFront(0)
	, mRear(0)
	, mIsEmpty(true)
{
	mBuffer = new char[iBufferSize];
}

RingBuffer::~RingBuffer(void)
{
	delete[] mBuffer;
}

void RingBuffer::Resize(int newSize)
{
	// 더 작게 줄이는 건 허용 안 함
	if (newSize <= mBufferSize)
		return;

	int used = GetUseSize();

	char* newBuf = new char[newSize];

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
	mBufferSize = newSize;

	if (used == 0)
	{
		mFront = 0;
		mRear = 0;
		mIsEmpty = true;
	}
	else
	{
		mFront = 0;
		mRear = used;
		mIsEmpty = false;
	}
}


int RingBuffer::GetBufferSize(void)
{
	return mBufferSize;
}

int RingBuffer::GetUseSize(void)
{
	if (mIsEmpty)
		return 0;

	if (mFront == mRear)
		return mBufferSize; // 가득 찬 상태

	if (mFront < mRear)
		return (mRear - mFront);
	else // mRear < mFront
		return (mBufferSize - mFront) + mRear;
}

int RingBuffer::GetFreeSize(void)
{
	// 전체 크기 - 사용 중
	return mBufferSize - GetUseSize();
}


int RingBuffer::Enqueue(const char* chpData, int iSize)
{
	if (iSize <= 0)
		return 0;

	int freeSize = GetFreeSize();
	if (freeSize <= 0)
		return 0;

	// 정책: 자리가 모자라면 그냥 실패시키고 0 리턴
	if (iSize > freeSize)
		return 0;

	// 1차 복사: rear ~ 버퍼 끝까지 한 번에 쓸 수 있는 구간
	int firstSize = mBufferSize - mRear;
	if (firstSize > iSize)
		firstSize = iSize;

	std::memcpy(mBuffer + mRear, chpData, firstSize);

	// 2차 복사: 남은 데이터는 버퍼 맨 앞으로 래핑해서 쓰기
	int remainSize = iSize - firstSize;
	if (remainSize > 0)
	{
		std::memcpy(mBuffer, chpData + firstSize, remainSize);
	}

	// rear 이동 + 래핑 처리
	mRear = (mRear + iSize) % mBufferSize;

	// 데이터가 들어왔으니 더 이상 비어 있지 않음
	mIsEmpty = false;

	return iSize;
}

int RingBuffer::Dequeue(char* chpDest, int iSize)
{
	if (iSize <= 0)
		return 0;

	int useSize = GetUseSize();
	// 정책: 자리가 모자라면 그냥 실패시키고 0 리턴
	if (iSize > useSize)
		return 0;

	int first = std::min(iSize, mBufferSize - mFront);
	memcpy(chpDest, mBuffer + mFront, first);

	int remain = iSize - first;
	if (remain > 0)
		memcpy(chpDest + first, mBuffer, remain);

	mFront = (mFront + iSize) % mBufferSize;
	if (mFront == mRear)
		mIsEmpty = true;

	return iSize;
}

int RingBuffer::Peek(char* chpDest, int iSize)
{
	if (iSize <= 0)
		return 0;

	int useSize = GetUseSize();
	if (useSize == 0)
		return 0;

	if (iSize > useSize)
		return 0;  // 정책: 모자라면 실패 (Dequeue와 맞춤)

	int first = std::min(iSize, mBufferSize - mFront);
	std::memcpy(chpDest, mBuffer + mFront, first);

	int remain = iSize - first;
	if (remain > 0)
		std::memcpy(chpDest + first, mBuffer, remain);

	return iSize;
}

void RingBuffer::ClearBuffer(void)
{
	memset(mBuffer, 0, mBufferSize);
	mFront = 0;
	mRear = 0;
	mIsEmpty = true;
}


int RingBuffer::DirectEnqueueSize()
{
	int free = GetFreeSize();
	if (free == 0) return 0;

	if (mRear >= mFront)
		return std::min(free, mBufferSize - mRear);
	else
		return mFront - mRear;
}

int RingBuffer::DirectDequeueSize()
{
	int used = GetUseSize();
	if (used == 0) return 0;

	if (mRear > mFront)
		return mRear - mFront;
	else
		return mBufferSize - mFront;
}

int RingBuffer::MoveRear(int iSize)
{
	if (iSize < 0 || iSize > GetFreeSize())
		return 0;

	mRear = (mRear + iSize) % mBufferSize;
	if (iSize > 0) mIsEmpty = false;
	return iSize;
}

int RingBuffer::MoveFront(int iSize)
{
	if (iSize < 0 || iSize > GetUseSize())
		return 0;

	mFront = (mFront + iSize) % mBufferSize;
	if (mFront == mRear) mIsEmpty = true;
	return iSize;
}


char* RingBuffer::GetFrontBufferPtr(void)
{
	return mBuffer + mFront;
}

char* RingBuffer::GetRearBufferPtr(void)
{
	return mBuffer + mRear;
}
