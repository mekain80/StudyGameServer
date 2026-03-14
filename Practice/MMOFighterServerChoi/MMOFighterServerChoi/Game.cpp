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

namespace
{
	void SendDeleteCharacterPacket(Session* session, DWORD sessionID) noexcept
	{
		PacketHeader packetHeader{};
		PacketSCDeleteCharacter packet{};
		MakePacket_DeleteCharacter(&packetHeader, &packet, sessionID);
		SendUnicast(session, &packetHeader, reinterpret_cast<char*>(&packet));
	}

	void SendCreateCharacterPacket(Session* session, const Character* character) noexcept
	{
		PacketHeader packetHeader{};
		PacketSCCreateOtherCharacter packet{};
		MakePacket_CreateOtherCharacter(
			&packetHeader,
			&packet,
			character->direction,
			character->sessionID,
			character->x,
			character->y,
			character->HP);
		SendUnicast(session, &packetHeader, reinterpret_cast<char*>(&packet));
	}

	void SendMoveStartPacket(Session* session, const Character* character) noexcept
	{
		if (character->action == dfACTION_STOP)
		{
			return;
		}

		PacketHeader packetHeader{};
		PacketSCMoveStart packet{};
		MakePacket_MoveStart(
			&packetHeader,
			&packet,
			character->sessionID,
			character->action,
			character->x,
			character->y);
		SendUnicast(session, &packetHeader, reinterpret_cast<char*>(&packet));
	}

	void BroadcastCharacterLeave(const SectorAround& removeAround, DWORD sessionID) noexcept
	{
		PacketHeader packetHeader{};
		PacketSCDeleteCharacter packet{};
		MakePacket_DeleteCharacter(&packetHeader, &packet, sessionID);
		SendPacket_BySectorAround(removeAround, &packetHeader, reinterpret_cast<char*>(&packet));
	}

	void SyncRemovedCharactersToCurrentSession(const SectorAround& removeAround, const Character* movingCharacter, Session* currentSession) noexcept
	{
		for (int index = 0; index < removeAround.count; ++index)
		{
			const SectorPos sectorPos = removeAround.around[index];
			std::list<Character*>& sectorList = gSector[sectorPos.y][sectorPos.x];
			for (Character* character : sectorList)
			{
				if (character == movingCharacter || !IsCharacterActive(character))
				{
					continue;
				}

				SendDeleteCharacterPacket(currentSession, character->sessionID);
			}
		}
	}

	void BroadcastCharacterEnter(const SectorAround& addAround, const Character* movingCharacter) noexcept
	{
		PacketHeader createHeader{};
		PacketSCCreateOtherCharacter createPacket{};
		MakePacket_CreateOtherCharacter(
			&createHeader,
			&createPacket,
			movingCharacter->direction,
			movingCharacter->sessionID,
			movingCharacter->x,
			movingCharacter->y,
			movingCharacter->HP);
		SendPacket_BySectorAround(addAround, &createHeader, reinterpret_cast<char*>(&createPacket));

		if (movingCharacter->action == dfACTION_STOP)
		{
			return;
		}

		PacketHeader moveHeader{};
		PacketSCMoveStart movePacket{};
		MakePacket_MoveStart(
			&moveHeader,
			&movePacket,
			movingCharacter->sessionID,
			movingCharacter->action,
			movingCharacter->x,
			movingCharacter->y);
		SendPacket_BySectorAround(addAround, &moveHeader, reinterpret_cast<char*>(&movePacket));
	}

	void SyncAddedCharactersToCurrentSession(const SectorAround& addAround, const Character* movingCharacter, Session* currentSession) noexcept
	{
		for (int index = 0; index < addAround.count; ++index)
		{
			const SectorPos sectorPos = addAround.around[index];
			std::list<Character*>& sectorList = gSector[sectorPos.y][sectorPos.x];
			for (Character* character : sectorList)
			{
				if (character == movingCharacter || !IsCharacterActive(character))
				{
					continue;
				}

				SendCreateCharacterPacket(currentSession, character);
				SendMoveStartPacket(currentSession, character);
			}
		}
	}
}

void UpdateCharacterSector(Character* pCharacter, Session* currentSession) noexcept
{
    if (pCharacter == nullptr || currentSession == nullptr || currentSession->disconnectFlag)
    {
        return;
    }

    // 이동이 반영된 현재 좌표로 섹터를 다시 계산한다.
    SectorPos curSector = CalcSector(pCharacter->x, pCharacter->y);
    // 이전 섹터와 같으면 시야 갱신이 필요 없다.
    if (IsSameSector(curSector, pCharacter->sector))
    {
        return;
    }

    // 섹터 이동 전후를 비교해서
    // 더 이상 보이지 않는 섹터와 새로 보이기 시작한 섹터를 구한다.
    SectorAround removeAround;
    SectorAround addAround;
    GetUpdateSectorAround(pCharacter, &removeAround, &addAround);
    // 계산이 끝났으면 캐릭터가 실제로 속한 섹터 정보를 갱신한다.
    if (!UpdateSector(pCharacter, &curSector))
    {
        return;
    }

    BroadcastCharacterLeave(removeAround, pCharacter->sessionID);
    SyncRemovedCharactersToCurrentSession(removeAround, pCharacter, currentSession);
    BroadcastCharacterEnter(addAround, pCharacter);
    SyncAddedCharactersToCurrentSession(addAround, pCharacter, currentSession);

}

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

        if (!IsCharacterActive(pCharacter))
        {
            continue;
        }

        Session* currentSession = FindSession(pCharacter->sessionID);
        if (currentSession == nullptr)
        {
            continue;
        }

        if (pCharacter->HP <= 0)
        {
            Disconnect(currentSession, L"HP 0 이하");
            continue;
        }

        const ULONGLONG currentTick = GetTickCount64();
        const ULONGLONG timeoutTick = static_cast<ULONGLONG>(NETWORK_PACKET_RECV_TIMEOUT) * 1000;
        if (currentTick - currentSession->lastRecvTime >= timeoutTick)
        {
            Disconnect(currentSession, L"recv timeout");
            continue;
        }

        if (!MoveCheck(pCharacter->action, pCharacter->x, pCharacter->y))
        {
            continue;
        }

        int dx = 0;
        int dy = 0;
        GetMoveDelta(pCharacter->action, dx, dy);
        pCharacter->x += dx;
        pCharacter->y += dy;

        // 이동 로그
        //{
        //    const wchar_t* dirStr = L"STOP";
        //    switch (pCharacter->action)
        //    {
        //    case dfPACKET_MOVE_DIR_UU: dirStr = L"UU"; break;
        //    case dfPACKET_MOVE_DIR_DD: dirStr = L"DD"; break;
        //    case dfPACKET_MOVE_DIR_RR: dirStr = L"RR"; break;
        //    case dfPACKET_MOVE_DIR_LL: dirStr = L"LL"; break;
        //    case dfPACKET_MOVE_DIR_RU: dirStr = L"RU"; break;
        //    case dfPACKET_MOVE_DIR_RD: dirStr = L"RD"; break;
        //    case dfPACKET_MOVE_DIR_LU: dirStr = L"LU"; break;
        //    case dfPACKET_MOVE_DIR_LD: dirStr = L"LD"; break;
        //    default:                   dirStr = L"STOP"; break;
        //    }

        //    _LOG(
        //        LOG_LEVEL_DEBUG,
        //        L"# gameRun : %s # SessionID : %u / X : %d / Y : %d",
        //        dirStr,
        //        pCharacter->sessionID,
        //        pCharacter->x,
        //        pCharacter->y);
        //}



        UpdateCharacterSector(pCharacter, currentSession);
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

BYTE NormalizeViewDir(BYTE direction, BYTE currentDirection) noexcept
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
    case dfPACKET_MOVE_DIR_UU:
    case dfPACKET_MOVE_DIR_DD:
        if (currentDirection == dfPACKET_MOVE_DIR_LL)
        {
            return dfPACKET_MOVE_DIR_LL;
        }

        return dfPACKET_MOVE_DIR_RR;
    default:
        if (currentDirection == dfPACKET_MOVE_DIR_LL)
        {
            return dfPACKET_MOVE_DIR_LL;
        }

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
