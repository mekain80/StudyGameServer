#include "RingBuffer.h"
#include <iostream>
#include <cstring>
#include <cassert>


#include <windows.h>
#include <process.h>

#include <list>
#include <string>
#include <vector>
#include <random>
#include <queue>


#include "Profiler.h"
#undef min
#undef max

using namespace std;

const int SEED = 1999;
const int MESSAGE_MAX_SIZE = 100;
const int MESSAGE_MIN_SIZE = 5;
const int MESSAGE_SIZE = 81;

char message[MESSAGE_SIZE + 1] = "1234567890 abcdefghijklmnopqrstuvwxyz 1234567890 abcdefghijklmnopqrstuvwxyz 12345";

RingBuffer* ringBuffer = new RingBuffer(10000);
queue<int>* que = new queue<int>;

HANDLE g_ExitEvent = nullptr;
DWORD g_startTime;

unsigned int WINAPI EnqueueThread(LPVOID lpParam)
{
    int start = 0;

    while (1)
    {
        // 'Q' 눌림, 종료, 테스트 1분 동안 진행
        if (GetAsyncKeyState('Q') & 1 || g_startTime + 6'0000 < GetTickCount())
        {
            SetEvent(g_ExitEvent);
            return 0;
        }


        int size = rand() % (MESSAGE_SIZE + 1);

        int freeSize = ringBuffer->GetFreeSize();
        if (freeSize < size)
        {
            SetEvent(g_ExitEvent);
            return 0;
            __debugbreak();
        }
            

        ProfileBegin(L"Noexcept EnqueueThread");
        if (start + size > MESSAGE_SIZE)
        {
            ringBuffer->Enqueue(message + start, MESSAGE_SIZE - start);
            ringBuffer->Enqueue(message, size - MESSAGE_SIZE + start);

            start += size - MESSAGE_SIZE;
        }
        else
        {
            ringBuffer->Enqueue(message + start, size);

            start += size;
        }
        ProfileEnd(L"Noexcept EnqueueThread");

        Sleep(1);
    }

    return 0;
}

unsigned int WINAPI DequeueThread(LPVOID lpParam)
{
    char* dequeueString = new char[RING_DEFAULT_SIZE + 1];
    while (1)
    {
        DWORD exitResult = WaitForSingleObject(g_ExitEvent, 0);
        if (exitResult == WAIT_OBJECT_0)
            return 0;


        int useSize = ringBuffer->GetUseSize();
        if (useSize > 0)
        {
            ProfileBegin(L"Noexcept DequeueThread");
            dequeueString[useSize] = '\0';

            ringBuffer->Dequeue(&dequeueString[0], useSize);

            printf("%s", dequeueString);
            ProfileEnd(L"Noexcept DequeueThread");
        }
    }

    return 0;
}

void ThreadTest()
{
    srand(SEED);

    HANDLE Threads[3];
    Threads[0] = (HANDLE)_beginthreadex(nullptr, 0, EnqueueThread, nullptr, 0, nullptr);
    Threads[1] = (HANDLE)_beginthreadex(nullptr, 0, DequeueThread, nullptr, 0, nullptr);

    WaitForMultipleObjects(2, Threads, true, INFINITE);
}

static void PrintState(const RingBuffer& rb, const char* tag, const char* base)
{
    const char* front = rb.GetFront();
    const char* rear = rb.GetRear();

    ptrdiff_t frontIdx = (front - base);
    ptrdiff_t rearIdx = (rear - base);

    cout << "---- " << tag << " ----\n";
    cout << "  size=" << rb.GetBufferSize()
        << " used=" << rb.GetUseSize()
        << " free=" << rb.GetFreeSize() << "\n";
    cout << "  empty=" << rb.IsEmpty()
        << " full=" << rb.IsFull() << "\n";
    cout << "  directEnq=" << rb.GetDirectEnqueueSize()
        << " directDeq=" << rb.GetDirectDequeueSize() << "\n";
    cout << "  frontIdx=" << frontIdx << " rearIdx=" << rearIdx << "\n";
    cout << "----------------------\n";
}

void Test()
{
    RingBuffer rb(8);

    // base = 버퍼 시작(construct 직후 front==rear==start 라는 전제)
    const char* base = rb.GetFront();

    PrintState(rb, "After Construct", base);
    assert(rb.IsEmpty());
    assert(!rb.IsFull());
    assert(rb.GetUseSize() == 0);
    assert(rb.GetFreeSize() == rb.GetBufferSize());

    // 1) Enqueue: "ABCD"
    {
        const char* data = "ABCD";
        bool ok = rb.Enqueue(data, 4);
        cout << "[Test1] Enqueue(\"ABCD\",4) ok=" << ok << "\n";
        assert(ok);
        assert(rb.GetUseSize() == 4);
        PrintState(rb, "After Enqueue ABCD", base);
    }

    // 2) Peek 4: "ABCD" (소모 X)
    {
        char buf[16] = {};
        bool ok = rb.Peek(buf, 4);
        cout << "[Test2] Peek(4) ok=" << ok << " data=" << string(buf, buf + 4) << "\n";
        assert(ok);
        assert(buf[0] == 'A' && buf[1] == 'B' && buf[2] == 'C' && buf[3] == 'D');
        assert(rb.GetUseSize() == 4); // Peek이니 그대로
        PrintState(rb, "After Peek 4 (no consume)", base);
    }

    // 3) Dequeue 2: "AB"
    {
        char buf[16] = {};
        bool ok = rb.Dequeue(buf, 2);
        cout << "[Test3] Dequeue(2) ok=" << ok << " data=" << string(buf, buf + 2) << "\n";
        assert(ok);
        assert(buf[0] == 'A' && buf[1] == 'B');
        assert(rb.GetUseSize() == 2); // "CD" 남음
        PrintState(rb, "After Dequeue 2 (expect CD left)", base);
    }

    // 4) Enqueue 6: "EFGHIJ" (wrap + full 만들기)
    // 남은 "CD"(2) + "EFGHIJ"(6) = 8 (full)
    {
        const char* data = "EFGHIJ";
        bool ok = rb.Enqueue(data, 6);
        cout << "[Test4] Enqueue(\"EFGHIJ\",6) ok=" << ok << "\n";
        assert(ok);
        assert(rb.GetUseSize() == 8);
        assert(rb.IsFull());
        PrintState(rb, "After Enqueue EFGHIJ (expect full, wrap)", base);
    }

    // 5) Full 상태에서 더 Enqueue 시도 -> 실패
    {
        const char* data = "Z";
        bool ok = rb.Enqueue(data, 1);
        cout << "[Test5] Enqueue(\"Z\",1) on full ok=" << ok << "\n";
        assert(!ok);
        assert(rb.IsFull());
        PrintState(rb, "After Enqueue fail on full", base);
    }

    // 6) Dequeue 3 -> "CDE"
    {
        char buf[16] = {};
        bool ok = rb.Dequeue(buf, 3);
        cout << "[Test6] Dequeue(3) ok=" << ok << " data=" << string(buf, buf + 3) << "\n";
        assert(ok);
        assert(buf[0] == 'C' && buf[1] == 'D' && buf[2] == 'E');
        assert(rb.GetUseSize() == 5); // "FGHIJ" 남음
        PrintState(rb, "After Dequeue 3 (expect FGHIJ left)", base);
    }

    // 7) DirectEnqueue + MoveRearBuffer: 뒤에 "XYZ" 직접 쓰기 (연속 구간만큼)
    {
        int direct = rb.GetDirectEnqueueSize();
        int freeSz = rb.GetFreeSize();
        int writeSize = std::min({ 3, direct, freeSz });

        cout << "[Test7] directEnq=" << direct << " free=" << freeSz << " writeSize=" << writeSize << "\n";
        assert(writeSize > 0);

        char* rearPtr = rb.GetRear();
        memcpy(rearPtr, "XYZ", writeSize);

        bool moved = rb.MoveRear(writeSize);
        cout << "[Test7] MoveRearBuffer(" << writeSize << ") moved=" << moved << "\n";
        assert(moved);

        PrintState(rb, "After DirectEnqueue XYZ + MoveRearBuffer", base);
    }

    // 8) DirectDequeue + MoveFrontBuffer: 앞 연속 구간만큼 직접 읽고 MoveFrontBuffer로 소모
    {
        int direct = rb.GetDirectDequeueSize();
        int usedSz = rb.GetUseSize();
        int readSize = std::min(direct, usedSz); // direct는 <= used가 보통이지만 방어적으로

        cout << "[Test8] directDeq=" << direct << " used=" << usedSz << " readSize=" << readSize << "\n";
        assert(readSize > 0);

        const char* frontPtr = rb.GetFront();
        cout << "[Test8] Direct front read: ";
        for (int i = 0; i < readSize; ++i) cout << frontPtr[i];
        cout << "\n";

        bool moved = rb.MoveFront(readSize);
        cout << "[Test8] MoveFrontBuffer(" << readSize << ") moved=" << moved << "\n";
        assert(moved);

        PrintState(rb, "After DirectDequeue by MoveFrontBuffer", base);
    }

    // 9) 남은 데이터 전부 Dequeue 해서 내용 검증(최종 정리)
    {
        int left = rb.GetUseSize();
        assert(left >= 0 && left <= rb.GetBufferSize());

        char buf[32] = {};
        bool ok = true;
        if (left > 0)
        {
            ok = rb.Dequeue(buf, left);
            cout << "[Test9] Dequeue(all=" << left << ") ok=" << ok << " data=" << string(buf, buf + left) << "\n";
            assert(ok);
        }

        assert(rb.IsEmpty());
        assert(rb.GetUseSize() == 0);
        PrintState(rb, "After Drain All", base);
    }

    // 10) ClearBuffer 테스트
    {
        rb.ClearBuffer();
        cout << "[Test10] After ClearBuffer()\n";
        assert(rb.GetUseSize() == 0);
        assert(rb.GetFreeSize() == rb.GetBufferSize());
        assert(rb.IsEmpty());
        PrintState(rb, "After ClearBuffer", base);
    }

    cout << "All tests finished (no assert failed).\n";
}

#include <stdint.h>    // uint32_t, uint64_t
#include <assert.h>

// ------------------------------------------------------------
// 테스트 파라미터 (constexpr 같은 거 안 쓰고 그냥 const)
// ------------------------------------------------------------
static const int      kRingSize = 64 * 1024;   // 링버퍼 크기(바이트)
static const uint32_t kTotalMsgs = 2000000;     // 메시지 개수
static const uint32_t kMagic = 0xA5A5A5A5u;

// ------------------------------------------------------------
// 테스트 메시지 포맷(고정 길이)
// ------------------------------------------------------------
struct SpscMsg
{
    uint32_t seq;
    uint32_t checksum;
    uint64_t payload;
};

// checksum/payload 생성 (람다 대신 일반 함수)
static uint32_t CalcChecksum(uint32_t seq)
{
    return (seq ^ kMagic);
}

static uint64_t MakePayload(uint32_t seq)
{
    return ((uint64_t)seq << 32) | (uint64_t)seq;
}

// busy-wait 완화용 backoff (람다 대신 일반 함수)
static void Backoff(uint32_t* spin)
{
    ++(*spin);

    // 256번에 1번 정도 양보
    if (((*spin) & 0xFFu) == 0)
    {
        // 가장 간단한 yield: Sleep(0)
        // (또는 SwitchToThread() 써도 됨)
        Sleep(0);
    }
}

// ------------------------------------------------------------
// Producer/Consumer가 공유하는 상태
// (std::atomic 대신 Interlocked로만 접근)
// ------------------------------------------------------------
struct SpscShared
{
    RingBuffer* rb;

    volatile LONG produced;     // 생산 성공 카운트
    volatile LONG consumed;     // 소비 성공 카운트
    volatile LONG failCount;    // 실패 카운트

    volatile LONG producerDone; // 0 = 진행중, 1 = 종료
};

// ------------------------------------------------------------
// Producer Thread
// ------------------------------------------------------------
static unsigned __stdcall ProducerProc(void* arg)
{
    SpscShared* s = (SpscShared*)arg;
    uint32_t spin = 0;

    for (uint32_t seq = 1; seq <= kTotalMsgs; ++seq)
    {
        SpscMsg msg;
        msg.seq = seq;
        msg.checksum = CalcChecksum(seq);
        msg.payload = MakePayload(seq);

        // msg 전체가 들어갈 때까지 반복
        while (!s->rb->Enqueue((const char*)&msg, (int)sizeof(msg)))
        {
            Backoff(&spin);
        }

        InterlockedIncrement(&s->produced);
    }

    // producer 종료 알림
    InterlockedExchange(&s->producerDone, 1);
    return 0;
}

// ------------------------------------------------------------
// Consumer Thread
// ------------------------------------------------------------
static unsigned __stdcall ConsumerProc(void* arg)
{
    SpscShared* s = (SpscShared*)arg;

    uint32_t expected = 1;
    uint32_t spin = 0;

    while (expected <= kTotalMsgs)
    {
        SpscMsg msg;

        if (!s->rb->Dequeue((char*)&msg, (int)sizeof(msg)))
        {
            // producer가 끝났고 버퍼도 비었는데 expected를 못 채웠다?
            // => 누락/꼬임 가능성
            LONG done = InterlockedCompareExchange(&s->producerDone, 0, 0);
            if (done != 0 && s->rb->IsEmpty())
            {
                InterlockedIncrement(&s->failCount);
                std::cerr << "[SPSC FAIL] producer done + buffer empty, but consumer didn't reach expected="
                    << expected << "\n";
                break;
            }

            Backoff(&spin);
            continue;
        }

        // 검증 1) 순서
        if (msg.seq != expected)
        {
            InterlockedIncrement(&s->failCount);
            std::cerr << "[SPSC FAIL] seq mismatch. expected=" << expected
                << " got=" << msg.seq << "\n";
            break;
        }

        // 검증 2) checksum
        {
            uint32_t chk = CalcChecksum(msg.seq);
            if (msg.checksum != chk)
            {
                InterlockedIncrement(&s->failCount);
                std::cerr << "[SPSC FAIL] checksum mismatch. seq=" << msg.seq
                    << " expectedChk=" << chk
                    << " gotChk=" << msg.checksum << "\n";
                break;
            }
        }

        // 검증 3) payload
        {
            uint64_t payload = MakePayload(msg.seq);
            if (msg.payload != payload)
            {
                InterlockedIncrement(&s->failCount);
                std::cerr << "[SPSC FAIL] payload mismatch. seq=" << msg.seq
                    << " expectedPayload=" << payload
                    << " gotPayload=" << msg.payload << "\n";
                break;
            }
        }

        // 정상 수신
        ++expected;
        InterlockedIncrement(&s->consumed);
    }

    return 0;
}

// ------------------------------------------------------------
// 테스트 함수
// ------------------------------------------------------------
void TestSPSC()
{
    RingBuffer rb(kRingSize);

    SpscShared s;
    s.rb = &rb;
    s.produced = 0;
    s.consumed = 0;
    s.failCount = 0;
    s.producerDone = 0;

    DWORD64 t0 = GetTickCount64();

    // thread 시작 (std::thread 대신 _beginthreadex)
    unsigned tidProd = 0;
    unsigned tidCons = 0;

    HANDLE hProd = (HANDLE)_beginthreadex(
        NULL, 0, &ProducerProc, &s, 0, &tidProd);

    HANDLE hCons = (HANDLE)_beginthreadex(
        NULL, 0, &ConsumerProc, &s, 0, &tidCons);

    assert(hProd != NULL && hCons != NULL);

    WaitForSingleObject(hProd, INFINITE);
    WaitForSingleObject(hCons, INFINITE);

    CloseHandle(hProd);
    CloseHandle(hCons);

    DWORD64 t1 = GetTickCount64();
    double sec = (double)(t1 - t0) / 1000.0;

    // 이제 스레드가 끝났으니 그냥 읽어도 OK
    LONG produced = s.produced;
    LONG consumed = s.consumed;
    LONG fails = s.failCount;

    std::cout << "SPSC test finished.\n";
    std::cout << "  produced=" << produced << "\n";
    std::cout << "  consumed=" << consumed << "\n";
    std::cout << "  fails   =" << fails << "\n";
    std::cout << "  seconds =" << sec << "\n";
    if (sec > 0.0)
    {
        std::cout << "  msg/s   =" << (uint64_t)((double)consumed / sec) << "\n";
    }

    // 통과 조건
    assert(fails == 0);
    assert((uint32_t)produced == kTotalMsgs);
    assert((uint32_t)consumed == kTotalMsgs);
    assert(rb.IsEmpty());
}

#include <atomic>
#include <chrono>
#include <cstdint>
#include <thread>

static bool TryEnqueueZeroCopy(RingBuffer& rb, const void* src, int bytes)
{
    if (bytes <= 0) return false;

    // 전체 free가 부족하면 실패
    if (rb.GetFreeSize() < bytes)
        return false;

    const int direct = rb.GetDirectEnqueueSize();
    char* rear = rb.GetRear();

    if (direct >= bytes)
    {
        // 한 덩어리로 쓸 수 있음
        std::memcpy(rear, src, bytes);
        return rb.MoveRear(bytes);
    }
    else
    {
        // wrap 필요: [rear..end] + [start..)
        const int first = direct;
        const int second = bytes - first;

        if (first <= 0)
            return false; // 이 경우는 보통 free<bytes인 상황이 많음(안전장치)

        std::memcpy(rear, src, first);

        // 1차 커밋(여기서 rear가 wrap되어 start로 갈 수 있음)
        if (!rb.MoveRear(first))
            return false;

        char* rear2 = rb.GetRear(); // 보통 start 쪽을 가리킴
        std::memcpy(rear2, (const char*)src + first, second);

        // 2차 커밋
        return rb.MoveRear(second);
    }
}

static bool TryDequeueZeroCopy(RingBuffer& rb, void* dst, int bytes)
{
    if (bytes <= 0) return false;

    // 전체 used가 부족하면 실패
    if (rb.GetUseSize() < bytes)
        return false;

    const int direct = rb.GetDirectDequeueSize();
    char* front = rb.GetFront();

    if (direct >= bytes)
    {
        // 한 덩어리로 읽을 수 있음
        std::memcpy(dst, front, bytes);
        return rb.MoveFront(bytes);
    }
    else
    {
        // wrap 필요: [front..end] + [start..)
        const int first = direct;
        const int second = bytes - first;

        if (first <= 0)
            return false; // 안전장치

        std::memcpy(dst, front, first);

        // 1차 소비 커밋(여기서 front가 wrap되어 start로 갈 수 있음)
        if (!rb.MoveFront(first))
            return false;

        char* front2 = rb.GetFront(); // 보통 start 쪽을 가리킴
        std::memcpy((char*)dst + first, front2, second);

        // 2차 소비 커밋
        return rb.MoveFront(second);
    }
}

void TestSPSC_ZeroCopy()
{
    // ============================================================
    // SPSC Zero-Copy 스트레스 테스트
    //
    // - Producer: GetRear() 버퍼에 직접 memcpy 후 MoveRear()로 커밋
    // - Consumer: GetFront() 버퍼를 직접 memcpy로 읽고 MoveFront()로 소비
    //
    // 검증:
    //  1) seq 순서 보장(1..N)
    //  2) checksum/payload 무결성
    // ============================================================

    constexpr int kRingSize = 64 * 1024;
    constexpr uint32_t kTotalMsgs = 2'000'000;

    struct SpscMsg
    {
        uint32_t seq;
        uint32_t checksum;
        uint64_t payload;
    };

    constexpr uint32_t kMagic = 0xA5A5A5A5u;

    auto CalcChecksum = [&](uint32_t seq) -> uint32_t
        {
            return seq ^ kMagic;
        };

    auto MakePayload = [&](uint32_t seq) -> uint64_t
        {
            return (static_cast<uint64_t>(seq) << 32) | seq;
        };

    auto Backoff = [&](uint32_t& spin)
        {
            ++spin;
            if ((spin & 0xFFu) == 0)
            {
                std::this_thread::yield();
            }
        };

    RingBuffer rb(kRingSize);

    std::atomic<uint32_t> produced{ 0 };
    std::atomic<uint32_t> consumed{ 0 };
    std::atomic<uint32_t> failCount{ 0 };
    std::atomic<bool> producerDone{ false };

    const auto t0 = std::chrono::steady_clock::now();

    std::thread producer([&]()
        {
            uint32_t spin = 0;
            const int msgSize = static_cast<int>(sizeof(SpscMsg));

            for (uint32_t seq = 1; seq <= kTotalMsgs; ++seq)
            {
                SpscMsg msg{};
                msg.seq = seq;
                msg.checksum = CalcChecksum(seq);
                msg.payload = MakePayload(seq);

                // 0-copy 경로: rear에 직접 쓰고 MoveRear로 커밋
                while (!TryEnqueueZeroCopy(rb, &msg, msgSize))
                {
                    Backoff(spin);
                }

                produced.fetch_add(1, std::memory_order_relaxed);
            }

            producerDone.store(true, std::memory_order_release);
        });

    std::thread consumer([&]()
        {
            uint32_t expected = 1;
            uint32_t spin = 0;
            const int msgSize = static_cast<int>(sizeof(SpscMsg));

            while (expected <= kTotalMsgs)
            {
                SpscMsg msg{};

                // 0-copy 경로: front에서 직접 읽고 MoveFront로 소비
                if (!TryDequeueZeroCopy(rb, &msg, msgSize))
                {
                    if (producerDone.load(std::memory_order_acquire))
                    {
                        const int used = rb.GetUseSize();
                        if (used == 0)
                        {
                            // 끝났고 버퍼도 비었는데 expected 못 채움 => 누락/꼬임
                            failCount.fetch_add(1, std::memory_order_relaxed);
                            std::cerr << "[SPSC FAIL] producer done + buffer empty, but consumer didn't reach expected="
                                << expected << "\n";
                            break;
                        }
                        else if (used > 0 && used < msgSize)
                        {
                            // producer 끝났는데 msgSize 미만 찌꺼기 => 메시지 경계 깨짐(심각)
                            failCount.fetch_add(1, std::memory_order_relaxed);
                            std::cerr << "[SPSC FAIL] producer done but leftover bytes < msgSize. used="
                                << used << " expected=" << expected << "\n";
                            break;
                        }
                    }

                    Backoff(spin);
                    continue;
                }

                // 검증 1) 순서
                if (msg.seq != expected)
                {
                    failCount.fetch_add(1, std::memory_order_relaxed);
                    std::cerr << "[SPSC FAIL] seq mismatch. expected=" << expected
                        << " got=" << msg.seq << "\n";
                    break;
                }

                // 검증 2) checksum
                const uint32_t expectedChk = CalcChecksum(msg.seq);
                if (msg.checksum != expectedChk)
                {
                    failCount.fetch_add(1, std::memory_order_relaxed);
                    std::cerr << "[SPSC FAIL] checksum mismatch. seq=" << msg.seq
                        << " expectedChk=" << expectedChk
                        << " gotChk=" << msg.checksum << "\n";
                    break;
                }

                // 검증 3) payload
                const uint64_t expectedPayload = MakePayload(msg.seq);
                if (msg.payload != expectedPayload)
                {
                    failCount.fetch_add(1, std::memory_order_relaxed);
                    std::cerr << "[SPSC FAIL] payload mismatch. seq=" << msg.seq
                        << " expectedPayload=" << expectedPayload
                        << " gotPayload=" << msg.payload << "\n";
                    break;
                }

                ++expected;
                consumed.fetch_add(1, std::memory_order_relaxed);
            }
        });

    producer.join();
    consumer.join();

    const auto t1 = std::chrono::steady_clock::now();
    const double sec = std::chrono::duration<double>(t1 - t0).count();

    std::cout << "SPSC ZeroCopy test finished.\n";
    std::cout << "  produced=" << produced.load() << "\n";
    std::cout << "  consumed=" << consumed.load() << "\n";
    std::cout << "  fails   =" << failCount.load() << "\n";
    std::cout << "  seconds =" << sec << "\n";
    if (sec > 0.0)
    {
        std::cout << "  msg/s   =" << static_cast<uint64_t>(consumed.load() / sec) << "\n";
    }

    // 통과 조건
    assert(failCount.load() == 0);
    assert(produced.load() == kTotalMsgs);
    assert(consumed.load() == kTotalMsgs);
    assert(rb.IsEmpty());
}



int main()
{
    g_ExitEvent = CreateEvent(NULL, true, false, NULL);
    g_startTime = GetTickCount();


    //Test();
    //ThreadTest();
    //TestSPSC();
    TestSPSC_ZeroCopy();

    ProfileDataOutText(L"Noexcept.txt");
    return 0;
}
