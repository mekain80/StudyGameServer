#include "stdafx.h"

#include "Game.h"

#include "Character.h"
#include "GameInfo.h"
#include "Log.h"
#include "Network.h"
#include "Protocol.h"
#include "PacketControl.h"
#include "Sector.h"
#include "proxy.h"

LARGE_INTEGER gFreq{};
LARGE_INTEGER gFrameStartTick{};
LARGE_INTEGER gFrameEndTick{};

void Update() noexcept
{
    QueryPerformanceCounter(&gFrameEndTick);
    double elapsed = static_cast<double>(gFrameEndTick.QuadPart - gFrameStartTick.QuadPart) / gFreq.QuadPart;

    if (elapsed <= 0.02)
    {
        return;
    }

    gFrameStartTick = gFrameEndTick;

    for (auto it = gCharacterMap.begin(); it != gCharacterMap.end();)
    {
        Character* pCharacter = it->second;
        ++it;

        if (pCharacter->HP <= 0)
        {
            Session* session = FindSession(pCharacter->sessionID);
            Disconnect(session);
            continue;
        }

        const ULONGLONG currentTick = GetTickCount64();
        const ULONGLONG timeoutTick = static_cast<ULONGLONG>(NETWORK_PACKET_RECV_TIMEOUT) * 1000;
        Session* currentSession = FindSession(pCharacter->sessionID);
        if (currentSession == nullptr)
        {
            continue;
        }

        if (currentTick - currentSession->lastRecvTime >= timeoutTick)
        {
            Disconnect(currentSession);
            continue;
        }

        if (!MoveCheck(pCharacter->action, pCharacter->x, pCharacter->y))
        {
            continue;
        }

        int dx = 0;
        int dy = 0;
        GetMoveDelta(pCharacter->action, dx, dy);
        pCharacter->x += static_cast<short>(dx);
        pCharacter->y += static_cast<short>(dy);

        // 이동 로그
        {
            const wchar_t* dirStr = L"STOP";
            switch (pCharacter->action)
            {
            case dfPACKET_MOVE_DIR_UU: dirStr = L"UU"; break;
            case dfPACKET_MOVE_DIR_DD: dirStr = L"DD"; break;
            case dfPACKET_MOVE_DIR_RR: dirStr = L"RR"; break;
            case dfPACKET_MOVE_DIR_LL: dirStr = L"LL"; break;
            case dfPACKET_MOVE_DIR_RU: dirStr = L"RU"; break;
            case dfPACKET_MOVE_DIR_RD: dirStr = L"RD"; break;
            case dfPACKET_MOVE_DIR_LU: dirStr = L"LU"; break;
            case dfPACKET_MOVE_DIR_LD: dirStr = L"LD"; break;
            default:                   dirStr = L"STOP"; break;
            }

            _LOG(
                LOG_LEVEL_DEBUG,
                L"# gameRun : %s # SessionID : %u / X : %d / Y : %d",
                dirStr,
                pCharacter->sessionID,
                pCharacter->x,
                pCharacter->y);
        }



        // 이동이 반영된 현재 좌표로 섹터를 다시 계산한다.
        SectorPos curSector = CalcSector(pCharacter->x, pCharacter->y);
        // 이전 섹터와 같으면 시야 갱신이 필요 없다.
        if (IsSameSector(curSector, pCharacter->sector))
        {
            continue;
        }

        // 섹터 이동 전후를 비교해서
        // 더 이상 보이지 않는 섹터와 새로 보이기 시작한 섹터를 구한다.
        SectorAround removeAround;
        SectorAround addAround;
        GetUpdateSectorAround(pCharacter, &removeAround, &addAround);
        // 계산이 끝났으면 캐릭터가 실제로 속한 섹터 정보를 갱신한다.
        if (!UpdateSector(pCharacter, &curSector))
        {
            continue;
        }

        // 1) 시야에서 빠진 섹터에 있는 다른 유저들에게
        // 이동한 캐릭터를 삭제하라고 알린다.
        PacketHeader delHeader;
        PacketSCDeleteCharacter delMsg;
        MakePacket_DeleteCharacter(&delHeader, &delMsg, pCharacter->sessionID);
        SendPacket_BySectorAround(removeAround, &delHeader, reinterpret_cast<char*>(&delMsg));

        // 2) 이동한 본인에게도 시야에서 빠진 다른 캐릭터들을 삭제시킨다.
        for (int index = 0; index < removeAround.count; ++index)
        {
            const SectorPos sectorPos = removeAround.around[index];
            std::list<Character*>& sectorList = gSector[sectorPos.y][sectorPos.x];
            for (Character* character : sectorList)
            {
                if (character == nullptr || character == pCharacter)
                {
                    continue;
                }

                PacketHeader otherDelHeader;
                PacketSCDeleteCharacter otherDelMsg;
                MakePacket_DeleteCharacter(&otherDelHeader, &otherDelMsg, character->sessionID);
                SendUnicast(currentSession, &otherDelHeader, reinterpret_cast<char*>(&otherDelMsg));
            }
        }

        // 3) 새 시야에 들어온 섹터의 다른 유저들에게
        // 이동한 캐릭터를 새로 생성하라고 알린다.
        PacketHeader phCreateCharacter;
        PacketSCCreateOtherCharacter createMsg;
        MakePacket_CreateOtherCharacter(&phCreateCharacter, &createMsg, pCharacter->direction, pCharacter->sessionID, pCharacter->x, pCharacter->y, pCharacter->HP);
        SendPacket_BySectorAround(addAround, &phCreateCharacter, reinterpret_cast<char*>(&createMsg));

        // 4) 이동한 본인에게도 새 시야에 들어온 다른 캐릭터들을 생성시킨다.
        for (int index = 0; index < addAround.count; ++index)
        {
            const SectorPos sectorPos = addAround.around[index];
            std::list<Character*>& sectorList = gSector[sectorPos.y][sectorPos.x];
            for (Character* character : sectorList)
            {
                if (character == nullptr || character == pCharacter)
                {
                    continue;
                }

                PacketHeader otherCreateHeader;
                PacketSCCreateOtherCharacter otherCreateMsg;
                MakePacket_CreateOtherCharacter(
                    &otherCreateHeader,
                    &otherCreateMsg,
                    character->direction,
                    character->sessionID,
                    character->x,
                    character->y,
                    character->HP);
                SendUnicast(currentSession, &otherCreateHeader, reinterpret_cast<char*>(&otherCreateMsg));
            }
        }

        // 섹터 변경 전/후 정보를 기록한다.
        {
            const wchar_t* dirStr = L"STOP";
            switch (pCharacter->action)
            {
            case dfPACKET_MOVE_DIR_UU: dirStr = L"UU"; break;
            case dfPACKET_MOVE_DIR_DD: dirStr = L"DD"; break;
            case dfPACKET_MOVE_DIR_RR: dirStr = L"RR"; break;
            case dfPACKET_MOVE_DIR_LL: dirStr = L"LL"; break;
            case dfPACKET_MOVE_DIR_RU: dirStr = L"RU"; break;
            case dfPACKET_MOVE_DIR_RD: dirStr = L"RD"; break;
            case dfPACKET_MOVE_DIR_LU: dirStr = L"LU"; break;
            case dfPACKET_MOVE_DIR_LD: dirStr = L"LD"; break;
            default:                   dirStr = L"STOP"; break;
            }

            _LOG(
                LOG_LEVEL_DEBUG,
                L"# sectorMove : %s # SessionID : %u / X : %d / Y : %d / OldSector : (%d, %d) / NewSector : (%d, %d)",
                dirStr,
                pCharacter->sessionID,
                pCharacter->x,
                pCharacter->y,
                curSector.x,
                curSector.y,
                pCharacter->sector.x,
                pCharacter->sector.y);
        }
    }
}

bool MoveCheck(BYTE direction, int x, int y) noexcept
{
    int dx = 0;
    int dy = 0;

    GetMoveDelta(direction, dx, dy);

    if (dx == 0 && dy == 0 &&
        direction != dfPACKET_MOVE_DIR_UU &&
        direction != dfPACKET_MOVE_DIR_DD &&
        direction != dfPACKET_MOVE_DIR_RR &&
        direction != dfPACKET_MOVE_DIR_LL &&
        direction != dfPACKET_MOVE_DIR_RU &&
        direction != dfPACKET_MOVE_DIR_RD &&
        direction != dfPACKET_MOVE_DIR_LU &&
        direction != dfPACKET_MOVE_DIR_LD)
    {
        return false;
    }

    int nx = x + dx;
    int ny = y + dy;

    if (ny < dfRANGE_MOVE_TOP || ny > dfRANGE_MOVE_BOTTOM)
    {
        return false;
    }

    if (nx < dfRANGE_MOVE_LEFT || nx > dfRANGE_MOVE_RIGHT)
    {
        return false;
    }

    return true;
}

BYTE NormalizeViewDir(BYTE direction) noexcept
{
    switch (direction)
    {
    case dfPACKET_MOVE_DIR_RR:
    case dfPACKET_MOVE_DIR_RU:
    case dfPACKET_MOVE_DIR_RD:
        return dfPACKET_MOVE_DIR_RR;
    case dfPACKET_MOVE_DIR_LU:
    case dfPACKET_MOVE_DIR_LL:
    case dfPACKET_MOVE_DIR_LD:
        return dfPACKET_MOVE_DIR_LL;
    default:
        return dfPACKET_MOVE_DIR_RR;
    }
}

void GetMoveDelta(BYTE direction, int& dx, int& dy) noexcept
{
    dx = 0;
    dy = 0;

    switch (direction)
    {
    case dfPACKET_MOVE_DIR_UU: dy = -dfMOVE_Y; break;
    case dfPACKET_MOVE_DIR_DD: dy = +dfMOVE_Y; break;
    case dfPACKET_MOVE_DIR_RR: dx = +dfMOVE_X; break;
    case dfPACKET_MOVE_DIR_LL: dx = -dfMOVE_X; break;
    case dfPACKET_MOVE_DIR_RU: dx = +dfMOVE_X; dy = -dfMOVE_Y; break;
    case dfPACKET_MOVE_DIR_RD: dx = +dfMOVE_X; dy = +dfMOVE_Y; break;
    case dfPACKET_MOVE_DIR_LU: dx = -dfMOVE_X; dy = -dfMOVE_Y; break;
    case dfPACKET_MOVE_DIR_LD: dx = -dfMOVE_X; dy = +dfMOVE_Y; break;
    default:
        break;
    }
}

bool IsValidMoveDirection(BYTE direction) noexcept
{
    switch (direction)
    {
    case dfPACKET_MOVE_DIR_UU:
    case dfPACKET_MOVE_DIR_DD:
    case dfPACKET_MOVE_DIR_RR:
    case dfPACKET_MOVE_DIR_LL:
    case dfPACKET_MOVE_DIR_RU:
    case dfPACKET_MOVE_DIR_RD:
    case dfPACKET_MOVE_DIR_LU:
    case dfPACKET_MOVE_DIR_LD:
        return true;
    default:
        return false;
    }
}

bool IsValidViewDirection(BYTE direction) noexcept
{
    return (direction == dfPACKET_MOVE_DIR_LL || direction == dfPACKET_MOVE_DIR_RR);
}

WORD GetExpectedBodySize(BYTE packetType) noexcept
{
    switch (packetType)
    {
    case dfPACKET_CS_MOVE_START: return sizeof(PacketCSMoveStart);
    case dfPACKET_CS_MOVE_STOP:  return sizeof(PacketCSMoveStop);
    case dfPACKET_CS_ATTACK1:    return sizeof(PacketCSAttack1);
    case dfPACKET_CS_ATTACK2:    return sizeof(PacketCSAttack2);
    case dfPACKET_CS_ATTACK3:    return sizeof(PacketCSAttack3);
    default:
        return 0;
    }
}

bool IsHitAttack1(const Character* attacker, const Character* target, int centerX, int centerY) noexcept
{
    if (abs(centerY - target->y) > dfATTACK1_RANGE_Y)
    {
        return false;
    }

    if (attacker->direction == dfPACKET_MOVE_DIR_RR)
    {
        if (target->x < centerX || target->x > centerX + dfATTACK1_RANGE_X)
        {
            return false;
        }
    }
    else
    {
        if (target->x > centerX || target->x < centerX - dfATTACK1_RANGE_X)
        {
            return false;
        }
    }

    return true;
}

bool IsHitDirectionalAttack(const Character* attacker, const Character* target, int centerX, int centerY, int rangeX, int rangeY) noexcept
{
    if (abs(centerY - target->y) > rangeY)
    {
        return false;
    }

    if (attacker->direction == dfPACKET_MOVE_DIR_RR)
    {
        if (target->x < centerX || target->x > centerX + rangeX)
        {
            return false;
        }
    }
    else
    {
        if (target->x > centerX || target->x < centerX - rangeX)
        {
            return false;
        }
    }

    return true;
}
