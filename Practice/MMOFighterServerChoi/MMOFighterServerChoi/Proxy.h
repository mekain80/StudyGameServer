#pragma once

#include "Protocol.h"

void InitHeader(PacketHeader* pHeader, BYTE type, BYTE bodySize) noexcept;

void MakePacket_CreateMyCharacter(PacketHeader* pHeader, PacketSCCreateMyCharacter* pPacket, BYTE direction, DWORD ID, int x, int y, int HP);
void MakePacket_CreateOtherCharacter(PacketHeader* pHeader, PacketSCCreateOtherCharacter* pPacket, BYTE direction, DWORD ID, int x, int y, int HP);
void MakePacket_DeleteCharacter(PacketHeader* pHeader, PacketSCDeleteCharacter* pPacket, DWORD ID);

void MakePacket_MoveStart(PacketHeader* pHeader, PacketSCMoveStart* pPacket, DWORD ID, BYTE direction, int x, int y);
void MakePacket_MoveStop(PacketHeader* pHeader, PacketSCMoveStop* pPacket, DWORD ID, BYTE direction, int x, int y);

void MakePacket_Damage(PacketHeader* pHeader, PacketSCDamage* pPacket, DWORD attackID, DWORD damageID, BYTE damageHP);
void MakePacket_Attack1(PacketHeader* pHeader, PacketSCAttack1* pPacket, BYTE direction, DWORD ID, int x, int y);
void MakePacket_Attack2(PacketHeader* pHeader, PacketSCAttack2* pPacket, BYTE direction, DWORD ID, int x, int y);
void MakePacket_Attack3(PacketHeader* pHeader, PacketSCAttack3* pPacket, BYTE direction, DWORD ID, int x, int y);
