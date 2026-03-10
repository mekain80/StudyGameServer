#include "stdafx.h"

#include "Game.h"

#include "Character.h"
#include "GameInfo.h"
#include "Log.h"
#include "Network.h"
#include "Protocol.h"
#include "PacketControl.h"

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
            Disconnect(pCharacter->session);
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
