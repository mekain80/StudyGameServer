#pragma once
#pragma once

static const int DEFAULT_SIZE = 10000;

/// <summary>
/// 고성능 송수신용 Ring Buffer 클래스
/// </summary>
class RingBuffer
{
public:
    /// <summary>
    /// 기본 생성자.
    /// </summary>
    RingBuffer(void);

    /// <summary>
    /// 버퍼 크기를 지정하는 생성자.
    /// </summary>
    /// <param name="iBufferSize">버퍼 초기 크기</param>
    RingBuffer(int iBufferSize);

    /// <summary>
    /// 소멸자.
    /// </summary>
    ~RingBuffer(void);

    /// <summary>
    /// 버퍼의 크기를 재조정한다.
    /// </summary>
    /// <param name="size">새로운 버퍼 크기</param>
    void Resize(int size);

    /// <summary>
    /// 전체 버퍼 크기를 반환한다.
    /// </summary>
    /// <returns>버퍼의 총 크기</returns>
    int GetBufferSize(void);

    /// <summary>
    /// 현재 사용 중인 용량을 반환한다.
    /// </summary>
    /// <returns>사용 중인 데이터 크기</returns>
    int GetUseSize(void);

    /// <summary>
    /// 현재 남은 용량을 반환한다.
    /// </summary>
    /// <returns>사용 가능한 용량</returns>
    int GetFreeSize(void);

    /// <summary>
    /// Rear 위치에 데이터를 삽입한다.
    /// </summary>
    /// <param name="chpData">입력 데이터 포인터</param>
    /// <param name="iSize">입력 데이터 크기</param>
    /// <returns>실제로 삽입된 크기</returns>
    int Enqueue(const char* chpData, int iSize);

    /// <summary>
    /// Front 위치에서 데이터를 가져오고, Front 인덱스를 이동한다.
    /// </summary>
    /// <param name="chpDest">데이터 저장 버퍼</param>
    /// <param name="iSize">읽고자 하는 크기</param>
    /// <returns>가져온 데이터 크기</returns>
    int Dequeue(char* chpDest, int iSize);

    /// <summary>
    /// Front 위치에서 데이터를 읽되, Front 인덱스는 이동하지 않는다.
    /// </summary>
    /// <param name="chpDest">데이터 저장 버퍼</param>
    /// <param name="iSize">읽고자 하는 크기</param>
    /// <returns>읽은 데이터 크기</returns>
    int Peek(char* chpDest, int iSize);

    /// <summary>
    /// 버퍼의 모든 데이터를 삭제하고 초기화한다.
    /// </summary>
    void ClearBuffer(void);

    /// <summary>
    /// 끊기지 않고 한 번에 Enqueue 가능한 연속 공간 크기를 반환한다.
    /// </summary>
    /// <returns>연속적으로 쓸 수 있는 공간 크기</returns>
    int DirectEnqueueSize(void);

    /// <summary>
    /// 끊기지 않고 한 번에 Dequeue 가능한 연속 데이터 크기를 반환한다.
    /// </summary>
    /// <returns>연속적으로 읽을 수 있는 데이터 크기</returns>
    int DirectDequeueSize(void);

    /// <summary>
    /// Rear 위치를 지정한 크기만큼 이동시킨다. (쓰기 위치 이동)
    /// </summary>
    /// <param name="iSize">이동할 크기</param>
    /// <returns>이동된 크기</returns>
    int MoveRear(int iSize);

    /// <summary>
    /// Front 위치를 지정한 크기만큼 이동시킨다. (읽기 위치 이동)
    /// </summary>
    /// <param name="iSize">이동할 크기</param>
    /// <returns>이동된 크기</returns>
    int MoveFront(int iSize);

    /// <summary>
    /// Front 위치의 버퍼 포인터를 반환한다.
    /// </summary>
    /// <returns>Front 위치의 char* 포인터</returns>
    char* GetFrontBufferPtr(void);

    /// <summary>
    /// Rear 위치의 버퍼 포인터를 반환한다.
    /// </summary>
    /// <returns>Rear 위치의 char* 포인터</returns>
    char* GetRearBufferPtr(void);

private:
    // 복사 금지
    RingBuffer(const RingBuffer&) = delete;
    RingBuffer& operator=(const RingBuffer&) = delete;

private:
    char* mBuffer;      /// 실제 버퍼 시작 주소
    int   mBufferSize;  /// 전체 버퍼 크기
    int   mFront;       /// Front 인덱스
    int   mRear;        /// Rear 인덱스
    bool  mIsEmpty;     /// front == rear 상태 구분용
};
