#pragma once

#include "Protocol.h"
#include "SerializedBuffer.h"
#include "SectorPos.h"
#include "Session.h"

struct Character;

bool EnqueuePacket(Session* session, const SerializedBuffer* pPacket) noexcept;
void SendUnicast(Session* pSession, const SerializedBuffer* pPacket) noexcept;
void SendBroadcast(Session* pSession, const SerializedBuffer* pPacket) noexcept;
void Disconnect(Session* pSession, const WCHAR* reason = nullptr) noexcept;
void FlushDisconnectedSessions() noexcept;

// 특정 섹터 1개에 있는 클라이언트들 에게 메시지 보내기
void SendPacket_SectorOne(int sectorX, int sectorY, const SerializedBuffer* pPacket, Session* pExceptSession = nullptr) noexcept;
// 특정 주변 섹터 목록에 있는 클라이언트들 에게 메시지 보내기
void SendPacket_BySectorAround(const SectorAround& sectorAround, const SerializedBuffer* pPacket, Session* pExceptSession = nullptr) noexcept;
// 캐릭터 기준 주변 섹터에 메시지 보내기
void SendPacket_AroundCharacter(const Character* character, const SerializedBuffer* pPacket, Session* pExceptSession = nullptr) noexcept;
// 클라이언트 기준 주변 섹터에 메시지 보내기 (최대 9개 영역)
void SendPacket_Around(Session* pSession, const SerializedBuffer* pPacket, bool sendMe = false) noexcept;
