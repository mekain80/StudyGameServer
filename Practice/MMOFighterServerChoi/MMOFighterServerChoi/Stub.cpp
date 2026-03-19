#include "stdafx.h"

#include "Stub.h"

#include "Game.h"
#include "GameInfo.h"
#include "Log.h"
#include "Network.h"
#include "PacketControl.h"
#include "Protocol.h"
#include "Proxy.h"
#include "Character.h"
#include "Sector.h"

namespace
{
	template <typename T>
	bool ReadPacketValue(const char*& cursor, const char* end, T& outValue) noexcept
	{
		if (cursor == nullptr || end == nullptr)
		{
			return false;
		}

		const ptrdiff_t remain = end - cursor;
		if (remain < 0 || static_cast<std::size_t>(remain) < sizeof(T))
		{
			return false;
		}

		std::memcpy(&outValue, cursor, sizeof(T));
		cursor += sizeof(T);
		return true;
	}

	bool ReadDirectionPositionPacket(
		Session* session,
		const char* packetData,
		WORD packetSize,
		BYTE& direction,
		WORD& x,
		WORD& y,
		const WCHAR* failReason) noexcept
	{
		const char* cursor = packetData;
		const char* const end = packetData + packetSize;

		if (!ReadPacketValue(cursor, end, direction) ||
			!ReadPacketValue(cursor, end, x) ||
			!ReadPacketValue(cursor, end, y) ||
			cursor != end)
		{
			Disconnect(session, failReason);
			return false;
		}

		return true;
	}

	bool ReadEchoPacket(Session* session, const char* packetData, WORD packetSize, DWORD& time) noexcept
	{
		const char* cursor = packetData;
		const char* const end = packetData + packetSize;
		if (!ReadPacketValue(cursor, end, time) || cursor != end)
		{
			Disconnect(session, L"invalid echo packet body");
			return false;
		}

		return true;
	}

	void AdvanceCharacterState(Character* character, Session* session, LONGLONG currentTick) noexcept
	{
		if (AdvanceCharacterByTime(character, currentTick))
		{
			UpdateCharacterSector(character, session);
		}
	}
}

bool PacketProc(Session* pSession, BYTE byPacketType, const char* packetData, WORD packetSize)
{
	switch (byPacketType)
	{
	case dfPACKET_CS_MOVE_START:
		return NetPacketProc_MoveStart(pSession, packetData, packetSize);
	case dfPACKET_CS_MOVE_STOP:
		return NetPacketProc_MoveStop(pSession, packetData, packetSize);
	case dfPACKET_CS_ATTACK1:
		return NetPacketProc_Attack1(pSession, packetData, packetSize);
	case dfPACKET_CS_ATTACK2:
		return NetPacketProc_Attack2(pSession, packetData, packetSize);
	case dfPACKET_CS_ATTACK3:
		return NetPacketProc_Attack3(pSession, packetData, packetSize);
	case dfPACKET_CS_ECHO:
		return NetPacketProc_Echo(pSession, packetData, packetSize);
	default:
		return true;
	}
}

bool NetPacketProc_MoveStart(Session* pSession, const char* packetData, WORD packetSize)
{
	BYTE direction = 0;
	WORD x = 0;
	WORD y = 0;
	if (!ReadDirectionPositionPacket(
		pSession,
		packetData,
		packetSize,
		direction,
		x,
		y,
		L"invalid move start packet body"))
	{
		return false;
	}

	if (!IsValidMoveDirection(direction))
	{
		Disconnect(pSession, L"invalid move start direction");
		return false;
	}

	// ID 로 캐릭터를 검색한다.
	Character* pCharacter = FindCharacter(pSession->sessionID);
	if (pCharacter == NULL)
	{
		return false;
	}

	const LONGLONG currentTick = GetCurrentMoveTick();
	AdvanceCharacterState(pCharacter, pSession, currentTick);

	// 서버의 위치와 받은 패킷의 위치값이 너무 큰 차이가 난다면 싱크 패킷을 보내어 좌표 보정.
	if (NeedSync(x, y, pCharacter))
	{
		SendSync(pSession, pCharacter);
		return true; // 세션 유지, 이번 입력만 무시
	}

	// 입력이 유효하면 클라이언트 기준 최신 좌표를 서버 상태에도 반영한다.
	SetCharacterPosition(pCharacter, x, y, currentTick);
	pCharacter->action = direction;
	RefreshCharacterMoveTracking(pCharacter);
	pCharacter->direction = NormalizeViewDir(direction, pCharacter->direction);
	UpdateCharacterSector(pCharacter, pSession);

	SerializedBuffer sendMsg(dfPACKET_HEADER_SIZE + dfPACKET_SC_MOVE_START_SIZE);
	MakePacket_MoveStart(
		&sendMsg,
		pCharacter->sessionID,
		pCharacter->action, 
		pCharacter->x, 
		pCharacter->y);
	SendPacket_AroundCharacter(pCharacter, &sendMsg, pSession);

	return true;
}

bool NetPacketProc_MoveStop(Session* pSession, const char* packetData, WORD packetSize)
{
	BYTE direction = 0;
	WORD x = 0;
	WORD y = 0;
	if (!ReadDirectionPositionPacket(
		pSession,
		packetData,
		packetSize,
		direction,
		x,
		y,
		L"invalid move stop packet body"))
	{
		return false;
	}

	if (!IsValidViewDirection(direction))
	{
		Disconnect(pSession, L"invalid move stop direction");
		return false;
	}

	Character* pCharacter = FindCharacter(pSession->sessionID);
	if (pCharacter == nullptr)
	{
		return false;
	}

	const LONGLONG currentTick = GetCurrentMoveTick();
	AdvanceCharacterState(pCharacter, pSession, currentTick);

	// 서버의 위치와 받은 패킷의 위치값이 너무 큰 차이가 난다면 싱크 패킷을 보내어 좌표 보정.
	if (NeedSync(x, y, pCharacter))
	{
		SendSync(pSession, pCharacter);
		return true; // 세션 유지, 이번 입력만 무시
	}

	pCharacter->action = dfACTION_STOP;
	RefreshCharacterMoveTracking(pCharacter);
	pCharacter->direction = NormalizeViewDir(direction, pCharacter->direction);
	SetCharacterPosition(pCharacter, x, y, currentTick);
	UpdateCharacterSector(pCharacter, pSession);

	SerializedBuffer sendMsg(dfPACKET_HEADER_SIZE + dfPACKET_SC_MOVE_STOP_SIZE);
	MakePacket_MoveStop(&sendMsg, pCharacter->sessionID, pCharacter->direction, pCharacter->x, pCharacter->y);
	SendPacket_AroundCharacter(pCharacter, &sendMsg, pSession);

	return true;
}

bool NetPacketProc_Attack1(Session* pSession, const char* packetData, WORD packetSize)
{
	BYTE direction = 0;
	WORD x = 0;
	WORD y = 0;
	if (!ReadDirectionPositionPacket(
		pSession,
		packetData,
		packetSize,
		direction,
		x,
		y,
		L"invalid attack1 packet body"))
	{
		return false;
	}

	if (!IsValidViewDirection(direction))
	{
		Disconnect(pSession, L"invalid attack1 direction");
		return false;
	}

	Character* pCharacter = FindCharacter(pSession->sessionID);
	if (pCharacter == nullptr)
	{
		return false;
	}

	const LONGLONG currentTick = GetCurrentMoveTick();
	AdvanceCharacterState(pCharacter, pSession, currentTick);

	if (NeedSync(x, y, pCharacter))
	{
		SendSync(pSession, pCharacter);
		return true;
	}

	SetCharacterPosition(pCharacter, x, y, currentTick);
	pCharacter->direction = NormalizeViewDir(direction, pCharacter->direction);
	UpdateCharacterSector(pCharacter, pSession);

	SerializedBuffer sendMsg(dfPACKET_HEADER_SIZE + dfPACKET_SC_ATTACK1_SIZE);
	MakePacket_Attack1(&sendMsg, pCharacter->direction, pCharacter->sessionID, pCharacter->x, pCharacter->y);
	SendPacket_AroundCharacter(pCharacter, &sendMsg);

	const int centerX = pCharacter->x;
	const int centerY = pCharacter->y;

	SectorAround secAround{};
	GetSectorAroundBySector(&pCharacter->sector, &secAround);
	for (int secIdx = 0; secIdx < secAround.count; secIdx++)
	{
		SectorPos sectorPos = secAround.around[secIdx];
		std::list<Character*>& sectorList = gSector[sectorPos.y][sectorPos.x];
		for (Character* character : sectorList)
		{
			if (character == pCharacter)
			{
				continue;
			}

			Session* targetSession = FindActiveSession(character);
			if (targetSession == nullptr)
			{
				continue;
			}

			if (!IsHitAttack1(pCharacter, character, centerX, centerY))
			{
				continue;
			}

			SerializedBuffer dmgMsg(dfPACKET_HEADER_SIZE + dfPACKET_SC_DAMAGE_SIZE);

			character->HP -= dfATTACK1_DAMAGE;
			if (character->HP < 0)
			{
				character->HP = 0;
			}

			MakePacket_Damage(&dmgMsg, pCharacter->sessionID, character->sessionID, static_cast<BYTE>(character->HP));
			SendPacket_AroundCharacter(character, &dmgMsg);
			if (character->HP <= 0)
			{
				MarkCharacterDead(character);
			}
		}
	}

	return true;
}

bool NetPacketProc_Attack2(Session* pSession, const char* packetData, WORD packetSize)
{
	BYTE direction = 0;
	WORD x = 0;
	WORD y = 0;
	if (!ReadDirectionPositionPacket(
		pSession,
		packetData,
		packetSize,
		direction,
		x,
		y,
		L"invalid attack2 packet body"))
	{
		return false;
	}

	if (!IsValidViewDirection(direction))
	{
		Disconnect(pSession, L"invalid attack2 direction");
		return false;
	}

	Character* pCharacter = FindCharacter(pSession->sessionID);
	if (pCharacter == nullptr)
	{
		return false;
	}

	const LONGLONG currentTick = GetCurrentMoveTick();
	AdvanceCharacterState(pCharacter, pSession, currentTick);

	if (NeedSync(x, y, pCharacter))
	{
		SendSync(pSession, pCharacter);
		return true;
	}

	SetCharacterPosition(pCharacter, x, y, currentTick);
	pCharacter->direction = NormalizeViewDir(direction, pCharacter->direction);
	UpdateCharacterSector(pCharacter, pSession);

	SerializedBuffer sendMsg(dfPACKET_HEADER_SIZE + dfPACKET_SC_ATTACK2_SIZE);
	MakePacket_Attack2(&sendMsg, pCharacter->direction, pCharacter->sessionID, pCharacter->x, pCharacter->y);
	SendPacket_AroundCharacter(pCharacter, &sendMsg);

	const int centerX = pCharacter->x;
	const int centerY = pCharacter->y;

	SectorAround secAround{};
	GetSectorAroundBySector(&pCharacter->sector, &secAround);
	for (int secIdx = 0; secIdx < secAround.count; secIdx++)
	{
		SectorPos sectorPos = secAround.around[secIdx];
		std::list<Character*>& sectorList = gSector[sectorPos.y][sectorPos.x];
		for (Character* character : sectorList)
		{
			if (character == pCharacter)
			{
				continue;
			}

			Session* targetSession = FindActiveSession(character);
			if (targetSession == nullptr)
			{
				continue;
			}

			if (!IsHitDirectionalAttack(pCharacter, character, centerX, centerY, dfATTACK2_RANGE_X, dfATTACK2_RANGE_Y))
			{
				continue;
			}

			SerializedBuffer dmgMsg(dfPACKET_HEADER_SIZE + dfPACKET_SC_DAMAGE_SIZE);

			character->HP -= dfATTACK2_DAMAGE;
			if (character->HP < 0)
			{
				character->HP = 0;
			}

			MakePacket_Damage(&dmgMsg, pCharacter->sessionID, character->sessionID, static_cast<BYTE>(character->HP));
			SendPacket_AroundCharacter(character, &dmgMsg);
			if (character->HP <= 0)
			{
				MarkCharacterDead(character);
			}
		}
	}

	return true;
}

bool NetPacketProc_Attack3(Session* pSession, const char* packetData, WORD packetSize)
{
	BYTE direction = 0;
	WORD x = 0;
	WORD y = 0;
	if (!ReadDirectionPositionPacket(
		pSession,
		packetData,
		packetSize,
		direction,
		x,
		y,
		L"invalid attack3 packet body"))
	{
		return false;
	}

	if (!IsValidViewDirection(direction))
	{
		Disconnect(pSession, L"invalid attack3 direction");
		return false;
	}

	Character* pCharacter = FindCharacter(pSession->sessionID);
	if (pCharacter == nullptr)
	{
		return false;
	}

	const LONGLONG currentTick = GetCurrentMoveTick();
	AdvanceCharacterState(pCharacter, pSession, currentTick);

	if (NeedSync(x, y, pCharacter))
	{
		SendSync(pSession, pCharacter);
		return true;
	}

	SetCharacterPosition(pCharacter, x, y, currentTick);
	pCharacter->direction = NormalizeViewDir(direction, pCharacter->direction);
	UpdateCharacterSector(pCharacter, pSession);

	SerializedBuffer sendMsg(dfPACKET_HEADER_SIZE + dfPACKET_SC_ATTACK3_SIZE);
	MakePacket_Attack3(&sendMsg, pCharacter->direction, pCharacter->sessionID, pCharacter->x, pCharacter->y);
	SendPacket_AroundCharacter(pCharacter, &sendMsg);

	const int centerX = pCharacter->x;
	const int centerY = pCharacter->y;

	SectorAround secAround{};
	GetSectorAroundBySector(&pCharacter->sector, &secAround);
	for (int secIdx = 0; secIdx < secAround.count; secIdx++)
	{
		SectorPos sectorPos = secAround.around[secIdx];
		std::list<Character*>& sectorList = gSector[sectorPos.y][sectorPos.x];
		for (Character* character : sectorList)
		{
			if (character == pCharacter)
			{
				continue;
			}

			Session* targetSession = FindActiveSession(character);
			if (targetSession == nullptr)
			{
				continue;
			}

			if (!IsHitDirectionalAttack(pCharacter, character, centerX, centerY, dfATTACK3_RANGE_X, dfATTACK3_RANGE_Y))
			{
				continue;
			}

			SerializedBuffer dmgMsg(dfPACKET_HEADER_SIZE + dfPACKET_SC_DAMAGE_SIZE);

			character->HP -= dfATTACK3_DAMAGE;
			if (character->HP < 0)
			{
				character->HP = 0;
			}

			MakePacket_Damage(&dmgMsg, pCharacter->sessionID, character->sessionID, static_cast<BYTE>(character->HP));
			SendPacket_AroundCharacter(character, &dmgMsg);
			if (character->HP <= 0)
			{
				MarkCharacterDead(character);
			}
		}
	}

	return true;
}

bool NetPacketProc_Echo(Session* pSession, const char* packetData, WORD packetSize)
{
	DWORD time = 0;
	if (!ReadEchoPacket(pSession, packetData, packetSize, time))
	{
		return false;
	}

	SerializedBuffer response(dfPACKET_HEADER_SIZE + dfPACKET_SC_ECHO_SIZE);
	MakePacket_Echo(&response, time);
	SendUnicast(pSession, &response);

	return true;
}
