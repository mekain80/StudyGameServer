#include "stdafx.h"

#include <WS2tcpip.h>
#include <process.h>
#include <stdio.h>
#include <tchar.h>

#include "Game.h"
#include "GameInfo.h"
#include "Log.h"
#include "Monitor.h"
#include "Network.h"
#include "Protocol.h"
#include "Sector.h"
#include "ServerControl.h"
#include "PacketControl.h"

namespace
{
    volatile LONG gMonitorThreadRunning = FALSE;

    unsigned int __stdcall MonitoringThreadFunc(void*)
    {
        DWORD nextTick = timeGetTime();
        while (InterlockedCompareExchange(&gMonitorThreadRunning, 0, 0) != FALSE)
        {
            const DWORD currentTick = timeGetTime();
            const DWORD elapsed = currentTick - nextTick;
            if (elapsed < 1000)
            {
                Sleep(1000 - elapsed);
            }

            if (InterlockedCompareExchange(&gMonitorThreadRunning, 0, 0) == FALSE)
            {
                break;
            }

            gServerMonitor.Tick();
            nextTick += 1000;
        }

        return 0;
    }
}

int _tmain(int argc, _TCHAR* argv[])
{
    UNREFERENCED_PARAMETER(argc);
    UNREFERENCED_PARAMETER(argv);

    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    system("chcp 65001 > nul");

    LogFileInit();
    timeBeginPeriod(1);
    InitializeProtocolCache();
    InitializeSectorCache();
    InitializeGameCache();
    NetStartUp(); // Initialize networking and create the listen socket.

    HANDLE monitorThread = nullptr;
    if (!gShutdown)
    {
        gServerMonitor.Initialize();
        InterlockedExchange(&gMonitorThreadRunning, TRUE);
        monitorThread = reinterpret_cast<HANDLE>(_beginthreadex(nullptr, 0, MonitoringThreadFunc, nullptr, 0, nullptr));
        if (monitorThread == nullptr)
        {
            InterlockedExchange(&gMonitorThreadRunning, FALSE);
            _LOG(LOG_LEVEL_ERROR, L"monitor thread create fail");
        }
    }

    ULONGLONG controlTick = GetTickCount64();

    while (!gShutdown)
    {
        gServerMonitor.OnLoop();
        NetIOProcess();     // Wait only for the first select batch until the next update tick.
        FlushDisconnectedSessions();
        const int updatedFrameCount = Update();
        if (updatedFrameCount > 0)
        {
            gServerMonitor.OnFrame(updatedFrameCount);
        }
        FlushDisconnectedSessions();

        const ULONGLONG currentTick = GetTickCount64();
        if (currentTick - controlTick >= 1000)
        {
            controlTick = currentTick;

            // Check keyboard control once per second to keep the main loop light.
            ServerControl();
        }
    }

    InterlockedExchange(&gMonitorThreadRunning, FALSE);
    if (monitorThread != nullptr)
    {
        WaitForSingleObject(monitorThread, 1500);
        CloseHandle(monitorThread);
    }

    NetEnd();

    return 0;
}
