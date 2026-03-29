#include <process.h>
#include <stdio.h>
#include <windows.h>

#include "LFStack.h"

constexpr int THREAD_COUNT = 2;

LFStack<UINT64, TRUE> lockfreeStack;

unsigned int __stdcall ThreadFunc(void* lpParam)
{
    UNREFERENCED_PARAMETER(lpParam);

    while (1)
    {
        for (UINT64 i = 0; i < 1000; ++i)
        {
            lockfreeStack.Push(i);
        }

        for (int i = 0; i < 1000; ++i)
        {
            UINT64 data = 0;
            lockfreeStack.Pop(&data);
        }
    }

    return 0;
}

int main()
{
    HANDLE arrTh[THREAD_COUNT] = {};

    for (int i = 0; i < THREAD_COUNT; ++i)
    {
        arrTh[i] = reinterpret_cast<HANDLE>(
            _beginthreadex(nullptr, 0, ThreadFunc, nullptr, CREATE_SUSPENDED, nullptr));

        if (arrTh[i] == nullptr)
        {
            return 1;
        }
    }

    for (int i = 0; i < THREAD_COUNT; ++i)
    {
        ResumeThread(arrTh[i]);
    }

    WaitForMultipleObjects(THREAD_COUNT, arrTh, TRUE, INFINITE);

    printf("normal exit\n");

    if (lockfreeStack.GetTop() != 0)
    {
        __debugbreak();
    }

    return 0;
}
