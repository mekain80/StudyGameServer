#include "stdafx.h"

#include "Proxy.h"

#include "GameInfo.h"
#include "Character.h"
#include "PacketControl.h"

namespace
{
    void BeginPacket(SerializedBuffer* pPacket, BYTE type, BYTE bodySize) noexcept
    {
        if (pPacket == nullptr)
        {
            return;
        }

        PacketHeader packetHeader{};
        packetHeader.code = dfNETWORK_PACKET_CODE;
        packetHeader.size = bodySize;
        packetHeader.type = type;

        pPacket->Clear();
        pPacket->PutData(reinterpret_cast<const char*>(&packetHeader), dfPACKET_HEADER_SIZE);
    }
}

void MakePacket_CreateMyCharacter(SerializedBuffer* pPacket, BYTE direction, DWORD ID, int x, int y, int HP)
{
    BeginPacket(pPacket, dfPACKET_SC_CREATE_MY_CHARACTER, dfPACKET_SC_CREATE_MY_CHARACTER_SIZE);
    *pPacket << static_cast<unsigned long>(ID)
        << direction
        << static_cast<unsigned short>(x)
        << static_cast<unsigned short>(y)
        << static_cast<unsigned char>(HP);
}

void MakePacket_CreateOtherCharacter(SerializedBuffer* pPacket, BYTE direction, DWORD ID, int x, int y, int HP)
{
    BeginPacket(pPacket, dfPACKET_SC_CREATE_OTHER_CHARACTER, dfPACKET_SC_CREATE_OTHER_CHARACTER_SIZE);
    *pPacket << static_cast<unsigned long>(ID)
        << direction
        << static_cast<unsigned short>(x)
        << static_cast<unsigned short>(y)
        << static_cast<unsigned char>(HP);
}

void MakePacket_DeleteCharacter(SerializedBuffer* pPacket, DWORD ID)
{
    BeginPacket(pPacket, dfPACKET_SC_DELETE_CHARACTER, dfPACKET_SC_DELETE_CHARACTER_SIZE);
    *pPacket << static_cast<unsigned long>(ID);
}

void MakePacket_MoveStart(SerializedBuffer* pPacket, DWORD ID, BYTE direction, int x, int y)
{
    BeginPacket(pPacket, dfPACKET_SC_MOVE_START, dfPACKET_SC_MOVE_START_SIZE);
    *pPacket << static_cast<unsigned long>(ID)
        << direction
        << static_cast<unsigned short>(x)
        << static_cast<unsigned short>(y);
}

void MakePacket_MoveStop(SerializedBuffer* pPacket, DWORD ID, BYTE direction, int x, int y)
{
    BeginPacket(pPacket, dfPACKET_SC_MOVE_STOP, dfPACKET_SC_MOVE_STOP_SIZE);
    *pPacket << static_cast<unsigned long>(ID)
        << direction
        << static_cast<unsigned short>(x)
        << static_cast<unsigned short>(y);
}

void MakePacket_Damage(SerializedBuffer* pPacket, DWORD attackID, DWORD damageID, BYTE damageHP)
{
    BeginPacket(pPacket, dfPACKET_SC_DAMAGE, dfPACKET_SC_DAMAGE_SIZE);
    *pPacket << static_cast<unsigned long>(attackID)
        << static_cast<unsigned long>(damageID)
        << damageHP;
}

void MakePacket_Attack1(SerializedBuffer* pPacket, BYTE direction, DWORD ID, int x, int y)
{
    BeginPacket(pPacket, dfPACKET_SC_ATTACK1, dfPACKET_SC_ATTACK1_SIZE);
    *pPacket << static_cast<unsigned long>(ID)
        << direction
        << static_cast<unsigned short>(x)
        << static_cast<unsigned short>(y);
}

void MakePacket_Attack2(SerializedBuffer* pPacket, BYTE direction, DWORD ID, int x, int y)
{
    BeginPacket(pPacket, dfPACKET_SC_ATTACK2, dfPACKET_SC_ATTACK2_SIZE);
    *pPacket << static_cast<unsigned long>(ID)
        << direction
        << static_cast<unsigned short>(x)
        << static_cast<unsigned short>(y);
}

void MakePacket_Attack3(SerializedBuffer* pPacket, BYTE direction, DWORD ID, int x, int y)
{
    BeginPacket(pPacket, dfPACKET_SC_ATTACK3, dfPACKET_SC_ATTACK3_SIZE);
    *pPacket << static_cast<unsigned long>(ID)
        << direction
        << static_cast<unsigned short>(x)
        << static_cast<unsigned short>(y);
}

void MakePacket_Sync(SerializedBuffer* pPacket, DWORD ID, WORD x, WORD y)
{
    BeginPacket(pPacket, dfPACKET_SC_SYNC, dfPACKET_SC_SYNC_SIZE);
    *pPacket << static_cast<unsigned long>(ID)
        << static_cast<unsigned short>(x)
        << static_cast<unsigned short>(y);
}

void MakePacket_Echo(SerializedBuffer* pPacket, DWORD time) noexcept
{
    BeginPacket(pPacket, dfPACKET_SC_ECHO, dfPACKET_SC_ECHO_SIZE);
    *pPacket << static_cast<unsigned long>(time);
}

bool NeedSync(int clientX, int clientY, const Character* ch) noexcept
{
    return std::abs(clientX - ch->x) > dfERROR_RANGE ||
        std::abs(clientY - ch->y) > dfERROR_RANGE;
}

void SendSync(Session* s, const Character* ch) noexcept
{
    SerializedBuffer p(dfPACKET_HEADER_SIZE + dfPACKET_SC_SYNC_SIZE);
    MakePacket_Sync(&p, ch->sessionID,
        static_cast<WORD>(ch->x),
        static_cast<WORD>(ch->y));
    SendUnicast(s, &p);
}
