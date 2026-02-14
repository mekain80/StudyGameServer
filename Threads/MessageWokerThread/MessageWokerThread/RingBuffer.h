#pragma once

static const int RING_DEFAULT_SIZE = 10000;

/// @brief 고성능 송수신용 Ring Buffer 클래스, 부분 Enqueue/Dequeue 미지원(요청 크기 미만이면 실패).
/// @brief 내부 버퍼는 (mBufferSize + 1)로 할당하며, Full/Empty 구분을 위해 항상 1칸을 비움.
class RingBuffer
{
public:
	/// @brief 기본 생성자.
	RingBuffer(void);
	explicit RingBuffer(int bufferSize);
	~RingBuffer(void) noexcept;

	/// @brief 끊기지 않고 한 번에 Enqueue 가능한 연속 공간 크기를 반환
	/// @return 연속적으로 쓸 수 있는 공간 크기
	int GetDirectEnqueueSize() const;

	/// @brief 끊기지 않고 한 번에 Dequeue 가능한 연속 데이터 크기를 반환
	/// @return 연속적으로 읽을 수 있는 데이터 크기
	int GetDirectDequeueSize() const;

	void ClearBuffer(void) noexcept;
	bool IsFull() const noexcept;

	int GetBufferSize(void) const noexcept { return mBufferSize; }

	int GetUseSize(void) const noexcept;
	int GetFreeSize(void) const noexcept { return mBufferSize - GetUseSize(); }
	bool IsEmpty() const noexcept { return mFront == mRear; }

	/// @brief Rear 위치에 데이터를 삽입
	bool Enqueue(const char* data, int size) noexcept;

	/// @brief Front 위치에서 데이터를 반환
	bool Dequeue(char* outData, int size) noexcept;

	/// @brief Front 위치에서 데이터를 읽기
	bool Peek(char* outData, int size) noexcept;

	/// @brief Front 포인터를 앞으로 이동시켜 데이터를 소비(제거)한다.
	bool MoveFront(int moveSize) noexcept;
	char* GetFront() const noexcept;

	/// @brief Rear 포인터를 앞으로 이동시켜 “쓰기 완료” 영역을 확장한다.
	bool MoveRear(int moveSize) noexcept;
	char* GetRear() const noexcept;

private:
	// 복사&이동 금지
	RingBuffer(RingBuffer&&) = delete;
	RingBuffer& operator=(RingBuffer&&) = delete;
	RingBuffer(const RingBuffer&) = delete;
	RingBuffer& operator=(const RingBuffer&) = delete;

	/// @brief 포인터 멤버 변수 이동에 사용
	void MovePointer(char*& pointer, int addValue);

	/// @brief Dequeue, Peek 구현
	bool ReadInternal(char* outData, int size, bool isPeekMode) noexcept;

private:
	char* mBuffer;      // 실제 버퍼 시작 주소
	char* mEnd;			// 마지막 유효 칸(= &mBuffer[mBufferSize])
	char* mFront;       // Front 포인터
	char* mRear;        // Rear 포인터
	int   mBufferSize;  // 전체 버퍼 크기(논리 capacity = mBufferSize)
};
