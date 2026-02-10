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

void PrintState(RingBuffer& rb, const char* title)
{
    cout << "==== " << title << " ====\n";
    cout << "BufferSize : " << rb.GetBufferSize() << "\n";
    cout << "UseSize    : " << rb.GetUseSize() << "\n";
    cout << "FreeSize   : " << rb.GetFreeSize() << "\n";

    int useSize = rb.GetUseSize();
    if (useSize > 0)
    {
        char temp[1024] = { 0 };
        int peekSize = (useSize < (int)sizeof(temp) - 1) ? useSize : (int)sizeof(temp) - 1;
        int ret = rb.Peek(temp, peekSize);
        cout << "Peek(" << ret << ") : ";
        for (int i = 0; i < ret; ++i)
        {
            cout << temp[i];
        }
        cout << "\n";
    }
    else
    {
        cout << "Peek       : (empty)\n";
    }

    cout << "DirectEnqueueSize : " << rb.DirectEnqueueSize() << "\n";
    cout << "DirectDequeueSize : " << rb.DirectDequeueSize() << "\n";
    cout << endl;
}

void Test()
{
    // 테스트용으로 사이즈 8짜리 링버퍼
    RingBuffer rb(8);

    cout << boolalpha;

    PrintState(rb, "After Construct");

    // 1) 기본 Enqueue / Peek 테스트: "ABCD"
    {
        const char* data = "ABCD";
        int ret = rb.Enqueue(data, 4);
        cout << "[Test1] Enqueue(\"ABCD\", 4) ret = " << ret << "\n";
        assert(ret == 4);
        PrintState(rb, "After Enqueue ABCD");
    }

    // 2) Dequeue 2바이트: "AB"
    {
        char buf[8] = { 0 };
        int ret = rb.Dequeue(buf, 2);
        cout << "[Test2] Dequeue(2) ret = " << ret << ", data = ";
        for (int i = 0; i < ret; ++i) cout << buf[i];
        cout << "\n";
        assert(ret == 2);
        assert(buf[0] == 'A' && buf[1] == 'B');
        PrintState(rb, "After Dequeue 2 (expect CD left)");
    }

    // 3) 다시 Enqueue 해서 래핑(full 상태 만들기): "EFGH"
    {
        const char* data = "EFGH";
        int ret = rb.Enqueue(data, 4);
        cout << "[Test3] Enqueue(\"EFGH\", 4) ret = " << ret << "\n";
        assert(ret == 4);
        PrintState(rb, "After Enqueue EFGH (expect full: CDEFGH)");
    }

    // 4) 가득 찬 상태에서 더 Enqueue 시도 → 실패(0)
    {
        const char* data = "ZZ";
        int ret = rb.Enqueue(data, 2);
        cout << "[Test4] Enqueue(\"ZZ\", 2) on full buffer ret = " << ret << "\n";
        PrintState(rb, "After Enqueue fail on full");
    }

    // 5) 래핑 Dequeue 테스트: 3바이트 꺼내기
    {
        char buf[8] = { 0 };
        int ret = rb.Dequeue(buf, 3);
        cout << "[Test5] Dequeue(3) ret = " << ret << ", data = ";
        for (int i = 0; i < ret; ++i) cout << buf[i];
        cout << "\n";
        // 여기서 나오는 값은 구현에 따라 "CDE"가 되어야 정상
        PrintState(rb, "After Dequeue 3");
    }

    // 6) DirectEnqueueSize / MoveRear 사용 테스트
    {
        int directSize = rb.DirectEnqueueSize();
        cout << "[Test6] DirectEnqueueSize = " << directSize << "\n";

        if (directSize > 0)
        {
            char* rearPtr = rb.GetRearBufferPtr();
            int writeSize = (directSize >= 3) ? 3 : directSize;
            memcpy(rearPtr, "XYZ", writeSize);
            int moved = rb.MoveRear(writeSize);
            cout << "[Test6] Direct write \"XYZ\" (" << writeSize << "), MoveRear ret = " << moved << "\n";
            PrintState(rb, "After DirectEnqueue XYZ");
        }
    }

    // 7) DirectDequeueSize / MoveFront 사용 테스트
    {
        int directSize = rb.DirectDequeueSize();
        cout << "[Test7] DirectDequeueSize = " << directSize << "\n";

        if (directSize > 0)
        {
            char* frontPtr = rb.GetFrontBufferPtr();
            cout << "[Test7] Direct read front region (up to " << directSize << " bytes): ";
            for (int i = 0; i < directSize; ++i)
                cout << frontPtr[i];
            cout << "\n";

            int moved = rb.MoveFront(directSize);
            cout << "[Test7] MoveFront(" << directSize << ") ret = " << moved << "\n";
            PrintState(rb, "After DirectDequeue by MoveFront");
        }
    }

    // 8) ClearBuffer 테스트
    {
        rb.ClearBuffer();
        cout << "[Test8] After ClearBuffer()\n";
        PrintState(rb, "After ClearBuffer");
        assert(rb.GetUseSize() == 0);
        assert(rb.GetFreeSize() == rb.GetBufferSize());
    }

    cout << "All tests finished (no assert failed).\n";
}


int main()
{
    g_ExitEvent = CreateEvent(NULL, true, false, NULL);
    g_startTime = GetTickCount();

    //Test();
    ThreadTest();

    ProfileDataOutText(L"Noexcept.txt");
    return 0;
}
