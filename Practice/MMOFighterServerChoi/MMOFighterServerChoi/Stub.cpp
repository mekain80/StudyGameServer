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

	// TODO) Disconnect 비활성화 하고 동기화 패킷 클라에 쏴주는 것 추가하기
	//-----------------------------------------------------
	// 서버의 위치와 받은 패킷의 위치값이 너무 큰 차이가 난다면 싱크 패킷을 보내어 좌표 보정.
	//
	// 본 게임의 좌표 동기화 구조가 단순한 키보드 조작 (클라이언트의 선처리, 서버의 후 반영) 방식으로
	// 클라이언트의 좌표를 그대로 믿는 방식을 택하고 있음.
	// 실제 온라인 게임이라면 클라이언트에서 목적지를 공유하는 방식을 택해야 함.
	// 지금은 좌표에 대해서는 간단한 구현을 목적으로 하고 있으므로
	// 서버는 클라이언트의 좌표를 그대로 믿지만, 서버와 너무 큰 차이가 있다면 강제좌표 동기화 하도록 함
	//-----------------------------------------------------
	if (abs(x - pCharacter->x) > dfERROR_RANGE ||
		abs(y - pCharacter->y) > dfERROR_RANGE)
	{
		//mpSync(pPacket, pSession->sessionID, pCharacter->x, pCharacter->y);
		//SendPacket_Around(pSession, pPacket, true);
		//x = pCharacter->x;
		//y = pCharacter->y;

		_LOG(LOG_LEVEL_ERROR, L"dfERROR_RANGE Fail ID=%d IP=%s", pSession->sessionID, pSession->ipStr);
		Disconnect(pSession);
		return false;
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
	SendBroadcast(pSession, &packetHeader, reinterpret_cast<char*>(&sendMsg));

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

	if (abs(moveStop.x - pCharacter->x) > dfERROR_RANGE ||
		abs(moveStop.y - pCharacter->y) > dfERROR_RANGE)
	{
		_LOG(LOG_LEVEL_ERROR, L"dfERROR_RANGE Fail ID=%d IP=%s", pSession->sessionID, pSession->ipStr);
		Disconnect(pSession);
		return false;
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

	if (abs(atk.x - pCharacter->x) > dfERROR_RANGE ||
		abs(atk.y - pCharacter->y) > dfERROR_RANGE)
	{
		_LOG(LOG_LEVEL_ERROR, L"Attack dfERROR_RANGE Fail ID=%d IP=%s", pSession->sessionID, pSession->ipStr);
		Disconnect(pSession);
		return false;
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

			MakePacket_Damage(&dmgHeader, &dmgMsg, pCharacter->sessionID, character->sessionID, static_cast<BYTE>(character->HP));
			SendPacket_Around(pSession, &dmgHeader, reinterpret_cast<char*>(&dmgMsg));
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

	if (abs(atk.x - pCharacter->x) > dfERROR_RANGE ||
		abs(atk.y - pCharacter->y) > dfERROR_RANGE)
	{
		_LOG(LOG_LEVEL_ERROR, L"Attack dfERROR_RANGE Fail ID=%d IP=%s", pSession->sessionID, pSession->ipStr);
		Disconnect(pSession);
		return false;
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

			MakePacket_Damage(&dmgHeader, &dmgMsg, pCharacter->sessionID, character->sessionID, static_cast<BYTE>(character->HP));
			SendPacket_Around(pSession, &dmgHeader, reinterpret_cast<char*>(&dmgMsg));
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

	if (abs(atk.x - pCharacter->x) > dfERROR_RANGE ||
		abs(atk.y - pCharacter->y) > dfERROR_RANGE)
	{
		_LOG(LOG_LEVEL_ERROR, L"Attack dfERROR_RANGE Fail ID=%d IP=%s", pSession->sessionID, pSession->ipStr);
		Disconnect(pSession);
		return false;
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

			MakePacket_Damage(&dmgHeader, &dmgMsg, pCharacter->sessionID, character->sessionID, static_cast<BYTE>(character->HP));
			SendPacket_Around(pSession, &dmgHeader, reinterpret_cast<char*>(&dmgMsg));
		}
	}

	return true;
}
