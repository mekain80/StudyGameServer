#include <iostream>
#include <string>
#include <list>
#include <cstdio>
#include <cstdlib>
#include <Windows.h>
#include <process.h>
#include <cstdint>
#include <vector>

using namespace std;

const int listCnt = 10'000;
list<int> gList(0); // 원하면 list<int> gList; 로 "빈 리스트" 시작 가능

CRITICAL_SECTION gListCS;
HANDLE exitEvt = nullptr;
HANDLE saveEvt = nullptr;

HANDLE threadHandleArr[10]{};
int handleCnt = 0;

unsigned __stdcall MainThread(void* /*lpParam*/)
{
    while (true)
    {
        // 'Z' 눌림(transition) 감지: 한번 누를 때 1회만 트리거
        if (GetAsyncKeyState('Z') & 1)
        {
            SetEvent(saveEvt);
        }

        // 'X' 눌림: 종료 브로드캐스트 + 나머지 스레드 join
        if (GetAsyncKeyState('X') & 1)
        {
            SetEvent(exitEvt);

            // SaveThread가 saveEvt에서 대기 중이면 깨어나게(선택)
            SetEvent(saveEvt);

            // 나 자신(0번)을 제외한 나머지 스레드가 종료될 때까지 대기
            if (handleCnt > 1)
                WaitForMultipleObjects(static_cast<DWORD>(handleCnt - 1), threadHandleArr + 1, TRUE, INFINITE);

            break;
        }
    }

    return 0;
}

unsigned __stdcall PrintThread(void* /*lpParam*/)
{
    while (true)
    {
        // 1초 주기: exitEvt가 오면 즉시 종료, 아니면 1초마다 작업
        DWORD r = WaitForSingleObject(exitEvt, 1000);

        if (r == WAIT_OBJECT_0)
            break;

        if (r == WAIT_TIMEOUT)
        {
            bool first = true;

            EnterCriticalSection(&gListCS);
            vector<int> snap;
            {
                snap.reserve(gList.size());
                for (int v : gList)
                    snap.push_back(v);
            }
            LeaveCriticalSection(&gListCS);

            for (auto it = snap.begin(); it != snap.end(); ++it)
            {
                if (!first) cout << "-";
                first = false;
                cout << *it;
            }
            cout << "\n";
        }
    }
    return 0;
}

unsigned __stdcall DeleteThread(void* /*lpParam*/)
{
    while (true)
    {
        DWORD r = WaitForSingleObject(exitEvt, 333);

        if (r == WAIT_OBJECT_0)
            break;

        if (r == WAIT_TIMEOUT)
        {
            EnterCriticalSection(&gListCS);
            if (!gList.empty())
            {
                gList.pop_back();
            }
            LeaveCriticalSection(&gListCS);
        }
    }
    return 0;
}

unsigned __stdcall WorkerThread(void* /*lpParam*/)
{
    while (true)
    {
        DWORD r = WaitForSingleObject(exitEvt, 1000);

        if (r == WAIT_OBJECT_0)
            break;

        if (r == WAIT_TIMEOUT)
        {
            EnterCriticalSection(&gListCS);
            gList.push_back(rand());
            LeaveCriticalSection(&gListCS);
        }
    }
    return 0;
}

unsigned __stdcall SaveThread(void* /*lpParam*/)
{
    HANDLE waits[2] = { exitEvt, saveEvt };

    while (true)
    {
        DWORD r = WaitForMultipleObjects(2, waits, FALSE, INFINITE);

        if (r == WAIT_OBJECT_0)
        {
            // exitEvt
            break;
        }
        else if (r == WAIT_OBJECT_0 + 1)
        {
            // saveEvt
            FILE* fp = nullptr;
            if (_wfopen_s(&fp, L"Test.txt", L"wt, ccs=UTF-8") != 0 || fp == nullptr)
                continue;

            wstring out;
            bool first = true;
            EnterCriticalSection(&gListCS);
            list<int> tempList = gList;
            LeaveCriticalSection(&gListCS);

            for (auto it = tempList.begin(); it != tempList.end(); ++it)
            {
                if (!first) out += L"-";
                first = false;
                out += std::to_wstring(*it);
            }

            std::fwprintf(fp, L"%ls\n", out.c_str());
            std::fclose(fp);
        }
    }

    return 0;
}

int main()
{
    InitializeCriticalSection(&gListCS);

    // exit: manual-reset, initially nonsignaled
    exitEvt = CreateEvent(nullptr, TRUE, FALSE, nullptr);
    // save: auto-reset, initially nonsignaled
    saveEvt = CreateEvent(nullptr, FALSE, FALSE, nullptr);

    if (!exitEvt || !saveEvt)
        return 1;

    threadHandleArr[handleCnt++] = (HANDLE)_beginthreadex(nullptr, 0, MainThread, nullptr, 0, nullptr);
    threadHandleArr[handleCnt++] = (HANDLE)_beginthreadex(nullptr, 0, PrintThread, nullptr, 0, nullptr);
    threadHandleArr[handleCnt++] = (HANDLE)_beginthreadex(nullptr, 0, DeleteThread, nullptr, 0, nullptr);

    threadHandleArr[handleCnt++] = (HANDLE)_beginthreadex(nullptr, 0, WorkerThread, nullptr, 0, nullptr);
    threadHandleArr[handleCnt++] = (HANDLE)_beginthreadex(nullptr, 0, WorkerThread, nullptr, 0, nullptr);
    threadHandleArr[handleCnt++] = (HANDLE)_beginthreadex(nullptr, 0, WorkerThread, nullptr, 0, nullptr);

    // CREATE_SUSPENDED 제거: 이벤트로 깨어나는 구조가 과제 의도
    threadHandleArr[handleCnt++] = (HANDLE)_beginthreadex(nullptr, 0, SaveThread, nullptr, 0, nullptr);

    // MainThread 종료까지 대기
    WaitForSingleObject(threadHandleArr[0], INFINITE);

    // 핸들 정리
    for (int i = 0; i < handleCnt; ++i)
    {
        if (threadHandleArr[i])
            CloseHandle(threadHandleArr[i]);
    }
    CloseHandle(exitEvt);
    CloseHandle(saveEvt);
    
    DeleteCriticalSection(&gListCS);

    return 0;
}
