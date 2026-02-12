#include "RingBuffer.h"
#include <cstring>
#include <algorithm>

RingBuffer::RingBuffer() : RingBuffer(RING_DEFAULT_SIZE)
{
}

RingBuffer::RingBuffer(int iBufferSize)
	: mBufferSize(iBufferSize)
	, mUsedSize(0)
{
	// 예외 방어: 0 이하 크기 방지
	if (mBufferSize <= 0)
	{
		mBufferSize = RING_DEFAULT_SIZE;
	}

	// +1: FULL/EMPTY 구분을 위해 "항상 비워두는 칸"을 포함한 실제 할당 크기
	mBuffer = new char[mBufferSize + 1];
	mEnd = mBuffer + mBufferSize;
	mFront = mBuffer;
	mRear = mBuffer;
}

RingBuffer::~RingBuffer(void) noexcept
{
	delete[] mBuffer;
	mBuffer = nullptr;
	mBufferSize = 0;
	mUsedSize = 0;
}

int RingBuffer::GetDirectEnqueueSize() const
{
	// [주의] "한 칸 비워두기" 규칙 때문에,
	// front가 start에 있을 때는 end 칸을 쓰면 FULL 판정이 애매해질 수 있어
	// 일부 케이스에서 end까지 못 쓰도록 제한한다.

	// front가 start라면 "맨 끝 칸"은 사용 불가로 보는 규칙
	if (mFront == mBuffer)
	{
		// rear ~ end 직전까지만
		return (mEnd - mRear);
	}
	else if (mFront > mRear)
	{
		return (mFront - mRear - 1);
	}
	else
	{
		// front가 rear보다 앞(또는 같지 않음)이라면,
		// rear부터 end까지는 연속으로 쓸 수 있음 (end 포함)
		return (mEnd - mRear + 1);
	}
}

int RingBuffer::GetDirectDequeueSize() const
{
	if (mFront > mRear)
	{
		// front가 뒤쪽(큰 주소)에 있고 rear가 앞쪽(작은 주소)이면,
		// front~end 까지 읽고 wrap해서 start부터 이어서 읽는 형태가 된다.
		// 따라서 일단 front~end 까지는 연속 구간
		return (mEnd - mFront) + 1;
	}

	return (mRear - mFront);
}

void RingBuffer::MovePointer(char*& pointer, int addValue)
{
	const int len = mBufferSize + 1;						// 실제 할당 길이
	int idx = static_cast<int>(pointer - mBuffer);			// 0..mBufferSize
	idx += addValue;
	idx %= len;
	pointer = mBuffer + idx;
}



// ----------------------------------------------------------------------------
// 포인터만 이동해서 "논리적으로" 데이터 제거/추가하는 함수
// (실제로 memcpy는 안 함)
// ----------------------------------------------------------------------------

bool RingBuffer::MoveFront(int moveSize) noexcept
{
	// 예외 처리
	if (IsEmpty() || moveSize <= 0 || moveSize > mBufferSize)
		return false;

	// 실제 사용량보다 더 빼려 하면 실패
	if (moveSize > mUsedSize)
		return false;

	MovePointer(mFront, moveSize);
	mUsedSize -= moveSize;
	return true;
}


char* RingBuffer::GetFront() const noexcept
{
	return mFront;
}

bool RingBuffer::MoveRear(int moveSize) noexcept
{
	// 예외 처리
	if (IsFull() || moveSize <= 0 || moveSize > mBufferSize)
		return false;

	// 남은 공간보다 더 넣으려 하면 실패
	if (moveSize > GetFreeSize())
		return false;

	MovePointer(mRear, moveSize);
	mUsedSize += moveSize;
	return true;
}


char* RingBuffer::GetRear() const noexcept
{
	return mRear;
}

void RingBuffer::ClearBuffer() noexcept
{
	mRear = mBuffer;
	mFront = mBuffer;
	mUsedSize = 0;
}


// ----------------------------------------------------------------------------
// 상태 판정
// ----------------------------------------------------------------------------

bool RingBuffer::IsFull() const noexcept
{
	// rear가 end이고 front가 start이면 "다음 칸"이 start라서 full
	if (mRear == mEnd && mFront == mBuffer)
		return true;

	if (mRear + 1 == mFront)
		return true;

	return false;
}

// ----------------------------------------------------------------------------
// Enqueue / Dequeue
// ----------------------------------------------------------------------------

bool RingBuffer::Enqueue(const char* data, int size) noexcept
{
	// 예외 처리
	if (data == nullptr || size <= 0 || size > mBufferSize)
		return false;

	// 공간이 부족하면 실패
	if (size > GetFreeSize())
		return false;

	// requestSize가 "연속으로 쓸 수 있는 크기"를 넘으면 split해서 2번 memcpy
	if (size > GetDirectEnqueueSize())
	{
		const int firstSize = GetDirectEnqueueSize();
		const int secondSize = size - firstSize;

		memcpy(mRear, data, firstSize);
		memcpy(mBuffer, data + firstSize, secondSize);
	}
	else
	{
		memcpy(mRear, data, size);
	}

	MovePointer(mRear, size);
	mUsedSize += size;

	return true;
}


bool RingBuffer::Dequeue(char* outData, int size) noexcept
{
	return ReadInternal(outData, size, false);
}

bool RingBuffer::Peek(char* outData, int size) noexcept
{
	return ReadInternal(outData, size, true);
}

// 내부 공통 처리: 복사는 항상 수행, isPeekMode=false일 때만 front/usedSize 이동
bool RingBuffer::ReadInternal(char* outData, int size, bool isPeekMode) noexcept
{
	// 예외 처리
	if (mUsedSize < size || outData == nullptr || size <= 0 || size > mBufferSize)
		return false;

	// front부터 끊기지 않고 읽을 수 있는 연속 구간
	const int direct = GetDirectDequeueSize();

	// 요청 크기가 연속 구간을 넘으면 split copy
	if (size > direct)
	{
		const int firstSize = direct;
		const int secondSize = size - firstSize;

		memcpy(outData, mFront, firstSize);
		memcpy(outData + firstSize, mBuffer, secondSize);
	}
	else
	{
		memcpy(outData, mFront, size);
	}

	// Peek가 아니면 실제로 소모(Front 이동 + Used 감소)
	if (!isPeekMode)
	{
		MovePointer(mFront, size); // wrap 포함 이동
		mUsedSize -= size;
	}

	return true;
}
