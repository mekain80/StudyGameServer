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

bool PacketProc(Session* pSession, BYTE byPacketType, char* pPacket, WORD packetSize)
{
	SerializedBuffer packet(static_cast<int>(packetSize));
	int putRet = packet.PutData(pPacket, packetSize);
	if (putRet != packetSize)
	{
		Disconnect(pSession, L"SerializedBuffer PutData fail");
		return false;
	}

	switch (byPacketType)
	{
	case dfPACKET_CS_MOVE_START:
		return NetPacketProc_MoveStart(pSession, packet);
	case dfPACKET_CS_MOVE_STOP:
		return NetPacketProc_MoveStop(pSession, packet);
	case dfPACKET_CS_ATTACK1:
		return NetPacketProc_Attack1(pSession, packet);
	case dfPACKET_CS_ATTACK2:
		return NetPacketProc_Attack2(pSession, packet);
	case dfPACKET_CS_ATTACK3:
		return NetPacketProc_Attack3(pSession, packet);
	case dfPACKET_CS_ECHO:
		return NetPacketProc_Echo(pSession, packet);
	default:
		return true;
	}
}

bool NetPacketProc_MoveStart(Session* pSession, SerializedBuffer& packet)
{
	BYTE direction;
	WORD x, y;

	packet >> direction >> x >> y;

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

	// 서버의 위치와 받은 패킷의 위치값이 너무 큰 차이가 난다면 싱크 패킷을 보내어 좌표 보정.
	if (NeedSync(x, y, pCharacter))
	{
		SendSync(pSession, pCharacter);
		return true; // 세션 유지, 이번 입력만 무시
	}

	// 입력이 유효하면 클라이언트 기준 최신 좌표를 서버 상태에도 반영한다.
	pCharacter->x = x;
	pCharacter->y = y;
	pCharacter->action = direction;
	pCharacter->direction = NormalizeViewDir(direction, pCharacter->direction);
	UpdateCharacterSector(pCharacter, pSession);

	SerializedBuffer sendMsg(dfPACKET_HEADER_SIZE + dfPACKET_SC_MOVE_START_SIZE);
	MakePacket_MoveStart(
		&sendMsg,
		pCharacter->sessionID,
		pCharacter->action, 
		pCharacter->x, 
		pCharacter->y);
	SendPacket_Around(pSession, &sendMsg);

	return true;
}

bool NetPacketProc_MoveStop(Session* pSession, SerializedBuffer& packet)
{
	BYTE direction = 0;
	WORD x = 0;
	WORD y = 0;

	packet >> direction
		>> x
		>> y;

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

	// 서버의 위치와 받은 패킷의 위치값이 너무 큰 차이가 난다면 싱크 패킷을 보내어 좌표 보정.
	if (NeedSync(x, y, pCharacter))
	{
		SendSync(pSession, pCharacter);
		return true; // 세션 유지, 이번 입력만 무시
	}

	pCharacter->action = dfACTION_STOP;
	pCharacter->direction = NormalizeViewDir(direction, pCharacter->direction);
	pCharacter->x = x;
	pCharacter->y = y;
	UpdateCharacterSector(pCharacter, pSession);

	SerializedBuffer sendMsg(dfPACKET_HEADER_SIZE + dfPACKET_SC_MOVE_STOP_SIZE);
	MakePacket_MoveStop(&sendMsg, pCharacter->sessionID, pCharacter->direction, pCharacter->x, pCharacter->y);
	SendPacket_Around(pSession, &sendMsg);

	return true;
}

bool NetPacketProc_Attack1(Session* pSession, SerializedBuffer& packet)
{
	BYTE direction = 0;
	WORD x = 0;
	WORD y = 0;
	packet >> direction
		>> x
		>> y;

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

	if (NeedSync(x, y, pCharacter))
	{
		SendSync(pSession, pCharacter);
		return true;
	}

	pCharacter->x = x;
	pCharacter->y = y;
	pCharacter->direction = NormalizeViewDir(direction, pCharacter->direction);
	UpdateCharacterSector(pCharacter, pSession);

	SerializedBuffer sendMsg(dfPACKET_HEADER_SIZE + dfPACKET_SC_ATTACK1_SIZE);
	MakePacket_Attack1(&sendMsg, pCharacter->direction, pCharacter->sessionID, pCharacter->x, pCharacter->y);
	SendPacket_Around(pSession, &sendMsg, true);

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
			if (character == pCharacter || !IsCharacterActive(character))
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

			Session* targetSession = FindSession(character->sessionID);
			MakePacket_Damage(&dmgMsg, pCharacter->sessionID, character->sessionID, static_cast<BYTE>(character->HP));
			SendPacket_Around(targetSession, &dmgMsg, true);
		}
	}

	return true;
}

bool NetPacketProc_Attack2(Session* pSession, SerializedBuffer& packet)
{
	BYTE direction = 0;
	WORD x = 0;
	WORD y = 0;
	packet >> direction
		>> x
		>> y;

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

	if (NeedSync(x, y, pCharacter))
	{
		SendSync(pSession, pCharacter);
		return true;
	}

	pCharacter->x = x;
	pCharacter->y = y;
	pCharacter->direction = NormalizeViewDir(direction, pCharacter->direction);
	UpdateCharacterSector(pCharacter, pSession);

	SerializedBuffer sendMsg(dfPACKET_HEADER_SIZE + dfPACKET_SC_ATTACK2_SIZE);
	MakePacket_Attack2(&sendMsg, pCharacter->direction, pCharacter->sessionID, pCharacter->x, pCharacter->y);
	SendPacket_Around(pSession, &sendMsg, true);

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
			if (character == pCharacter || !IsCharacterActive(character))
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

			Session* targetSession = FindSession(character->sessionID);
			MakePacket_Damage(&dmgMsg, pCharacter->sessionID, character->sessionID, static_cast<BYTE>(character->HP));
			SendPacket_Around(targetSession, &dmgMsg, true);
		}
	}

	return true;
}

bool NetPacketProc_Attack3(Session* pSession, SerializedBuffer& packet)
{
	BYTE direction = 0;
	WORD x = 0;
	WORD y = 0;
	packet >> direction
		>> x
		>> y;

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

	if (NeedSync(x, y, pCharacter))
	{
		SendSync(pSession, pCharacter);
		return true;
	}

	pCharacter->x = x;
	pCharacter->y = y;
	pCharacter->direction = NormalizeViewDir(direction, pCharacter->direction);
	UpdateCharacterSector(pCharacter, pSession);

	SerializedBuffer sendMsg(dfPACKET_HEADER_SIZE + dfPACKET_SC_ATTACK3_SIZE);
	MakePacket_Attack3(&sendMsg, pCharacter->direction, pCharacter->sessionID, pCharacter->x, pCharacter->y);
	SendPacket_Around(pSession, &sendMsg, true);

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
			if (character == pCharacter || !IsCharacterActive(character))
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

			Session* targetSession = FindSession(character->sessionID);
			MakePacket_Damage(&dmgMsg, pCharacter->sessionID, character->sessionID, static_cast<BYTE>(character->HP));
			SendPacket_Around(targetSession, &dmgMsg, true);
		}
	}

	return true;
}

bool NetPacketProc_Echo(Session* pSession, SerializedBuffer& packet)
{
	DWORD time = 0;
	packet >> time;

	SerializedBuffer response(dfPACKET_HEADER_SIZE + dfPACKET_SC_ECHO_SIZE);
	MakePacket_Echo(&response, time);
	SendUnicast(pSession, &response);

	return true;
}
