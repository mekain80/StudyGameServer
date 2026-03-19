#include "stdafx.h"

#include "PacketControl.h"

#include "Log.h"
#include "Network.h"
#include "Proxy.h"
#include "Character.h"
#include "Game.h"
#include "Sector.h"

namespace
{
    std::vector<Session*> gPendingDisconnects;

    bool IsValidSectorIndex(short sectorX, short sectorY) noexcept
    {
        return sectorX >= 0
            && sectorX < dfSECTOR_MAX_X
            && sectorY >= 0
            && sectorY < dfSECTOR_MAX_Y;
    }

    void SendPacket_ToSectorList(
        const std::list<Character*>& sectorList,
        const SerializedBuffer* pPacket,
        Session* pExceptSession) noexcept
    {
        for (Character* character : sectorList)
        {
            Session* session = FindActiveSession(character);
            if (session == nullptr || session == pExceptSession)
            {
                continue;
            }

            EnqueuePacket(session, pPacket);
        }
    }
}

bool EnqueuePacket(Session* session, const SerializedBuffer* pPacket) noexcept
{
    if (session == nullptr || session->disconnectFlag || pPacket == nullptr)
        return false;

    const int packetSize = pPacket->GetDataSize();

    if (session->sendQ.GetFreeSize() < packetSize)
    {
        _LOG(LOG_LEVEL_ERROR, L"sendQ is full, disconnect");
        _LOG(LOG_LEVEL_ERROR, L"sendQ is full ID=%d IP=%s", session->sessionID, session->ipStr);

        Disconnect(session, L"sendQ free size 부족");
        return false;
    }

    bool isPacketEnqueued = session->sendQ.Enqueue(pPacket->GetBufferRead(), packetSize);
    if (!isPacketEnqueued)
    {
        Disconnect(session, L"sendQ enqueue 실패");
        return false;
    }

    return true;
}

void SendUnicast(Session* pSession, const SerializedBuffer* pPacket) noexcept
{
    if (pSession == nullptr || pSession->disconnectFlag)
    {
        return;
    }

    EnqueuePacket(pSession, pPacket);
}

void SendBroadcast(Session* pSession, const SerializedBuffer* pPacket) noexcept
{
    for (auto it = gSessionMap.begin(); it != gSessionMap.end();)
    {
        Session* session = it->second;
        ++it;

        if (session == pSession)
        {
            continue;
        }

        EnqueuePacket(session, pPacket);
    }
}

void SendPacket_SectorOne(int sectorX, int sectorY, const SerializedBuffer* pPacket, Session* pExceptSession) noexcept
{
    if (!IsValidSectorIndex(static_cast<short>(sectorX), static_cast<short>(sectorY)))
    {
        return;
    }

    SendPacket_ToSectorList(gSector[sectorY][sectorX], pPacket, pExceptSession);
}

void SendPacket_BySectorAround(const SectorAround& sectorAround, const SerializedBuffer* pPacket, Session* pExceptSession) noexcept
{
    for (int index = 0; index < sectorAround.count; ++index)
    {
        const SectorPos& sectorPos = sectorAround.around[index];
        if (!IsValidSectorIndex(sectorPos.x, sectorPos.y))
        {
            continue;
        }

        SendPacket_ToSectorList(gSector[sectorPos.y][sectorPos.x], pPacket, pExceptSession);
    }
}

void SendPacket_AroundCharacter(const Character* character, const SerializedBuffer* pPacket, Session* pExceptSession) noexcept
{
    if (character == nullptr)
    {
        return;
    }

    SectorAround sectorAround{};
    GetSectorAroundBySector(&character->sector, &sectorAround);
    SendPacket_BySectorAround(sectorAround, pPacket, pExceptSession);
}

void SendPacket_Around(Session* pSession, const SerializedBuffer* pPacket, bool sendMe) noexcept
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

    Session* exceptSession = sendMe ? nullptr : pSession;
    SendPacket_AroundCharacter(character, pPacket, exceptSession);
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

    Character* character = FindCharacter(pSession->sessionID);
    if (character != nullptr)
    {
        character->session = nullptr;
    }

    _LOG(
        LOG_LEVEL_DEBUG,
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
            SerializedBuffer packet(dfPACKET_HEADER_SIZE + dfPACKET_SC_DELETE_CHARACTER_SIZE);
            MakePacket_DeleteCharacter(&packet, pSession->sessionID);

            // 아직 Character와 sector 정보가 살아 있으니 주변에 삭제 통지 가능
            SendPacket_AroundCharacter(pCharacter, &packet, pSession);

            RemoveSector(pCharacter);
            gCharacterMap.erase(pCharacter->sessionID);
            pCharacter->session = nullptr;
            RemoveCharacterFromUpdateTracking(pCharacter);
            FreeCharacter(pCharacter);
        }

        FreeSession(pSession);
    }
}
