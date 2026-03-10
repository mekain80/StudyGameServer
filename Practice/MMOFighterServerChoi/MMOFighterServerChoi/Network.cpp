#include "stdafx.h"

#include "Network.h"

#include <WS2tcpip.h>

#include "Game.h"
#include "GameInfo.h"
#include "Log.h"
#include "PacketControl.h"
#include "Protocol.h"
#include "Proxy.h"
#include "Stub.h"

bool gbShutdown = false;
DWORD gAllocID = 1;
SOCKET gListenSocket = INVALID_SOCKET;
std::list<Session*> gSessionList;

void NetIOProcess() noexcept
{
    FD_SET readSet;
    FD_SET writeSet;

    FD_ZERO(&readSet);
    FD_ZERO(&writeSet);

    FD_SET(gListenSocket, &readSet);

    for (Session* session : gSessionList)
    {
        FD_SET(session->socket, &readSet);
        if (session->sendQ.GetUseSize() > 0)
        {
            FD_SET(session->socket, &writeSet);
        }
    }

    timeval time{};

    int result = select(0, &readSet, &writeSet, nullptr, &time);
    if (result == SOCKET_ERROR)
    {
        ErrorHandler(L"Select fail");
        return;
    }

    if (result == 0)
    {
        return;
    }

    if (FD_ISSET(gListenSocket, &readSet))
    {
        --result;
        NetProc_Accept();
        if (result <= 0)
        {
            return;
        }
    }

    for (auto it = gSessionList.begin(); it != gSessionList.end() && result > 0;)
    {
        Session* session = *it;
        SOCKET socket = session->socket;
        ++it;

        if (FD_ISSET(socket, &readSet))
        {
            --result;
            if (!NetProc_Recv(session))
            {
                continue;
            }
            if (result <= 0)
            {
                break;
            }
        }

        if (result > 0 && FD_ISSET(socket, &writeSet))
        {
            --result;
            if (!NetProc_Send(session))
            {
                continue;
            }
            if (result <= 0)
            {
                break;
            }
        }
    }
}

void NetProc_Accept() noexcept
{
    SOCKADDR_IN clientAddr{};
    int addrlen = sizeof(clientAddr);

    SOCKET clientSocket = accept(gListenSocket, reinterpret_cast<SOCKADDR*>(&clientAddr), &addrlen);
    if (clientSocket == INVALID_SOCKET)
    {
        Logger(L"clientSocket accept fail");
        return;
    }

    u_long on = 1;
    if (ioctlsocket(clientSocket, FIONBIO, &on) == SOCKET_ERROR)
    {
        closesocket(clientSocket);
        Logger(L"clientSocket ioctlsocket fail");
        return;
    }

    Session* pSession = new Session;
    pSession->socket = clientSocket;
    pSession->addr = clientAddr;

    InetNtopW(AF_INET, &clientAddr.sin_addr, pSession->ipStr, _countof(pSession->ipStr));
    pSession->port = ntohs(clientAddr.sin_port);

    pSession->sessionID = gAllocID++;
    pSession->y = static_cast<short>(dfRANGE_MOVE_TOP + rand() % (dfRANGE_MOVE_BOTTOM - dfRANGE_MOVE_TOP + 1));
    pSession->x = static_cast<short>(dfRANGE_MOVE_LEFT + rand() % (dfRANGE_MOVE_RIGHT - dfRANGE_MOVE_LEFT + 1));
    pSession->direction = dfPACKET_MOVE_DIR_LL;
    pSession->action = dfACTION_STOP;
    pSession->HP = MAX_HP;

    gSessionList.push_back(pSession);

    PacketHeader packetHeader;
    PacketSCCreateMyCharacter createMyCharacter;
    MakePacket_CreateMyCharacter(&packetHeader, &createMyCharacter, pSession->direction, pSession->sessionID, pSession->x, pSession->y, pSession->HP);

    wprintf(L"[Server] CreateMyCharacter header : code=%u size=%u type=%u\n", packetHeader.code, packetHeader.size, packetHeader.type);
    wprintf(
        L"[Server] CreateMyCharacter body   : ID=%u Dir=%u X=%d Y=%d HP=%d\n",
        createMyCharacter.ID,
        createMyCharacter.direction,
        createMyCharacter.x,
        createMyCharacter.y,
        createMyCharacter.HP);

    SendUnicast(pSession, &packetHeader, reinterpret_cast<char*>(&createMyCharacter));

    for (auto& other : gSessionList)
    {
        if (other == pSession)
        {
            continue;
        }

        PacketHeader otherHeader;
        PacketSCCreateOtherCharacter otherPacket;
        MakePacket_CreateOtherCharacter(&otherHeader, &otherPacket, other->direction, other->sessionID, other->x, other->y, other->HP);
        SendUnicast(pSession, &otherHeader, reinterpret_cast<char*>(&otherPacket));
    }

    PacketHeader broadHeader;
    PacketSCCreateOtherCharacter broadPacket;
    MakePacket_CreateOtherCharacter(&broadHeader, &broadPacket, pSession->direction, pSession->sessionID, pSession->x, pSession->y, pSession->HP);
    SendBroadcast(pSession, &broadHeader, reinterpret_cast<char*>(&broadPacket));

    wchar_t buf[256];
    _snwprintf_s(buf, 256, _TRUNCATE, L"Connect # IP:%s / SessionID:%d", pSession->ipStr, pSession->sessionID);
    Logger(buf);
    _snwprintf_s(buf, 256, _TRUNCATE, L"# PACKET_CONNECT # SessionID:%d", pSession->sessionID);
    Logger(buf);
    _snwprintf_s(buf, 256, _TRUNCATE, L"Create Character # SessionID:%d    X:%d    Y:%d", pSession->sessionID, pSession->x, pSession->y);
    Logger(buf);
}

bool NetProc_Recv(Session* pSession) noexcept
{
    char buffer[BUFFER_SIZE] = { 0 };

    int recvRet = recv(pSession->socket, buffer, sizeof(buffer), 0);
    if (recvRet == 0)
    {
        Disconnect(pSession);
        return false;
    }
    else if (recvRet == SOCKET_ERROR)
    {
        int err = WSAGetLastError();
        if (err == WSAEWOULDBLOCK)
        {
            return true;
        }

        if (err == WSAECONNRESET || err == WSAECONNABORTED ||
            err == WSAENETRESET || err == WSAESHUTDOWN || err == WSAENOTCONN)
        {
            wchar_t buf[256];
            _snwprintf_s(buf, 256, _TRUNCATE, L"WSAGetLastError # SessionID:%d    WSA NUM:%d", pSession->sessionID, err);
            Logger(buf);
            Disconnect(pSession);
            return false;
        }

        Disconnect(pSession);
        return false;
    }

    if (!pSession->recvQ.Enqueue(buffer, recvRet))
    {
        Logger(L"recvQ enqueue fail");
        Disconnect(pSession);
        return false;
    }

    while (true)
    {
        const size_t headerSize = sizeof(PacketHeader);
        if (pSession->recvQ.GetUseSize() < headerSize)
        {
            break;
        }

        PacketHeader packetHeader;
        if (!pSession->recvQ.Peek(reinterpret_cast<char*>(&packetHeader), static_cast<int>(headerSize)))
        {
            Disconnect(pSession);
            return false;
        }

        if (packetHeader.code != dfNETWORK_PACKET_CODE)
        {
            Disconnect(pSession);
            return false;
        }

        WORD expectedSize = GetExpectedBodySize(packetHeader.type);
        if (expectedSize == 0 || packetHeader.size != expectedSize)
        {
            Disconnect(pSession);
            return false;
        }

        int totalSize = static_cast<int>(headerSize) + packetHeader.size;
        if (pSession->recvQ.GetUseSize() < totalSize)
        {
            return true;
        }

        pSession->recvQ.MoveFront(static_cast<int>(headerSize));

        if (packetHeader.size > BUFFER_SIZE)
        {
            Disconnect(pSession);
            return false;
        }

        char packetBuffer[BUFFER_SIZE];
        if (!pSession->recvQ.Dequeue(packetBuffer, packetHeader.size))
        {
            Disconnect(pSession);
            return false;
        }

        if (!PacketProc(pSession, packetHeader.type, packetBuffer, packetHeader.size))
        {
            return false;
        }
    }

    return true;
}

bool NetProc_Send(Session* pSession) noexcept
{
    char buffer[BUFFER_SIZE];

    while (true)
    {
        int useSize = static_cast<int>(pSession->sendQ.GetUseSize());
        if (useSize <= 0)
        {
            break;
        }

        int sendSize = useSize;
        if (sendSize > BUFFER_SIZE)
        {
            sendSize = BUFFER_SIZE;
        }

        if (!pSession->sendQ.Peek(buffer, sendSize))
        {
            Disconnect(pSession);
            return false;
        }

        int sendRet = send(pSession->socket, buffer, sendSize, 0);
        if (sendRet == 0)
        {
            Disconnect(pSession);
            return false;
        }
        else if (sendRet == SOCKET_ERROR)
        {
            int err = WSAGetLastError();
            if (err == WSAEWOULDBLOCK)
            {
                return true;
            }

            if (err == WSAECONNRESET || err == WSAECONNABORTED ||
                err == WSAENETRESET || err == WSAESHUTDOWN || err == WSAENOTCONN)
            {
                Disconnect(pSession);
                return false;
            }

            Disconnect(pSession);
            return false;
        }

        pSession->sendQ.MoveFront(sendRet);
    }

    return true;
}
