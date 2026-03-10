#pragma once

#include "Protocol.h"
#include "Session.h"

bool EnqueuePacket(Session* session, PacketHeader* pHeader, char* pPacket) noexcept;
void SendUnicast(Session* pSession, PacketHeader* pHeader, char* pPacket) noexcept;
void SendBroadcast(Session* pSession, PacketHeader* pHeader, char* pPacket) noexcept;
void Disconnect(Session* pSession) noexcept;
