#pragma once

#include <WinSock2.h>
#include <WS2tcpip.h>

#include "RingBuffer.h"

struct Session
{
    SOCKET socket = INVALID_SOCKET;
    SOCKADDR_IN addr{};

    wchar_t ipStr[32]{};
    USHORT port = 0;

    DWORD sessionID = 0;
    RingBuffer recvQ;
    RingBuffer sendQ;

    BYTE action = 0;
    BYTE direction = 0;
    short x = 0;
    short y = 0;
    int HP = 0;
};
