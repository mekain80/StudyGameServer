#include "stdafx.h"

#include "Protocol.h"

WORD GetExpectedBodySize(BYTE packetType) noexcept
{
    switch (packetType)
    {
    case dfPACKET_CS_MOVE_START: return dfPACKET_CS_MOVE_START_SIZE;
    case dfPACKET_CS_MOVE_STOP:  return dfPACKET_CS_MOVE_STOP_SIZE;
    case dfPACKET_CS_ATTACK1:    return dfPACKET_CS_ATTACK1_SIZE;
    case dfPACKET_CS_ATTACK2:    return dfPACKET_CS_ATTACK2_SIZE;
    case dfPACKET_CS_ATTACK3:    return dfPACKET_CS_ATTACK3_SIZE;
    case dfPACKET_CS_SYNC:       return dfPACKET_CS_SYNC_SIZE;
    case dfPACKET_CS_ECHO:       return dfPACKET_CS_ECHO_SIZE;
    default:
        return 0;
    }
}
