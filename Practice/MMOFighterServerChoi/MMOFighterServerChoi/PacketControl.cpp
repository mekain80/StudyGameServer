#include "stdafx.h"

#include "PacketControl.h"

#include "Log.h"
#include "Network.h"
#include "Proxy.h"
#include "Character.h"
#include "Sector.h"

namespace
{
    bool IsValidSectorIndex(const SectorPos& sectorPos) noexcept
    {
        return sectorPos.x >= 0
            && sectorPos.x < dfSECTOR_MAX_X
            && sectorPos.y >= 0
            && sectorPos.y < dfSECTOR_MAX_Y;
    }
}

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
    for (auto it = gSessionMap.begin(); it != gSessionMap.end();)
    {
        Session* session = it->second;
        ++it;

        if (session == pSession)
        {
            continue;
        }

        EnqueuePacket(session, pHeader, pPacket);
    }
}

void SendPacket_SectorOne(int sectorX, int sectorY, PacketHeader* pHeader, char* pPacket, Session* pExceptSession) noexcept
{
    SectorPos sectorPos
    {
        static_cast<short>(sectorX),
        static_cast<short>(sectorY)
    };

    if (!IsValidSectorIndex(sectorPos))
    {
        return;
    }

    std::list<Character*>& sectorList = gSector[sectorPos.y][sectorPos.x];
    for (Character* character : sectorList)
    {
        if (character == nullptr || character->session == nullptr)
        {
            continue;
        }

        if (character->session == pExceptSession)
        {
            continue;
        }

        EnqueuePacket(character->session, pHeader, pPacket);
    }
}

void SendPacket_BySectorAround(SectorAround sectorAround, PacketHeader* pHeader, char* pPacket, Session* pExceptSession) noexcept
{
    for (int index = 0; index < sectorAround.count; ++index)
    {
        const SectorPos sectorPos = sectorAround.around[index];
        SendPacket_SectorOne(sectorPos.x, sectorPos.y, pHeader, pPacket, pExceptSession);
    }
}

void SendPacket_Around(Session* pSession, PacketHeader* pHeader, char* pPacket, bool sendMe) noexcept
{
    if (pSession == nullptr)
    {
        return;
    }

    Character* character = FindCharacter(pSession->sessionID);
    if (character == nullptr)
    {
        return;
    }

    SectorAround sectorAround{};
    GetSectorAroundBySector(&character->sector, &sectorAround);
    Session* exceptSession = sendMe ? nullptr : pSession;
    SendPacket_BySectorAround(sectorAround, pHeader, pPacket, exceptSession);
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
    Character* pCharacter = FindCharacter(pSession->sessionID);
    RemoveSector(pCharacter);
    gCharacterMap.erase(pCharacter->sessionID);
    delete pCharacter;
    delete pSession;
}
