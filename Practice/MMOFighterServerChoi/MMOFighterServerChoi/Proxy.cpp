#include "stdafx.h"

#include "Proxy.h"

#include "GameInfo.h"
#include "Character.h"
#include "PacketControl.h"

void InitHeader(PacketHeader* pHeader, BYTE type, BYTE bodySize) noexcept
{
    pHeader->code = dfNETWORK_PACKET_CODE;
    pHeader->size = bodySize;
    pHeader->type = type;
}

void MakePacket_CreateMyCharacter(PacketHeader* pHeader, PacketSCCreateMyCharacter* pPacket, BYTE direction, DWORD ID, int x, int y, int HP)
{
    InitHeader(pHeader, dfPACKET_SC_CREATE_MY_CHARACTER, sizeof(PacketSCCreateMyCharacter));
    pPacket->ID = ID;
    pPacket->direction = direction;
    pPacket->x = static_cast<WORD>(x);
    pPacket->y = static_cast<WORD>(y);
    pPacket->HP = HP;
}

void MakePacket_CreateOtherCharacter(PacketHeader* pHeader, PacketSCCreateOtherCharacter* pPacket, BYTE direction, DWORD ID, int x, int y, int HP)
{
    InitHeader(pHeader, dfPACKET_SC_CREATE_OTHER_CHARACTER, sizeof(PacketSCCreateOtherCharacter));
    pPacket->ID = ID;
    pPacket->direction = direction;
    pPacket->x = static_cast<WORD>(x);
    pPacket->y = static_cast<WORD>(y);
    pPacket->HP = HP;
}

void MakePacket_DeleteCharacter(PacketHeader* pHeader, PacketSCDeleteCharacter* pPacket, DWORD ID)
{
    InitHeader(pHeader, dfPACKET_SC_DELETE_CHARACTER, sizeof(PacketSCDeleteCharacter));
    pPacket->ID = ID;
}

void MakePacket_MoveStart(PacketHeader* pHeader, PacketSCMoveStart* pPacket, DWORD ID, BYTE direction, int x, int y)
{
    InitHeader(pHeader, dfPACKET_SC_MOVE_START, sizeof(PacketSCMoveStart));
    pPacket->ID = ID;
    pPacket->direction = direction;
    pPacket->x = static_cast<WORD>(x);
    pPacket->y = static_cast<WORD>(y);
}

void MakePacket_MoveStop(PacketHeader* pHeader, PacketSCMoveStop* pPacket, DWORD ID, BYTE direction, int x, int y)
{
    InitHeader(pHeader, dfPACKET_SC_MOVE_STOP, sizeof(PacketSCMoveStop));
    pPacket->ID = ID;
    pPacket->direction = direction;
    pPacket->x = static_cast<WORD>(x);
    pPacket->y = static_cast<WORD>(y);
}

void MakePacket_Damage(PacketHeader* pHeader, PacketSCDamage* pPacket, DWORD attackID, DWORD damageID, BYTE damageHP)
{
    InitHeader(pHeader, dfPACKET_SC_DAMAGE, sizeof(PacketSCDamage));
    pPacket->attackID = attackID;
    pPacket->damageID = damageID;
    pPacket->damageHP = damageHP;
}

void MakePacket_Attack1(PacketHeader* pHeader, PacketSCAttack1* pPacket, BYTE direction, DWORD ID, int x, int y)
{
    InitHeader(pHeader, dfPACKET_SC_ATTACK1, sizeof(PacketSCAttack1));
    pPacket->ID = ID;
    pPacket->direction = direction;
    pPacket->x = static_cast<WORD>(x);
    pPacket->y = static_cast<WORD>(y);
}

void MakePacket_Attack2(PacketHeader* pHeader, PacketSCAttack2* pPacket, BYTE direction, DWORD ID, int x, int y)
{
    InitHeader(pHeader, dfPACKET_SC_ATTACK2, sizeof(PacketSCAttack2));
    pPacket->ID = ID;
    pPacket->direction = direction;
    pPacket->x = static_cast<WORD>(x);
    pPacket->y = static_cast<WORD>(y);
}

void MakePacket_Attack3(PacketHeader* pHeader, PacketSCAttack3* pPacket, BYTE direction, DWORD ID, int x, int y)
{
    InitHeader(pHeader, dfPACKET_SC_ATTACK3, sizeof(PacketSCAttack3));
    pPacket->ID = ID;
    pPacket->direction = direction;
    pPacket->x = static_cast<WORD>(x);
    pPacket->y = static_cast<WORD>(y);
}

void MakePacket_Sync(PacketHeader* pHeader, PacketSCSync* pPacket, DWORD ID, WORD x, WORD y)
{
    InitHeader(pHeader, dfPACKET_SC_SYNC, sizeof(PacketSCSync));
    pPacket->ID = ID;
    pPacket->x = x;
    pPacket->y = y;
}

void MakePacket_Echo(PacketHeader* pHeader, PacketSCEcho* pPacket, DWORD time) noexcept
{
    InitHeader(pHeader, dfPACKET_SC_ECHO, sizeof(PacketSCEcho));
    pPacket->time = time;
}

bool NeedSync(int clientX, int clientY, const Character* ch) noexcept
{
    return std::abs(clientX - ch->x) > dfERROR_RANGE ||
        std::abs(clientY - ch->y) > dfERROR_RANGE;
}

void SendSync(Session* s, const Character* ch) noexcept
{
    PacketHeader h{};
    PacketSCSync p{};
    MakePacket_Sync(&h, &p, ch->sessionID,
        static_cast<WORD>(ch->x),
        static_cast<WORD>(ch->y));
    SendUnicast(s, &h, reinterpret_cast<char*>(&p));
}
