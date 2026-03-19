#pragma once

#include "Session.h"

bool PacketProc(Session* pSession, BYTE byPacketType, const char* packetData, WORD packetSize);
bool NetPacketProc_MoveStart(Session* pSession, const char* packetData, WORD packetSize);
bool NetPacketProc_MoveStop(Session* pSession, const char* packetData, WORD packetSize);
bool NetPacketProc_Attack1(Session* pSession, const char* packetData, WORD packetSize);
bool NetPacketProc_Attack2(Session* pSession, const char* packetData, WORD packetSize);
bool NetPacketProc_Attack3(Session* pSession, const char* packetData, WORD packetSize);
bool NetPacketProc_Echo(Session* pSession, const char* packetData, WORD packetSize);
