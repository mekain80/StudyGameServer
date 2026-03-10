#pragma once

#include "SerializedBuffer.h"
#include "Session.h"

bool PacketProc(Session* pSession, BYTE byPacketType, char* pPacket, WORD packetSize);
bool NetPacketProc_MoveStart(Session* pSession, SerializedBuffer& packet);
bool NetPacketProc_MoveStop(Session* pSession, SerializedBuffer& packet);
bool NetPacketProc_Attack1(Session* pSession, SerializedBuffer& packet);
bool NetPacketProc_Attack2(Session* pSession, SerializedBuffer& packet);
bool NetPacketProc_Attack3(Session* pSession, SerializedBuffer& packet);
