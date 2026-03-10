#include "stdafx.h"

#include <WS2tcpip.h>
#include <stdio.h>
#include <tchar.h>

#include "Game.h"
#include "GameInfo.h"
#include "Log.h"
#include "Network.h"

int _tmain(int argc, _TCHAR* argv[])
{
    UNREFERENCED_PARAMETER(argc);
    UNREFERENCED_PARAMETER(argv);

    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    system("chcp 65001 > nul");

    timeBeginPeriod(1);
    QueryPerformanceFrequency(&gFreq);
    QueryPerformanceCounter(&gFrameStartTick);

    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
    {
        ErrorHandler(L"WSAStartup fail");
    }
    Logger(L"WSAStartup #");

    gListenSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (gListenSocket == INVALID_SOCKET)
    {
        ErrorHandler(L"socket fail");
    }

    u_long on = 1;
    if (ioctlsocket(gListenSocket, FIONBIO, &on) == SOCKET_ERROR)
    {
        ErrorHandler(L"ioctlsocket fail");
    }

    SOCKADDR_IN serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(SERVER_PORT);
    serverAddr.sin_addr.S_un.S_addr = htonl(INADDR_ANY);

    if (bind(gListenSocket, reinterpret_cast<SOCKADDR*>(&serverAddr), sizeof(serverAddr)) == SOCKET_ERROR)
    {
        ErrorHandler(L"bind fail");
    }

    {
        wchar_t buf[256];
        _snwprintf_s(buf, 256, _TRUNCATE, L"BIND OK # Port:%d", SERVER_PORT);
        Logger(buf);
    }

    if (listen(gListenSocket, SOMAXCONN) == SOCKET_ERROR)
    {
        ErrorHandler(L"listen() fail");
    }

    Logger(L"Listen OK #");

    while (!gbShutdown)
    {
        NetIOProcess();
        Update();
    }

    return 0;
}
