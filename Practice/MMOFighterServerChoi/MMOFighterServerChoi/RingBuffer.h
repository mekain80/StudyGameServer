#pragma once

#include <cstddef>

static constexpr std::size_t RING_DEFAULT_SIZE = 5000;

// 고성능 송수신용 Ring Buffer 클래스, 부분 Enqueue/Dequeue 미지원(요청 크기 미만이면 실패).
// 내부 버퍼는 (mCapacity + 1)로 할당하며, Full/Empty 구분을 위해 항상 1칸을 비움.
class RingBuffer
{
public:
	// 기본 생성자.
	RingBuffer();
	explicit RingBuffer(std::size_t bufferSize);
	~RingBuffer() noexcept;

	// 끊기지 않고 한 번에 Enqueue 가능한 연속 공간 크기를 반환
	// @return 연속적으로 쓸 수 있는 공간 크기
	std::size_t GetDirectEnqueueSize() const;

	// 끊기지 않고 한 번에 Dequeue 가능한 연속 데이터 크기를 반환
	// @return 연속적으로 읽을 수 있는 데이터 크기
	std::size_t GetDirectDequeueSize() const;

	void ClearBuffer() noexcept;
	bool IsFull() const noexcept;

	std::size_t GetBufferSize() const noexcept { return mCapacity; }

	std::size_t GetUseSize() const noexcept;
	std::size_t GetFreeSize() const noexcept { return mCapacity - GetUseSize(); }
	bool IsEmpty() const noexcept { return mReadWindow == mWriteWindow; }

	// Rear 위치에 데이터를 삽입
	bool Enqueue(const char* data, std::size_t size) noexcept;

	// Front 위치에서 데이터를 반환
	bool Dequeue(char* outData, std::size_t size) noexcept;

	// Front 위치에서 데이터를 읽기
	bool Peek(char* outData, std::size_t size) noexcept;

	// Front 포인터를 앞으로 이동시켜 데이터를 소비(제거)한다.
	bool MoveFront(std::size_t moveSize) noexcept;
	char* GetFront() const noexcept;

	// Rear 포인터를 앞으로 이동시켜 "쓰기 완료" 영역을 확장한다.
	bool MoveRear(std::size_t moveSize) noexcept;
	char* GetRear() const noexcept;

private:
	// 복사&이동 금지
	RingBuffer(RingBuffer&&) = delete;
	RingBuffer& operator=(RingBuffer&&) = delete;
	RingBuffer(const RingBuffer&) = delete;
	RingBuffer& operator=(const RingBuffer&) = delete;

	// 포인터 멤버 변수 이동에 사용
	void MovePointer(char*& pointer, std::size_t addValue);

	// Dequeue, Peek 구현
	bool ReadInternal(char* outData, std::size_t size, bool isPeekMode) noexcept;

private:
	char* mBuffer;   // 실제 버퍼 시작 주소
	char* mEnd;     // 마지막 유효 칸(= &mBuffer[mCapacity])
	char* mReadWindow;    // 현재 읽기 시작 위치
	char* mWriteWindow;   // 현재 쓰기 시작 위치
	std::size_t mCapacity; // 전체 버퍼 크기(논리 capacity = mCapacity)
};
