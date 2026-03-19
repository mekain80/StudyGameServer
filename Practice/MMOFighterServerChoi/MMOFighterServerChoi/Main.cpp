#include "stdafx.h"

#include <WS2tcpip.h>
#include <stdio.h>
#include <tchar.h>

#include "Game.h"
#include "GameInfo.h"
#include "Log.h"
#include "Network.h"
#include "ServerControl.h"
#include "PacketControl.h"

int _tmain(int argc, _TCHAR* argv[])
{
    UNREFERENCED_PARAMETER(argc);
    UNREFERENCED_PARAMETER(argv);

    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    system("chcp 65001 > nul");

    LogFileInit();
    timeBeginPeriod(1);
    NetStartUp(); // 네트워크 초기화, 리슨소켓 생성 및 listen

    ULONGLONG monitorTick = GetTickCount64();
    unsigned int frameCount = 0;
    unsigned int loopCount = 0;

    while (!gShutdown)
    {
        ++loopCount;
        NetIOProcess();     // 첫 select 배치만 다음 업데이트 시점까지 대기
        FlushDisconnectedSessions();
        if (Update())       // 게임 로직 업데이트
        {
            ++frameCount;
        }
        FlushDisconnectedSessions();
        

        const ULONGLONG currentTick = GetTickCount64();
        if (currentTick - monitorTick >= 1000)
        {
            _LOG(LOG_LEVEL_SYSTEM, L"Frame : %u  Loop : %u", frameCount, loopCount);
            frameCount = 0;
            loopCount = 0;
            monitorTick = currentTick;

            // 부하 때문에 1초에 한번만 되도록 수정
            ServerControl();    // 키보드 입력을 통해서 서버를 제어할 경우 사용
        }
    }

    NetEnd();

    return 0;
}
