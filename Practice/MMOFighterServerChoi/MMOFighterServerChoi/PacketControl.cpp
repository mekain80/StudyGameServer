#include "stdafx.h"

#include "PacketControl.h"

#include "Log.h"
#include "Network.h"
#include "Proxy.h"
#include "Character.h"
#include "Sector.h"

namespace
{
    std::vector<Session*> gPendingDisconnects;

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
    if (session == nullptr || session->disconnectFlag)
        return false;

    const int totalSize = static_cast<int>(sizeof(PacketHeader)) + pHeader->size;

    if (session->sendQ.GetFreeSize() < totalSize)
    {
        _LOG(LOG_LEVEL_ERROR, L"sendQ is full, disconnect");
        _LOG(LOG_LEVEL_ERROR, L"sendQ is full ID=%d IP=%s", session->sessionID, session->ipStr);

        Disconnect(session, L"sendQ free size 부족");
        return false;
    }

    bool isHeaderEnqueued = session->sendQ.Enqueue(reinterpret_cast<char*>(pHeader), sizeof(PacketHeader));
    bool isBodyEnqueued = session->sendQ.Enqueue(pPacket, pHeader->size);
    if (!isHeaderEnqueued || !isBodyEnqueued)
    {
        Disconnect(session, L"sendQ enqueue 실패");
        return false;
    }

    return true;
}

void SendUnicast(Session* pSession, PacketHeader* pHeader, char* pPacket) noexcept
{
    if (pSession == nullptr || pSession->disconnectFlag)
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
        if (!IsCharacterActive(character))
        {
            continue;
        }

        Session* session = FindSession(character->sessionID);
        if (session == nullptr)
        {
            continue;
        }

        if (session == pExceptSession)
        {
            continue;
        }

        EnqueuePacket(session, pHeader, pPacket);
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

void Disconnect(Session* pSession, const WCHAR* reason) noexcept
{
    if (pSession == nullptr)
    {
        return;
    }

    if (pSession->disconnectFlag)
    {
        return; // 중복 예약 방지
    }
    pSession->disconnectFlag = true;

    _LOG(
        LOG_LEVEL_SYSTEM,
        L"Disconnect 예약 # SessionID:%u / IP:%s / Socket:%llu / Reason:%s",
        pSession->sessionID,
        pSession->ipStr,
        static_cast<unsigned long long>(pSession->socket),
        (reason != nullptr) ? reason : L"(none)");

    // 앞으로 새로운 루프/조회에서 안 잡히게 먼저 분리
    gSessionIdMap.erase(pSession->sessionID);
    gSessionMap.erase(pSession->socket);

    // 소켓은 바로 닫아도 됨
    if (pSession->socket != INVALID_SOCKET)
    {
        shutdown(pSession->socket, SD_BOTH);
        closesocket(pSession->socket);
        pSession->socket = INVALID_SOCKET;
    }

    gPendingDisconnects.push_back(pSession);
}

void FlushDisconnectedSessions() noexcept
{
    if (gPendingDisconnects.empty())
    {
        return;
    }

    std::vector<Session*> pending(gPendingDisconnects.size());
    pending.swap(gPendingDisconnects); // flush 중 새로 들어온 건 다음 flush에서 처리

    for (Session* pSession : pending)
    {
        if (pSession == nullptr)
        {
            continue;
        }

        Character* pCharacter = FindCharacter(pSession->sessionID);

        if (pCharacter != nullptr)
        {
            PacketHeader header{};
            PacketSCDeleteCharacter packet{};
            MakePacket_DeleteCharacter(&header, &packet, pSession->sessionID);

            // 아직 Character와 sector 정보가 살아 있으니 주변에 삭제 통지 가능
            SendPacket_Around(pSession, &header, reinterpret_cast<char*>(&packet));

            RemoveSector(pCharacter);
            gCharacterMap.erase(pCharacter->sessionID);
            delete pCharacter;
        }

        delete pSession;
    }
}
