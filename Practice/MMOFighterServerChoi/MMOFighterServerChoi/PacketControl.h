#pragma once

#include "Protocol.h"
#include "SectorPos.h"
#include "Session.h"

bool EnqueuePacket(Session* session, PacketHeader* pHeader, char* pPacket) noexcept;
void SendUnicast(Session* pSession, PacketHeader* pHeader, char* pPacket) noexcept;
void SendBroadcast(Session* pSession, PacketHeader* pHeader, char* pPacket) noexcept;
void Disconnect(Session* pSession) noexcept;

// 특정 섹터 1개에 있는 클라이언트들 에게 메시지 보내기
void SendPacket_SectorOne(int sectorX, int sectorY, PacketHeader* pHeader, char* pPacket, Session* pExceptSession = nullptr) noexcept;
// 특정 주변 섹터 목록에 있는 클라이언트들 에게 메시지 보내기
void SendPacket_BySectorAround(SectorAround sectorAround, PacketHeader* pHeader, char* pPacket, Session* pExceptSession = nullptr) noexcept;
// 클라이언트 기준 주변 섹터에 메시지 보내기 (최대 9개 영역)
void SendPacket_Around(Session* pSession, PacketHeader* pHeader, char* pPacket, bool sendMe = false) noexcept;
