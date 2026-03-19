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

#include <deque>
#include <vector>

namespace
{
	struct PendingDamageEvent
	{
		explicit PendingDamageEvent(Character* targetCharacter)
			: target(targetCharacter)
			, packet(dfPACKET_HEADER_SIZE + dfPACKET_SC_DAMAGE_SIZE)
		{
		}

		Character* target = nullptr;
		SerializedBuffer packet;
	};

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

	bool IsCharacterVisibleBySector(const Character* observer, const Character* target) noexcept
	{
		if (observer == nullptr || target == nullptr)
		{
			return false;
		}

		return std::abs(observer->sector.x - target->sector.x) <= 1 &&
			std::abs(observer->sector.y - target->sector.y) <= 1;
	}

	void BroadcastPendingDamageEvents(const std::deque<PendingDamageEvent>& damageEvents) noexcept
	{
		if (damageEvents.empty())
		{
			return;
		}

		bool visitedSector[dfSECTOR_MAX_Y][dfSECTOR_MAX_X]{};
		std::vector<SectorPos> sectorsToVisit;
		sectorsToVisit.reserve(damageEvents.size() * 9);

		for (const PendingDamageEvent& damageEvent : damageEvents)
		{
			SectorAround targetAround{};
			GetSectorAroundBySector(&damageEvent.target->sector, &targetAround);
			for (int sectorIndex = 0; sectorIndex < targetAround.count; ++sectorIndex)
			{
				const SectorPos sectorPos = targetAround.around[sectorIndex];
				if (visitedSector[sectorPos.y][sectorPos.x])
				{
					continue;
				}

				visitedSector[sectorPos.y][sectorPos.x] = true;
				sectorsToVisit.push_back(sectorPos);
			}
		}

		for (const SectorPos& sectorPos : sectorsToVisit)
		{
			SectorCharacterList& sectorList = gSector[sectorPos.y][sectorPos.x];
			for (Character* observer : sectorList)
			{
				Session* observerSession = FindActiveSession(observer);
				if (observerSession == nullptr)
				{
					continue;
				}

				for (const PendingDamageEvent& damageEvent : damageEvents)
				{
					if (!IsCharacterVisibleBySector(observer, damageEvent.target))
					{
						continue;
					}

					SendUnicast(observerSession, &damageEvent.packet);
				}
			}
		}
	}

	template <typename HitPredicate>
	void ApplyAttackDamage(
		Character* attacker,
		int centerX,
		int centerY,
		int damage,
		HitPredicate&& isHit) noexcept
	{
		SectorAround attackerAround{};
		GetSectorAroundBySector(&attacker->sector, &attackerAround);

		std::deque<PendingDamageEvent> damageEvents;
		for (int sectorIndex = 0; sectorIndex < attackerAround.count; ++sectorIndex)
		{
			const SectorPos sectorPos = attackerAround.around[sectorIndex];
			SectorCharacterList& sectorList = gSector[sectorPos.y][sectorPos.x];
			for (Character* target : sectorList)
			{
				if (target == attacker)
				{
					continue;
				}

				if (FindActiveSession(target) == nullptr)
				{
					continue;
				}

				if (!isHit(target, centerX, centerY))
				{
					continue;
				}

				target->HP -= damage;
				if (target->HP < 0)
				{
					target->HP = 0;
				}

				damageEvents.emplace_back(target);
				PendingDamageEvent& damageEvent = damageEvents.back();
				MakePacket_Damage(
					&damageEvent.packet,
					attacker->sessionID,
					target->sessionID,
					static_cast<BYTE>(target->HP));

				if (target->HP <= 0)
				{
					MarkCharacterDead(target);
				}
			}
		}

		BroadcastPendingDamageEvents(damageEvents);
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

	static SerializedBuffer sendMsg(dfPACKET_HEADER_SIZE + dfPACKET_SC_MOVE_START_SIZE);
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

	static SerializedBuffer sendMsg(dfPACKET_HEADER_SIZE + dfPACKET_SC_MOVE_STOP_SIZE);
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

	static SerializedBuffer sendMsg(dfPACKET_HEADER_SIZE + dfPACKET_SC_ATTACK1_SIZE);
	MakePacket_Attack1(&sendMsg, pCharacter->direction, pCharacter->sessionID, pCharacter->x, pCharacter->y);
	SendPacket_AroundCharacter(pCharacter, &sendMsg);

	const int centerX = pCharacter->x;
	const int centerY = pCharacter->y;
	ApplyAttackDamage(
		pCharacter,
		centerX,
		centerY,
		dfATTACK1_DAMAGE,
		[pCharacter](Character* target, int attackCenterX, int attackCenterY) noexcept
		{
			return IsHitAttack1(pCharacter, target, attackCenterX, attackCenterY);
		});

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

	static SerializedBuffer sendMsg(dfPACKET_HEADER_SIZE + dfPACKET_SC_ATTACK2_SIZE);
	MakePacket_Attack2(&sendMsg, pCharacter->direction, pCharacter->sessionID, pCharacter->x, pCharacter->y);
	SendPacket_AroundCharacter(pCharacter, &sendMsg);

	const int centerX = pCharacter->x;
	const int centerY = pCharacter->y;
	ApplyAttackDamage(
		pCharacter,
		centerX,
		centerY,
		dfATTACK2_DAMAGE,
		[pCharacter](Character* target, int attackCenterX, int attackCenterY) noexcept
		{
			return IsHitDirectionalAttack(
				pCharacter,
				target,
				attackCenterX,
				attackCenterY,
				dfATTACK2_RANGE_X,
				dfATTACK2_RANGE_Y);
		});

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

	static SerializedBuffer sendMsg(dfPACKET_HEADER_SIZE + dfPACKET_SC_ATTACK3_SIZE);
	MakePacket_Attack3(&sendMsg, pCharacter->direction, pCharacter->sessionID, pCharacter->x, pCharacter->y);
	SendPacket_AroundCharacter(pCharacter, &sendMsg);

	const int centerX = pCharacter->x;
	const int centerY = pCharacter->y;
	ApplyAttackDamage(
		pCharacter,
		centerX,
		centerY,
		dfATTACK3_DAMAGE,
		[pCharacter](Character* target, int attackCenterX, int attackCenterY) noexcept
		{
			return IsHitDirectionalAttack(
				pCharacter,
				target,
				attackCenterX,
				attackCenterY,
				dfATTACK3_RANGE_X,
				dfATTACK3_RANGE_Y);
		});

	return true;
}

bool NetPacketProc_Echo(Session* pSession, const char* packetData, WORD packetSize)
{
	DWORD time = 0;
	if (!ReadEchoPacket(pSession, packetData, packetSize, time))
	{
		return false;
	}

	static SerializedBuffer response(dfPACKET_HEADER_SIZE + dfPACKET_SC_ECHO_SIZE);
	MakePacket_Echo(&response, time);
	SendUnicast(pSession, &response);

	return true;
}
