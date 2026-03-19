#pragma once

#include <cstddef>
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <unordered_map>
#include <vector>

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
    ULONGLONG lastRecvTime = 0; // 메시지 수신 체크를 위한 시간 (타임아웃용)
    bool disconnectFlag = false;
    std::size_t activeSessionIndex = static_cast<std::size_t>(-1);
};

using ActiveSessionList = std::vector<Session*>;

extern SOCKET gListenSocket;
extern std::unordered_map<SOCKET, Session*> gSessionMap;
extern std::unordered_map<DWORD, Session*> gSessionIdMap;
extern ActiveSessionList gActiveSessions;

Session* AllocSession() noexcept;
void FreeSession(Session* session) noexcept;
Session* FindSession(DWORD sessionID) noexcept;
void AddActiveSession(Session* session) noexcept;
void RemoveActiveSession(Session* session) noexcept;
