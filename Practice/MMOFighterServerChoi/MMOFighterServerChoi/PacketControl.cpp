#include "stdafx.h"

#include "PacketControl.h"

#include "Log.h"
#include "Network.h"
#include "Proxy.h"
#include "Character.h"
#include "Game.h"
#include "Monitor.h"
#include "Sector.h"

namespace
{
    std::vector<Session*> gPendingDisconnects;
    std::vector<Session*> gPendingDisconnectScratch;

    bool IsValidSectorIndex(short sectorX, short sectorY) noexcept
    {
        return sectorX >= 0
            && sectorX < dfSECTOR_MAX_X
            && sectorY >= 0
            && sectorY < dfSECTOR_MAX_Y;
    }

    void SendPacket_ToSectorList(
        const SectorCharacterList& sectorList,
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
    const bool wasSendQueueEmpty = session->sendQ.IsEmpty();

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

    if (wasSendQueueEmpty)
    {
        AddWritableSession(session);
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

void FlushPacketBatch(Session* session, SerializedBuffer* batchPacket) noexcept
{
    if (session == nullptr || batchPacket == nullptr)
    {
        return;
    }

    if (batchPacket->GetDataSize() <= 0)
    {
        return;
    }

    SendUnicast(session, batchPacket);
    batchPacket->Clear();
}

void AppendPacketBatch(Session* session, SerializedBuffer* batchPacket, const SerializedBuffer* packet) noexcept
{
    if (session == nullptr || batchPacket == nullptr || packet == nullptr)
    {
        return;
    }

    const int packetSize = packet->GetDataSize();
    if (packetSize <= 0)
    {
        return;
    }

    if (packetSize > batchPacket->GetBufferSize())
    {
        FlushPacketBatch(session, batchPacket);
        SendUnicast(session, packet);
        return;
    }

    if (batchPacket->GetFreeSize() < packetSize)
    {
        FlushPacketBatch(session, batchPacket);
    }

    if (batchPacket->PutData(packet->GetBufferRead(), packetSize) != packetSize)
    {
        FlushPacketBatch(session, batchPacket);
        SendUnicast(session, packet);
    }
}

void SendBroadcast(Session* pSession, const SerializedBuffer* pPacket) noexcept
{
    const std::size_t activeSessionCount = gActiveSessions.size();
    for (std::size_t index = 0; index < activeSessionCount; ++index)
    {
        Session* session = gActiveSessions[index];
        if (session == nullptr || session->disconnectFlag)
        {
            continue;
        }

        if (session->socket == INVALID_SOCKET)
        {
            continue;
        }

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

    RemoveWritableSession(pSession);

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

    gPendingDisconnectScratch.clear();
    if (gPendingDisconnectScratch.capacity() < gPendingDisconnects.size())
    {
        gPendingDisconnectScratch.reserve(gPendingDisconnects.size());
    }

    gPendingDisconnectScratch.insert(
        gPendingDisconnectScratch.end(),
        gPendingDisconnects.begin(),
        gPendingDisconnects.end());
    gPendingDisconnects.clear(); // flush 중 새로 들어온 건 다음 flush에서 처리

    for (Session* pSession : gPendingDisconnectScratch)
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
            gServerMonitor.OnPlayerReleased();
            FreeCharacter(pCharacter);
        }

        RemoveActiveSession(pSession);
        gServerMonitor.OnSessionReleased();
        FreeSession(pSession);
    }

    gPendingDisconnectScratch.clear();
}
