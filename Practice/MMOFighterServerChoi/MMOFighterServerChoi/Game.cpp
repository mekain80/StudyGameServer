#include "stdafx.h"

#include <unordered_set>

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
	constexpr double kServerFrameSeconds = 0.02;
	std::unordered_set<Character*> gMovingCharacters;
	std::unordered_set<Character*> gPendingDeadCharacters;
	ULONGLONG gLastTimeoutCheckTick = 0;

	double GetElapsedSeconds(LONGLONG previousTick, LONGLONG currentTick) noexcept
	{
		if (currentTick <= previousTick || gFreq.QuadPart <= 0)
		{
			return 0.0;
		}

		return static_cast<double>(currentTick - previousTick) / static_cast<double>(gFreq.QuadPart);
	}

	bool IsTrackedMoveAction(BYTE action) noexcept
	{
		switch (action)
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

	void ProcessPendingDeadCharacters() noexcept
	{
		for (auto it = gPendingDeadCharacters.begin(); it != gPendingDeadCharacters.end();)
		{
			Character* character = *it;
			auto currentIt = it++;
			gPendingDeadCharacters.erase(currentIt);

			if (character == nullptr)
			{
				continue;
			}

			gMovingCharacters.erase(character);

			Session* session = FindActiveSession(character);
			if (session == nullptr)
			{
				continue;
			}

			Disconnect(session, L"HP 0 이하");
		}
	}

	void ProcessTimeoutSessions(ULONGLONG currentTick) noexcept
	{
		const ULONGLONG timeoutTick = static_cast<ULONGLONG>(NETWORK_PACKET_RECV_TIMEOUT) * 1000;
		for (auto it = gSessionMap.begin(); it != gSessionMap.end();)
		{
			Session* session = it->second;
			++it;

			if (session == nullptr || session->disconnectFlag)
			{
				continue;
			}

			if (currentTick - session->lastRecvTime >= timeoutTick)
			{
				Disconnect(session, L"recv timeout");
			}
		}
	}

	void SendDeleteCharacterPacket(Session* session, DWORD sessionID) noexcept
	{
		SerializedBuffer packet(dfPACKET_HEADER_SIZE + dfPACKET_SC_DELETE_CHARACTER_SIZE);
		MakePacket_DeleteCharacter(&packet, sessionID);
		SendUnicast(session, &packet);
	}

	void SendCreateCharacterPacket(Session* session, const Character* character) noexcept
	{
		SerializedBuffer packet(dfPACKET_HEADER_SIZE + dfPACKET_SC_CREATE_OTHER_CHARACTER_SIZE);
		MakePacket_CreateOtherCharacter(
			&packet,
			character->direction,
			character->sessionID,
			character->x,
			character->y,
			character->HP);
		SendUnicast(session, &packet);
	}

	void SendMoveStartPacket(Session* session, const Character* character) noexcept
	{
		if (character->action == dfACTION_STOP)
		{
			return;
		}

		SerializedBuffer packet(dfPACKET_HEADER_SIZE + dfPACKET_SC_MOVE_START_SIZE);
		MakePacket_MoveStart(
			&packet,
			character->sessionID,
			character->action,
			character->x,
			character->y);
		SendUnicast(session, &packet);
	}

	void BroadcastCharacterLeave(const SectorAround& removeAround, DWORD sessionID) noexcept
	{
		SerializedBuffer packet(dfPACKET_HEADER_SIZE + dfPACKET_SC_DELETE_CHARACTER_SIZE);
		MakePacket_DeleteCharacter(&packet, sessionID);
		SendPacket_BySectorAround(removeAround, &packet);
	}

	void SyncRemovedCharactersToCurrentSession(const SectorAround& removeAround, const Character* movingCharacter, Session* currentSession) noexcept
	{
		for (int index = 0; index < removeAround.count; ++index)
		{
			const SectorPos sectorPos = removeAround.around[index];
			std::list<Character*>& sectorList = gSector[sectorPos.y][sectorPos.x];
			for (Character* character : sectorList)
			{
				if (character == movingCharacter || FindActiveSession(character) == nullptr)
				{
					continue;
				}

				SendDeleteCharacterPacket(currentSession, character->sessionID);
			}
		}
	}

	void BroadcastCharacterEnter(const SectorAround& addAround, const Character* movingCharacter) noexcept
	{
		SerializedBuffer createPacket(dfPACKET_HEADER_SIZE + dfPACKET_SC_CREATE_OTHER_CHARACTER_SIZE);
		MakePacket_CreateOtherCharacter(
			&createPacket,
			movingCharacter->direction,
			movingCharacter->sessionID,
			movingCharacter->x,
			movingCharacter->y,
			movingCharacter->HP);
		SendPacket_BySectorAround(addAround, &createPacket);

		if (movingCharacter->action == dfACTION_STOP)
		{
			return;
		}

		SerializedBuffer movePacket(dfPACKET_HEADER_SIZE + dfPACKET_SC_MOVE_START_SIZE);
		MakePacket_MoveStart(
			&movePacket,
			movingCharacter->sessionID,
			movingCharacter->action,
			movingCharacter->x,
			movingCharacter->y);
		SendPacket_BySectorAround(addAround, &movePacket);
	}

	void SyncAddedCharactersToCurrentSession(const SectorAround& addAround, const Character* movingCharacter, Session* currentSession) noexcept
	{
		for (int index = 0; index < addAround.count; ++index)
		{
			const SectorPos sectorPos = addAround.around[index];
			std::list<Character*>& sectorList = gSector[sectorPos.y][sectorPos.x];
			for (Character* character : sectorList)
			{
				if (character == movingCharacter || FindActiveSession(character) == nullptr)
				{
					continue;
				}

				SendCreateCharacterPacket(currentSession, character);
				SendMoveStartPacket(currentSession, character);
			}
		}
	}
}

LONGLONG GetCurrentMoveTick() noexcept
{
	LARGE_INTEGER currentTick{};
	QueryPerformanceCounter(&currentTick);
	return currentTick.QuadPart;
}

void SetCharacterPosition(Character* character, int x, int y, LONGLONG currentTick) noexcept
{
	if (character == nullptr)
	{
		return;
	}

	character->x = x;
	character->y = y;
	character->lastMoveTick = currentTick;
	character->moveTimeRemainder = 0.0;
}

void RefreshCharacterMoveTracking(Character* character) noexcept
{
	if (character == nullptr)
	{
		return;
	}

	if (IsTrackedMoveAction(character->action))
	{
		gMovingCharacters.insert(character);
		return;
	}

	gMovingCharacters.erase(character);
}

void RemoveCharacterFromUpdateTracking(Character* character) noexcept
{
	if (character == nullptr)
	{
		return;
	}

	gMovingCharacters.erase(character);
	gPendingDeadCharacters.erase(character);
}

void MarkCharacterDead(Character* character) noexcept
{
	if (character == nullptr)
	{
		return;
	}

	gPendingDeadCharacters.insert(character);
}

bool AdvanceCharacterByTime(Character* character, LONGLONG currentTick) noexcept
{
	if (character == nullptr)
	{
		return false;
	}

	if (character->lastMoveTick == 0)
	{
		SetCharacterPosition(character, character->x, character->y, currentTick);
		return false;
	}

	const double elapsedSeconds = GetElapsedSeconds(character->lastMoveTick, currentTick);
	character->lastMoveTick = currentTick;

	if (elapsedSeconds <= 0.0)
	{
		return false;
	}

	if (character->action == dfACTION_STOP)
	{
		character->moveTimeRemainder = 0.0;
		return false;
	}

	character->moveTimeRemainder += elapsedSeconds;

	int dx = 0;
	int dy = 0;
	GetMoveDelta(character->action, dx, dy);
	if (dx == 0 && dy == 0)
	{
		character->moveTimeRemainder = 0.0;
		return false;
	}

	bool moved = false;
	while (character->moveTimeRemainder >= kServerFrameSeconds)
	{
		if (!MoveCheck(character->action, character->x, character->y))
		{
			// 경계에서 이동이 막히면 누적 시간을 비워서 다음 프레임에 과도한 보정이 생기지 않게 한다.
			character->moveTimeRemainder = 0.0;
			break;
		}

		character->x += dx;
		character->y += dy;
		character->moveTimeRemainder -= kServerFrameSeconds;
		moved = true;
	}

	return moved;
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

bool Update() noexcept
{
    QueryPerformanceCounter(&gFrameEndTick);
    const double elapsed = GetElapsedSeconds(gFrameStartTick.QuadPart, gFrameEndTick.QuadPart);

    if (elapsed <= kServerFrameSeconds)
    {
        return false;
    }

    gFrameStartTick = gFrameEndTick;
    const LONGLONG currentMoveTick = gFrameEndTick.QuadPart;

	ProcessPendingDeadCharacters();

	const ULONGLONG currentSystemTick = GetTickCount64();
	if (gLastTimeoutCheckTick == 0 || currentSystemTick - gLastTimeoutCheckTick >= 1000)
	{
		gLastTimeoutCheckTick = currentSystemTick;
		ProcessTimeoutSessions(currentSystemTick);
	}

	for (auto it = gMovingCharacters.begin(); it != gMovingCharacters.end();)
	{
		Character* pCharacter = *it;
		auto currentIt = it++;

		if (pCharacter == nullptr || !IsTrackedMoveAction(pCharacter->action))
		{
			gMovingCharacters.erase(currentIt);
			continue;
		}

		Session* currentSession = FindActiveSession(pCharacter);
		if (currentSession == nullptr)
		{
			gMovingCharacters.erase(currentIt);
			continue;
		}

		if (AdvanceCharacterByTime(pCharacter, currentMoveTick))
		{
			UpdateCharacterSector(pCharacter, currentSession);
		}
	}

    return true;
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
