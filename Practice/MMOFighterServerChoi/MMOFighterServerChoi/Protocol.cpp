#include "stdafx.h"

#include "Protocol.h"

WORD GetExpectedBodySize(BYTE packetType) noexcept
{
    switch (packetType)
    {
    case dfPACKET_CS_MOVE_START: return sizeof(PacketCSMoveStart);
    case dfPACKET_CS_MOVE_STOP:  return sizeof(PacketCSMoveStop);
    case dfPACKET_CS_ATTACK1:    return sizeof(PacketCSAttack1);
    case dfPACKET_CS_ATTACK2:    return sizeof(PacketCSAttack2);
    case dfPACKET_CS_ATTACK3:    return sizeof(PacketCSAttack3);
    case dfPACKET_CS_SYNC:       return sizeof(WORD) * 2;
    case dfPACKET_CS_ECHO:       return sizeof(PacketCSEcho);
    default:
        return 0;
    }
}
