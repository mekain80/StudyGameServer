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
    // LoadData(); // 설정 및 게임데이터, DB 데이터 로딩
    NetStartUp(); // 네트워크 초기화, 리슨소켓 생성 및 listen

    while (!gShutdown)
    {
        NetIOProcess();     // 네트워크 송수신 처리
        FlushDisconnectedSessions();
        Update();           // 게임 로직 업데이트
        FlushDisconnectedSessions();
        ServerControl();    // 키보드 입력을 통해서 서버를 제어할 경우 사용
        //Monitor();          // 모니터링 정보를 표시,저장, 전송하는 경우 사용
    }

    NetEnd();

    return 0;
}
