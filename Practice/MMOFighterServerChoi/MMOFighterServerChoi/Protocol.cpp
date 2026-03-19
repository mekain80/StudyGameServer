#include "stdafx.h"

#include <cstring>

#include "Protocol.h"

namespace
{
	WORD gExpectedBodySizeTable[256]{};
	bool gProtocolCacheInitialized = false;
}

void InitializeProtocolCache() noexcept
{
	if (gProtocolCacheInitialized)
	{
		return;
	}

	std::memset(gExpectedBodySizeTable, 0, sizeof(gExpectedBodySizeTable));
	gExpectedBodySizeTable[dfPACKET_CS_MOVE_START] = dfPACKET_CS_MOVE_START_SIZE;
	gExpectedBodySizeTable[dfPACKET_CS_MOVE_STOP] = dfPACKET_CS_MOVE_STOP_SIZE;
	gExpectedBodySizeTable[dfPACKET_CS_ATTACK1] = dfPACKET_CS_ATTACK1_SIZE;
	gExpectedBodySizeTable[dfPACKET_CS_ATTACK2] = dfPACKET_CS_ATTACK2_SIZE;
	gExpectedBodySizeTable[dfPACKET_CS_ATTACK3] = dfPACKET_CS_ATTACK3_SIZE;
	gExpectedBodySizeTable[dfPACKET_CS_SYNC] = dfPACKET_CS_SYNC_SIZE;
	gExpectedBodySizeTable[dfPACKET_CS_ECHO] = dfPACKET_CS_ECHO_SIZE;
	gProtocolCacheInitialized = true;
}

WORD GetExpectedBodySize(BYTE packetType) noexcept
{
	if (!gProtocolCacheInitialized)
	{
		InitializeProtocolCache();
	}

	return gExpectedBodySizeTable[packetType];
}
