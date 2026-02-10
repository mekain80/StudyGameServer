#pragma once

static const int RING_DEFAULT_SIZE = 10000;

/// @brief 고성능 송수신용 Ring Buffer 클래스
class RingBuffer
{
public:
    /// @brief 기본 생성자.
    RingBuffer(void);

    /// @brief 버퍼 크기를 지정하는 생성자.
    /// @param bufferSize 버퍼 초기 크기
    RingBuffer(int bufferSize);

    /// @brief 소멸자.
    ~RingBuffer(void);

    /// @brief 버퍼의 크기를 재조정한다.
    /// @param size 새로운 버퍼 크기
    void Resize(int size);

    /// @brief 전체 버퍼 크기를 반환한다.
    /// @return 버퍼의 총 크기
    int GetBufferSize(void);

    /// @brief 현재 사용 중인 용량을 반환한다.
    /// @return 사용 중인 데이터 크기
    int GetUseSize(void);

    /// @brief 현재 남은 용량을 반환한다.
    /// @return 사용 가능한 용량
    int GetFreeSize(void);

    /// @brief Rear 위치에 데이터를 삽입한다.
    /// @param data 입력 데이터 포인터
    /// @param size 입력 데이터 크기
    /// @return 실제로 삽입된 크기
    int Enqueue(const char* data, int size);

    /// @brief Front 위치에서 데이터를 가져오고, Front 인덱스를 이동한다.
    /// @param dest 데이터 저장 버퍼
    /// @param size 읽고자 하는 크기
    /// @return 가져온 데이터 크기
    int Dequeue(char* dest, int size);

    /// @brief Front 위치에서 데이터를 읽되, Front 인덱스는 이동하지 않는다.
    /// @param dest 데이터 저장 버퍼
    /// @param size 읽고자 하는 크기
    /// @return 읽은 데이터 크기
    int Peek(char* dest, int size);

    /// @brief 버퍼의 모든 데이터를 삭제하고 초기화한다.
    void ClearBuffer(void);

    /// @brief 끊기지 않고 한 번에 Enqueue 가능한 연속 공간 크기를 반환한다.
    /// @return 연속적으로 쓸 수 있는 공간 크기
    int DirectEnqueueSize(void);

    /// @brief 끊기지 않고 한 번에 Dequeue 가능한 연속 데이터 크기를 반환한다.
    /// @return 연속적으로 읽을 수 있는 데이터 크기
    int DirectDequeueSize(void);

    /// @brief Rear 위치를 지정한 크기만큼 이동시킨다. (쓰기 위치 이동)
    /// @param size 이동할 크기
    /// @return 이동된 크기
    int MoveRear(int size);

    /// @brief Front 위치를 지정한 크기만큼 이동시킨다. (읽기 위치 이동)
    /// @param size 이동할 크기
    /// @return 이동된 크기
    int MoveFront(int size);

    /// @brief Front 위치의 버퍼 포인터를 반환한다.
    /// @return Front 위치의 char* 포인터
    char* GetFrontBufferPtr(void);

    /// @brief Rear 위치의 버퍼 포인터를 반환한다.
    /// @return Rear 위치의 char* 포인터
    char* GetRearBufferPtr(void);

private:
    // 복사 금지
    RingBuffer(const RingBuffer&) = delete;
    RingBuffer& operator=(const RingBuffer&) = delete;

private:
    char* mBuffer;      // 실제 버퍼 시작 주소
    int   mBufferSize;  // 전체 버퍼 크기
    int   mFront;       // Front 인덱스
    int   mRear;        // Rear 인덱스
};
