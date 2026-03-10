#include "stdafx.h"

#include "PacketControl.h"

#include "Log.h"
#include "Network.h"
#include "Proxy.h"
#include "Character.h"

bool EnqueuePacket(Session* session, PacketHeader* pHeader, char* pPacket) noexcept
{
    const int totalSize = static_cast<int>(sizeof(PacketHeader)) + pHeader->size;

    if (session->sendQ.GetFreeSize() < totalSize)
    {
        _LOG(LOG_LEVEL_ERROR, L"sendQ is full, disconnect");
        _LOG(LOG_LEVEL_ERROR, L"sendQ is full ID=%d IP=%s", session->sessionID, session->ipStr);

        Disconnect(session);
        return false;
    }

    bool isHeaderEnqueued = session->sendQ.Enqueue(reinterpret_cast<char*>(pHeader), sizeof(PacketHeader));
    bool isBodyEnqueued = session->sendQ.Enqueue(pPacket, pHeader->size);
    if (!isHeaderEnqueued || !isBodyEnqueued)
    {
        Disconnect(session);
        return false;
    }

    return true;
}

void SendUnicast(Session* pSession, PacketHeader* pHeader, char* pPacket) noexcept
{
    if (pSession == nullptr)
    {
        return;
    }

    EnqueuePacket(pSession, pHeader, pPacket);
}

void SendBroadcast(Session* pSession, PacketHeader* pHeader, char* pPacket) noexcept
{
    for (auto it = gSessionMap.begin(); it != gSessionMap.end(); ++it)
    {
        Session* session = it->second;

        if (session == pSession)
        {
            continue;
        }

        EnqueuePacket(session, pHeader, pPacket);
    }
}

void Disconnect(Session* pSession) noexcept
{
    if (pSession == nullptr)
    {
        return;
    }

    PacketHeader header;
    PacketSCDeleteCharacter packet;
    MakePacket_DeleteCharacter(&header, &packet, pSession->sessionID);

    SendBroadcast(pSession, &header, reinterpret_cast<char*>(&packet));

    closesocket(pSession->socket);
    gSessionMap.erase(pSession->socket);
    gCharacterMap.erase(pSession->sessionID);

    delete pSession;
}