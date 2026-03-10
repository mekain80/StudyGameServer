#include "stdafx.h"
#include "RingBuffer.h"

#include <cstring>

RingBuffer::RingBuffer() : RingBuffer(RING_DEFAULT_SIZE)
{
}

RingBuffer::RingBuffer(std::size_t bufferSize)
	: mBuffer(nullptr)
	, mEnd(nullptr)
	, mReadWindow(nullptr)
	, mWriteWindow(nullptr)
	, mCapacity(bufferSize)
{
	// 예외 방어: 0 크기 방지
	if (mCapacity == 0)
	{
		mCapacity = RING_DEFAULT_SIZE;
	}

	// +1: FULL/EMPTY 구분을 위해 "항상 비워두는 칸"을 포함한 실제 할당 크기
	mBuffer = new char[mCapacity + 1];
	mEnd = mBuffer + mCapacity;
	mReadWindow = mBuffer;
	mWriteWindow = mBuffer;
}

RingBuffer::~RingBuffer() noexcept
{
	delete[] mBuffer;
	mBuffer = nullptr;
	mEnd = nullptr;
	mReadWindow = nullptr;
	mWriteWindow = nullptr;
	mCapacity = 0;
}

std::size_t RingBuffer::GetUseSize() const noexcept
{
	// write/read window의 상대 위치로 사용량 계산 (mBufferEnd는 inclusive)
	if (mWriteWindow >= mReadWindow)
	{
		return static_cast<std::size_t>(mWriteWindow - mReadWindow);
	}

	// wrap: [readWindow..end] + [start..writeWindow)
	return static_cast<std::size_t>((mEnd - mReadWindow) + 1 + (mWriteWindow - mBuffer));
}

std::size_t RingBuffer::GetDirectEnqueueSize() const
{
	// [주의] "한 칸 비워두기" 규칙 때문에,
	// read window가 start에 있을 때는 end 칸을 쓰면 FULL 판정이 애매해질 수 있어
	// 일부 케이스에서 end까지 못 쓰도록 제한한다.

	// read window가 start라면 "맨 끝 칸"은 사용 불가로 보는 규칙
	if (mReadWindow == mBuffer)
	{
		// write window ~ end 직전까지만
		return static_cast<std::size_t>(mEnd - mWriteWindow);
	}
	else if (mReadWindow > mWriteWindow)
	{
		return static_cast<std::size_t>(mReadWindow - mWriteWindow - 1);
	}
	else
	{
		// write window부터 end까지는 연속으로 쓸 수 있음 (end 포함)
		return static_cast<std::size_t>(mEnd - mWriteWindow + 1);
	}

	return 0;
}

std::size_t RingBuffer::GetDirectDequeueSize() const
{
	if (mReadWindow > mWriteWindow)
	{
		// read window~end 까지는 연속 구간
		return static_cast<std::size_t>((mEnd - mReadWindow) + 1);
	}

	return static_cast<std::size_t>(mWriteWindow - mReadWindow);
}

void RingBuffer::MovePointer(char*& pointer, std::size_t addValue)
{
	const std::size_t len = mCapacity + 1; // 실제 할당 길이
	std::size_t idx = static_cast<std::size_t>(pointer - mBuffer); // 0..mCapacity
	idx += addValue;
	idx %= len;
	pointer = mBuffer + idx;
}

// ----------------------------------------------------------------------------
// 포인터만 이동해서 "논리적으로" 데이터 제거/추가하는 함수
// (실제로 memcpy는 안 함)
// ----------------------------------------------------------------------------

bool RingBuffer::MoveFront(std::size_t moveSize) noexcept
{
	// 예외 처리
	if (IsEmpty() || moveSize == 0 || moveSize > mCapacity)
		return false;

	// 실제 사용량보다 더 빼려 하면 실패
	const std::size_t used = GetUseSize();
	if (moveSize > used)
		return false;

	MovePointer(mReadWindow, moveSize);
	return true;
}

char* RingBuffer::GetFront() const noexcept
{
	return mReadWindow;
}

bool RingBuffer::MoveRear(std::size_t moveSize) noexcept
{
	// 예외 처리
	if (IsFull() || moveSize == 0 || moveSize > mCapacity)
		return false;

	// 남은 공간보다 더 넣으려 하면 실패
	const std::size_t freeSize = GetFreeSize();
	if (moveSize > freeSize)
		return false;

	MovePointer(mWriteWindow, moveSize);
	return true;
}

char* RingBuffer::GetRear() const noexcept
{
	return mWriteWindow;
}

void RingBuffer::ClearBuffer() noexcept
{
	mWriteWindow = mBuffer;
	mReadWindow = mBuffer;
}

// ----------------------------------------------------------------------------
// 상태 판정
// ----------------------------------------------------------------------------

bool RingBuffer::IsFull() const noexcept
{
	// write window가 end이고 read window가 start이면 "다음 칸"이 start라서 full
	if (mWriteWindow == mEnd && mReadWindow == mBuffer)
		return true;

	if (mWriteWindow + 1 == mReadWindow)
		return true;

	return false;
}

// ----------------------------------------------------------------------------
// Enqueue / Dequeue
// ----------------------------------------------------------------------------

bool RingBuffer::Enqueue(const char* data, std::size_t size) noexcept
{
	// 예외 처리
	if (data == nullptr || size == 0 || size > mCapacity)
		return false;

	// 공간이 부족하면 실패
	if (size > GetFreeSize())
		return false;

	const std::size_t direct = GetDirectEnqueueSize();

	// requestSize가 "연속으로 쓸 수 있는 크기"를 넘으면 split해서 2번 memcpy
	if (size > direct)
	{
		const std::size_t firstSize = direct;
		const std::size_t secondSize = size - firstSize;

		if (firstSize > 0)
			std::memcpy(mWriteWindow, data, firstSize);
		std::memcpy(mBuffer, data + firstSize, secondSize);
	}
	else
	{
		std::memcpy(mWriteWindow, data, size);
	}

	MovePointer(mWriteWindow, size);
	return true;
}

bool RingBuffer::Dequeue(char* outData, std::size_t size) noexcept
{
	return ReadInternal(outData, size, false);
}

bool RingBuffer::Peek(char* outData, std::size_t size) noexcept
{
	return ReadInternal(outData, size, true);
}

// 내부 공통 처리: 복사는 항상 수행, isPeekMode=false일 때만 front 이동
bool RingBuffer::ReadInternal(char* outData, std::size_t size, bool isPeekMode) noexcept
{
	// 예외 처리
	if (outData == nullptr || size == 0 || size > mCapacity)
		return false;

	if (GetUseSize() < size)
		return false;

	// read window부터 끊기지 않고 읽을 수 있는 연속 구간
	const std::size_t direct = GetDirectDequeueSize();

	// 요청 크기가 연속 구간을 넘으면 split copy
	if (size > direct)
	{
		const std::size_t firstSize = direct;
		const std::size_t secondSize = size - firstSize;

		if (firstSize > 0)
			std::memcpy(outData, mReadWindow, firstSize);
		std::memcpy(outData + firstSize, mBuffer, secondSize);
	}
	else
	{
		std::memcpy(outData, mReadWindow, size);
	}

	// Peek가 아니면 실제로 소모(read window 이동)
	if (!isPeekMode)
	{
		MovePointer(mReadWindow, size); // wrap 포함 이동
	}

	return true;
}
