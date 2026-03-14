#pragma once

#include "Protocol.h"
#include "SerializedBuffer.h"
#include "Session.h"

struct Character;

void MakePacket_CreateMyCharacter(SerializedBuffer* pPacket, BYTE direction, DWORD ID, int x, int y, int HP);
void MakePacket_CreateOtherCharacter(SerializedBuffer* pPacket, BYTE direction, DWORD ID, int x, int y, int HP);
void MakePacket_DeleteCharacter(SerializedBuffer* pPacket, DWORD ID);

void MakePacket_MoveStart(SerializedBuffer* pPacket, DWORD ID, BYTE direction, int x, int y);
void MakePacket_MoveStop(SerializedBuffer* pPacket, DWORD ID, BYTE direction, int x, int y);

void MakePacket_Damage(SerializedBuffer* pPacket, DWORD attackID, DWORD damageID, BYTE damageHP);
void MakePacket_Attack1(SerializedBuffer* pPacket, BYTE direction, DWORD ID, int x, int y);
void MakePacket_Attack2(SerializedBuffer* pPacket, BYTE direction, DWORD ID, int x, int y);
void MakePacket_Attack3(SerializedBuffer* pPacket, BYTE direction, DWORD ID, int x, int y);

void MakePacket_Sync(SerializedBuffer* pPacket, DWORD ID, WORD x, WORD y);
void MakePacket_Echo(SerializedBuffer* pPacket, DWORD time) noexcept;

bool NeedSync(int clientX, int clientY, const Character* ch) noexcept;
void SendSync(Session* s, const Character* ch) noexcept;
