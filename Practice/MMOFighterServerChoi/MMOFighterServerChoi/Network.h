#pragma once

#include <list>

#include <WinSock2.h>

#include "Session.h"

extern bool gbShutdown;
extern DWORD gAllocID;
extern SOCKET gListenSocket;
extern std::list<Session*> gSessionList;

void NetIOProcess() noexcept;
void NetProc_Accept() noexcept;
bool NetProc_Recv(Session* pSession) noexcept;
bool NetProc_Send(Session* pSession) noexcept;
