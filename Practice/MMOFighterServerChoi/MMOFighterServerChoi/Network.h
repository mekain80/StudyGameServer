#pragma once

#include <list>
#include <conio.h>

#include <WinSock2.h>

#include "Session.h"

extern bool gShutdown;
extern DWORD gAllocID;
extern SOCKET gListenSocket;
extern std::list<Session*> gSessionList;

void NetStartUp() noexcept;
void NetEnd() noexcept;
void NetIOProcess() noexcept;
void NetProc_Accept() noexcept;
bool NetProc_Recv(Session* pSession) noexcept;
bool NetProc_Send(Session* pSession) noexcept;