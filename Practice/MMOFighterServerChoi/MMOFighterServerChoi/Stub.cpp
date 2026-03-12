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
		_LOG(LOG_LEVEL_ERROR, L"SerializedBuffer PutData fail");
		Disconnect(pSession);
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
	default:
		return true;
	}
}

bool NetPacketProc_MoveStart(Session* pSession, SerializedBuffer& packet)
{
	BYTE direction;
	WORD x, y;

	packet >> direction >> x >> y;

	_LOG(LOG_LEVEL_DEBUG, L"# PACKET_MOVESTART # SessionID:%u / Direction:%u / X:%d / Y:%d", pSession->sessionID, direction, x, y);

	if (!IsValidMoveDirection(direction))
	{
		Disconnect(pSession);
		return false;
	}

	// ID 로 캐릭터를 검색한다.
	Character* pCharacter = FindCharacter(pSession->sessionID);
	if (pCharacter == NULL)
	{
		_LOG(LOG_LEVEL_DEBUG, L"# MOVESTART > SessionID:%d Character Not Found!",
			pSession->sessionID);
		return false;
	}

	// 서버의 위치와 받은 패킷의 위치값이 너무 큰 차이가 난다면 싱크 패킷을 보내어 좌표 보정.
	if (NeedSync(x, y, pCharacter))
	{
		SendSync(pSession, pCharacter);
		_LOG(LOG_LEVEL_ERROR, L"SYNC_RANGE MoveStart ID=%d IP=%s", pSession->sessionID, pSession->ipStr);
		return true; // 세션 유지, 이번 입력만 무시
	}

	pCharacter->action = direction;
	pCharacter->direction = NormalizeViewDir(direction);
	// TODO 왜 Character의 x, y가 short인지
	pCharacter->x = static_cast<short>(x);
	pCharacter->y = static_cast<short>(y);


	// TODO) 패킷 뿌리는 것 섹터 기준으로 수정하기
	//// 정지를 하면서 좌표가 약간 조절된 경우섹터 업데이트를 함. (섹터는 차후 설명)
	////-----------------------------------------------------------------------
	//if (Sector_UpdateCharacter(pCharacter))
	//{
	//	//-----------------------------------------------------------------------
	//	// 섹터가 변경된 경우는 클라에게 관련 정보를 쏜다. (섹터는 차후 설명)
	//	//-----------------------------------------------------------------------
	//	CharacterSectorUpdatePacket(pCharacter);
	//}
	////-----------------------------------------------------------------------
	//mpMoveStart(pPacket, pSession->dwSessionID, byDirection, pCharacter->shX, pCharacter->shY);
	////-----------------------------------------------------
	//// 현재 접속중인 사용자에게 모든 패킷을 뿌린다. (섹터 단위 패킷 전송 함수 )
	////-----------------------------------------------------
	//SendPacket_Around(pSession, pPacket);

	PacketHeader packetHeader;
	PacketSCMoveStart sendMsg;
	MakePacket_MoveStart(&packetHeader, &sendMsg, pCharacter->sessionID, pCharacter->direction, pCharacter->x, pCharacter->y);
	SendPacket_Around(pSession, &packetHeader, reinterpret_cast<char*>(&sendMsg));

	return true;
}

bool NetPacketProc_MoveStop(Session* pSession, SerializedBuffer& packet)
{
	PacketCSMoveStop moveStop{};

	packet >> moveStop.direction
		>> moveStop.x
		>> moveStop.y;

	_LOG(LOG_LEVEL_DEBUG, L"# PACKET_MOVESTOP # SessionID:%u / Direction:%u / X:%d / Y:%d", pSession->sessionID, moveStop.direction, moveStop.x, moveStop.y);

	if (!IsValidViewDirection(moveStop.direction))
	{
		Disconnect(pSession);
		return false;
	}

	Character* pCharacter = FindCharacter(pSession->sessionID);
	if (pCharacter == nullptr)
	{
		_LOG(LOG_LEVEL_DEBUG, L"# MOVESTOP > SessionID:%d Character Not Found!", pSession->sessionID);
		return false;
	}

	// 서버의 위치와 받은 패킷의 위치값이 너무 큰 차이가 난다면 싱크 패킷을 보내어 좌표 보정.
	if (NeedSync(moveStop.x, moveStop.y, pCharacter))
	{
		SendSync(pSession, pCharacter);
		_LOG(LOG_LEVEL_ERROR, L"SYNC_RANGE MoveStop ID=%d IP=%s", pSession->sessionID, pSession->ipStr);
		return true; // 세션 유지, 이번 입력만 무시
	}

	pCharacter->action = dfACTION_STOP;
	pCharacter->direction = NormalizeViewDir(moveStop.direction);
	pCharacter->x = static_cast<short>(moveStop.x);
	pCharacter->y = static_cast<short>(moveStop.y);

	PacketHeader packetHeader;
	PacketSCMoveStop sendMsg;
	MakePacket_MoveStop(&packetHeader, &sendMsg, pCharacter->sessionID, pCharacter->direction, pCharacter->x, pCharacter->y);
	SendBroadcast(pSession, &packetHeader, reinterpret_cast<char*>(&sendMsg));

	return true;
}

bool NetPacketProc_Attack1(Session* pSession, SerializedBuffer& packet)
{
	PacketCSAttack1 atk{};
	packet >> atk.direction
		>> atk.x
		>> atk.y;

	_LOG(LOG_LEVEL_DEBUG, L"# PACKET_ATTACK1 # SessionID:%u / Direction:%u / X:%d / Y:%d", pSession->sessionID, atk.direction, atk.x, atk.y);

	if (!IsValidViewDirection(atk.direction))
	{
		Disconnect(pSession);
		return false;
	}

	Character* pCharacter = FindCharacter(pSession->sessionID);
	if (pCharacter == nullptr)
	{
		_LOG(LOG_LEVEL_DEBUG, L"# ATTACK1 > SessionID:%d Character Not Found!", pSession->sessionID);
		return false;
	}

	if (NeedSync(atk.x, atk.y, pCharacter))
	{
		SendSync(pSession, pCharacter);
		_LOG(LOG_LEVEL_ERROR, L"SYNC_RANGE Attack1 ID=%d IP=%s", pSession->sessionID, pSession->ipStr);
		return true;
	}

	pCharacter->direction = NormalizeViewDir(atk.direction);

	PacketHeader packetHeader;
	PacketSCAttack1 sendMsg;
	MakePacket_Attack1(&packetHeader, &sendMsg, pCharacter->direction, pCharacter->sessionID, pCharacter->x, pCharacter->y);
	SendPacket_Around(pSession, &packetHeader, reinterpret_cast<char*>(&sendMsg), true);

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

			if (!IsHitAttack1(pCharacter, character, centerX, centerY))
			{
				continue;
			}

			PacketHeader dmgHeader;
			PacketSCDamage dmgMsg;

			character->HP -= dfATTACK1_DAMAGE;
			if (character->HP < 0)
			{
				character->HP = 0;
			}

			Session* targetSession = FindSession(character->sessionID);
			MakePacket_Damage(&dmgHeader, &dmgMsg, pCharacter->sessionID, character->sessionID, static_cast<BYTE>(character->HP));
			SendPacket_Around(targetSession, &dmgHeader, reinterpret_cast<char*>(&dmgMsg));
		}
	}

	return true;
}

bool NetPacketProc_Attack2(Session* pSession, SerializedBuffer& packet)
{
	PacketCSAttack2 atk{};
	packet >> atk.direction
		>> atk.x
		>> atk.y;

	_LOG(LOG_LEVEL_DEBUG, L"# PACKET_ATTACK2 # SessionID:%u / Direction:%u / X:%d / Y:%d", pSession->sessionID, atk.direction, atk.x, atk.y);

	if (!IsValidViewDirection(atk.direction))
	{
		Disconnect(pSession);
		return false;
	}

	Character* pCharacter = FindCharacter(pSession->sessionID);
	if (pCharacter == nullptr)
	{
		_LOG(LOG_LEVEL_DEBUG, L"# ATTACK2 > SessionID:%d Character Not Found!", pSession->sessionID);
		return false;
	}

	if (NeedSync(atk.x, atk.y, pCharacter))
	{
		SendSync(pSession, pCharacter);
		_LOG(LOG_LEVEL_ERROR, L"SYNC_RANGE Attack2 ID=%d IP=%s", pSession->sessionID, pSession->ipStr);
		return true;
	}

	pCharacter->direction = NormalizeViewDir(atk.direction);

	PacketHeader packetHeader;
	PacketSCAttack2 sendMsg;
	MakePacket_Attack2(&packetHeader, &sendMsg, pCharacter->direction, pCharacter->sessionID, pCharacter->x, pCharacter->y);
	SendPacket_Around(pSession, &packetHeader, reinterpret_cast<char*>(&sendMsg), true);

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

			if (!IsHitDirectionalAttack(pCharacter, character, centerX, centerY, dfATTACK2_RANGE_X, dfATTACK2_RANGE_Y))
			{
				continue;
			}

			PacketHeader dmgHeader;
			PacketSCDamage dmgMsg;

			character->HP -= dfATTACK2_DAMAGE;
			if (character->HP < 0)
			{
				character->HP = 0;
			}

			Session* targetSession = FindSession(character->sessionID);
			MakePacket_Damage(&dmgHeader, &dmgMsg, pCharacter->sessionID, character->sessionID, static_cast<BYTE>(character->HP));
			SendPacket_Around(targetSession, &dmgHeader, reinterpret_cast<char*>(&dmgMsg));
		}
	}

	return true;
}

bool NetPacketProc_Attack3(Session* pSession, SerializedBuffer& packet)
{
	PacketCSAttack3 atk{};
	packet >> atk.direction
		>> atk.x
		>> atk.y;

	_LOG(LOG_LEVEL_DEBUG, L"# PACKET_ATTACK3 # SessionID:%u / Direction:%u / X:%d / Y:%d", pSession->sessionID, atk.direction, atk.x, atk.y);

	if (!IsValidViewDirection(atk.direction))
	{
		Disconnect(pSession);
		return false;
	}

	Character* pCharacter = FindCharacter(pSession->sessionID);
	if (pCharacter == nullptr)
	{
		_LOG(LOG_LEVEL_DEBUG, L"# ATTACK3 > SessionID:%d Character Not Found!", pSession->sessionID);
		return false;
	}

	if (NeedSync(atk.x, atk.y, pCharacter))
	{
		SendSync(pSession, pCharacter);
		_LOG(LOG_LEVEL_ERROR, L"SYNC_RANGE Attack3 ID=%d IP=%s", pSession->sessionID, pSession->ipStr);
		return true;
	}

	pCharacter->direction = NormalizeViewDir(atk.direction);

	PacketHeader packetHeader;
	PacketSCAttack3 sendMsg;
	MakePacket_Attack3(&packetHeader, &sendMsg, pCharacter->direction, pCharacter->sessionID, pCharacter->x, pCharacter->y);
	SendPacket_Around(pSession, &packetHeader, reinterpret_cast<char*>(&sendMsg), true);

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

			if (!IsHitDirectionalAttack(pCharacter, character, centerX, centerY, dfATTACK3_RANGE_X, dfATTACK3_RANGE_Y))
			{
				continue;
			}

			PacketHeader dmgHeader;
			PacketSCDamage dmgMsg;

			character->HP -= dfATTACK3_DAMAGE;
			if (character->HP < 0)
			{
				character->HP = 0;
			}

			Session* targetSession = FindSession(character->sessionID);
			MakePacket_Damage(&dmgHeader, &dmgMsg, pCharacter->sessionID, character->sessionID, static_cast<BYTE>(character->HP));
			SendPacket_Around(targetSession, &dmgHeader, reinterpret_cast<char*>(&dmgMsg));
		}
	}

	return true;
}
