#include <iostream>
#include <process.h>
#include <windows.h>
#include <vector>
#include <atomic>

#include "ConcurrentStack.h"

std::vector<char[128]> debugging(100'0000);
std::atomic<int> debugCnt = 0;

namespace
{
    enum class TestCase
    {
        AbaSmallLoop,
        UafPopOnly
    };

    constexpr TestCase kSelectedTestCase = TestCase::AbaSmallLoop;
    constexpr int kThreadCount = 2;
    constexpr int kAbaLoopCount = 2;
    constexpr int kUafPrefillCount = 10000;

    ConcurrentStack<int> g_Stack;

    unsigned __stdcall AbaWorker(void* arg)
    {
        UNREFERENCED_PARAMETER(arg);

        while (1)
        {
            for (int i = 0; i < kAbaLoopCount; ++i)
            {
                int data = 0;
                g_Stack.pop(&data);
            }

            for (int i = 0; i < kAbaLoopCount; ++i)
            {
                int data = i;
                g_Stack.push(data);
            }
        }

        return 0;
    }

    unsigned __stdcall PopOnlyWorker(void* arg)
    {
        UNREFERENCED_PARAMETER(arg);

        while (1)
        {
            int data = 0;
            g_Stack.pop(&data);
        }

        return 0;
    }

    int StartWorkers(unsigned(__stdcall* worker)(void*))
    {
        HANDLE threadHandles[kThreadCount] = {};

        for (int i = 0; i < kThreadCount; ++i)
        {
            uintptr_t handle = _beginthreadex(nullptr, 0, worker, nullptr, 0, nullptr);

            if (handle == 0)
            {
                std::cerr << "_beginthreadex failed: " << GetLastError() << '\n';
                return 1;
            }

            threadHandles[i] = reinterpret_cast<HANDLE>(handle);
        }

        WaitForMultipleObjects(kThreadCount, threadHandles, TRUE, INFINITE);

        for (HANDLE handle : threadHandles)
        {
            CloseHandle(handle);
        }

        return 0;
    }

    int RunAbaSmallLoopTc()
    {
        std::cout << "[TC] ABA small-loop test\n";
        std::cout << "thread count: " << kThreadCount << ", push/pop count: " << kAbaLoopCount << '\n';

        g_Stack.push(100);
        g_Stack.push(200);
        g_Stack.push(300);

        return StartWorkers(&AbaWorker);
    }

    int RunUafPopOnlyTc()
    {
        std::cout << "[TC] UAF pop-only test\n";
        std::cout << "prefill count: " << kUafPrefillCount << ", thread count: " << kThreadCount << '\n';

        for (int i = 0; i < kUafPrefillCount; ++i)
        {
            g_Stack.push(i);
        }

        return StartWorkers(&PopOnlyWorker);
    }
}

int main()
{
    switch (kSelectedTestCase)
    {
    case TestCase::AbaSmallLoop:
        return RunAbaSmallLoopTc();

    case TestCase::UafPopOnly:
        return RunUafPopOnlyTc();
    }

    return 0;
}
