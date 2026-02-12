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



int main()
{
    g_ExitEvent = CreateEvent(NULL, true, false, NULL);
    g_startTime = GetTickCount();

    Test();
    //ThreadTest();

    ProfileDataOutText(L"Noexcept.txt");
    return 0;
}
