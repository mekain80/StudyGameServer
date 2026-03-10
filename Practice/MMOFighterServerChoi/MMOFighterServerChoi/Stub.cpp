#include "stdafx.h"

#include "Stub.h"

#include "Game.h"
#include "GameInfo.h"
#include "Log.h"
#include "Network.h"
#include "PacketControl.h"
#include "Protocol.h"
#include "Proxy.h"

bool PacketProc(Session* pSession, BYTE byPacketType, char* pPacket, WORD packetSize)
{
    SerializedBuffer packet(static_cast<int>(packetSize));
    int putRet = packet.PutData(pPacket, packetSize);
    if (putRet != packetSize)
    {
        Logger(L"SerializedBuffer PutData fail");
        Disconnect(pSession);
        return false;
    }

    switch (byPacketType)
    {
    case dfPACKET_CS_MOVE_START:
        return NetPacketProc_MoveStart(pSession, packet);
    case dfPACKET_CS_MOVE_STOP:
        return NetPacketProc_MoveStop(pSession, packet);
    case dfPACKET_CS_ATTACK1:
        return NetPacketProc_Attack1(pSession, packet);
    case dfPACKET_CS_ATTACK2:
        return NetPacketProc_Attack2(pSession, packet);
    case dfPACKET_CS_ATTACK3:
        return NetPacketProc_Attack3(pSession, packet);
    default:
        return true;
    }
}

bool NetPacketProc_MoveStart(Session* pSession, SerializedBuffer& packet)
{
    PacketCSMoveStart moveStart{};

    packet >> moveStart.direction
        >> moveStart.x
        >> moveStart.y;

    {
        wchar_t buf[256];
        _snwprintf_s(buf, 256, _TRUNCATE, L"# PACKET_MOVESTART # SessionID:%u / Direction:%u / X:%d / Y:%d", pSession->sessionID, moveStart.direction, moveStart.x, moveStart.y);
        Logger(buf);
    }

    if (!IsValidMoveDirection(moveStart.direction))
    {
        Disconnect(pSession);
        return false;
    }

    if (abs(moveStart.x - pSession->x) > dfERROR_RANGE ||
        abs(moveStart.y - pSession->y) > dfERROR_RANGE)
    {
        wchar_t buf[256];
        _snwprintf_s(buf, 256, _TRUNCATE, L"dfERROR_RANGE Fail ID=%d IP=%s", pSession->sessionID, pSession->ipStr);
        Logger(buf);
        Disconnect(pSession);
        return false;
    }

    pSession->action = moveStart.direction;
    pSession->direction = NormalizeViewDir(moveStart.direction);
    pSession->x = static_cast<short>(moveStart.x);
    pSession->y = static_cast<short>(moveStart.y);

    PacketHeader packetHeader;
    PacketSCMoveStart sendMsg;
    MakePacket_MoveStart(&packetHeader, &sendMsg, pSession->sessionID, moveStart.direction, pSession->x, pSession->y);
    SendBroadcast(pSession, &packetHeader, reinterpret_cast<char*>(&sendMsg));

    return true;
}

bool NetPacketProc_MoveStop(Session* pSession, SerializedBuffer& packet)
{
    PacketCSMoveStop moveStop{};

    packet >> moveStop.direction
        >> moveStop.x
        >> moveStop.y;

    {
        wchar_t buf[256];
        _snwprintf_s(buf, 256, _TRUNCATE, L"# PACKET_MOVESTOP # SessionID:%u / Direction:%u / X:%d / Y:%d", pSession->sessionID, moveStop.direction, moveStop.x, moveStop.y);
        Logger(buf);
    }

    if (!IsValidViewDirection(moveStop.direction))
    {
        Disconnect(pSession);
        return false;
    }

    if (abs(moveStop.x - pSession->x) > dfERROR_RANGE ||
        abs(moveStop.y - pSession->y) > dfERROR_RANGE)
    {
        wchar_t buf[256];
        _snwprintf_s(buf, 256, _TRUNCATE, L"dfERROR_RANGE Fail ID=%d IP=%s", pSession->sessionID, pSession->ipStr);
        Logger(buf);
        Disconnect(pSession);
        return false;
    }

    pSession->action = dfACTION_STOP;
    pSession->direction = NormalizeViewDir(moveStop.direction);
    pSession->x = static_cast<short>(moveStop.x);
    pSession->y = static_cast<short>(moveStop.y);

    PacketHeader packetHeader;
    PacketSCMoveStop sendMsg;
    MakePacket_MoveStop(&packetHeader, &sendMsg, pSession->sessionID, pSession->direction, pSession->x, pSession->y);
    SendBroadcast(pSession, &packetHeader, reinterpret_cast<char*>(&sendMsg));

    return true;
}

bool NetPacketProc_Attack1(Session* pSession, SerializedBuffer& packet)
{
    PacketCSAttack1 atk{};
    packet >> atk.direction
        >> atk.x
        >> atk.y;

    {
        wchar_t buf[256];
        _snwprintf_s(buf, 256, _TRUNCATE, L"# PACKET_ATTACK1 # SessionID:%u / Direction:%u / X:%d / Y:%d", pSession->sessionID, atk.direction, atk.x, atk.y);
        Logger(buf);
    }

    if (!IsValidViewDirection(atk.direction))
    {
        Disconnect(pSession);
        return false;
    }

    if (abs(atk.x - pSession->x) > dfERROR_RANGE ||
        abs(atk.y - pSession->y) > dfERROR_RANGE)
    {
        wchar_t buf[256];
        _snwprintf_s(buf, 256, _TRUNCATE, L"Attack dfERROR_RANGE Fail ID=%d IP=%s", pSession->sessionID, pSession->ipStr);
        Logger(buf);
        Disconnect(pSession);
        return false;
    }

    pSession->direction = NormalizeViewDir(atk.direction);

    PacketHeader packetHeader;
    PacketSCAttack1 sendMsg;
    MakePacket_Attack1(&packetHeader, &sendMsg, pSession->direction, pSession->sessionID, pSession->x, pSession->y);
    SendBroadcast(pSession, &packetHeader, reinterpret_cast<char*>(&sendMsg));

    const int centerX = pSession->x;
    const int centerY = pSession->y;

    for (auto& session : gSessionList)
    {
        if (session == pSession)
        {
            continue;
        }

        if (!IsHitAttack1(pSession, session, centerX, centerY))
        {
            continue;
        }

        PacketHeader dmgHeader;
        PacketSCDamage dmgMsg;

        session->HP -= dfATTACK1_DAMAGE;
        if (session->HP < 0)
        {
            session->HP = 0;
        }

        MakePacket_Damage(&dmgHeader, &dmgMsg, pSession->sessionID, session->sessionID, session->HP);
        SendBroadcast(nullptr, &dmgHeader, reinterpret_cast<char*>(&dmgMsg));
    }

    return true;
}

bool NetPacketProc_Attack2(Session* pSession, SerializedBuffer& packet)
{
    PacketCSAttack2 atk{};
    packet >> atk.direction
        >> atk.x
        >> atk.y;

    {
        wchar_t buf[256];
        _snwprintf_s(buf, 256, _TRUNCATE, L"# PACKET_ATTACK2 # SessionID:%u / Direction:%u / X:%d / Y:%d", pSession->sessionID, atk.direction, atk.x, atk.y);
        Logger(buf);
    }

    if (!IsValidViewDirection(atk.direction))
    {
        Disconnect(pSession);
        return false;
    }

    if (abs(atk.x - pSession->x) > dfERROR_RANGE ||
        abs(atk.y - pSession->y) > dfERROR_RANGE)
    {
        wchar_t buf[256];
        _snwprintf_s(buf, 256, _TRUNCATE, L"Attack dfERROR_RANGE Fail ID=%d IP=%s", pSession->sessionID, pSession->ipStr);
        Logger(buf);
        Disconnect(pSession);
        return false;
    }

    pSession->direction = NormalizeViewDir(atk.direction);

    PacketHeader packetHeader;
    PacketSCAttack2 sendMsg;
    MakePacket_Attack2(&packetHeader, &sendMsg, pSession->direction, pSession->sessionID, pSession->x, pSession->y);
    SendBroadcast(pSession, &packetHeader, reinterpret_cast<char*>(&sendMsg));

    const int centerX = pSession->x;
    const int centerY = pSession->y;

    for (auto& session : gSessionList)
    {
        if (session == pSession)
        {
            continue;
        }

        if (!IsHitDirectionalAttack(pSession, session, centerX, centerY, dfATTACK2_RANGE_X, dfATTACK2_RANGE_Y))
        {
            continue;
        }

        PacketHeader dmgHeader;
        PacketSCDamage dmgMsg;

        session->HP -= dfATTACK2_DAMAGE;
        if (session->HP < 0)
        {
            session->HP = 0;
        }

        MakePacket_Damage(&dmgHeader, &dmgMsg, pSession->sessionID, session->sessionID, session->HP);
        SendBroadcast(nullptr, &dmgHeader, reinterpret_cast<char*>(&dmgMsg));
    }

    return true;
}

bool NetPacketProc_Attack3(Session* pSession, SerializedBuffer& packet)
{
    PacketCSAttack3 atk{};
    packet >> atk.direction
        >> atk.x
        >> atk.y;

    {
        wchar_t buf[256];
        _snwprintf_s(buf, 256, _TRUNCATE, L"# PACKET_ATTACK3 # SessionID:%u / Direction:%u / X:%d / Y:%d", pSession->sessionID, atk.direction, atk.x, atk.y);
        Logger(buf);
    }

    if (!IsValidViewDirection(atk.direction))
    {
        Disconnect(pSession);
        return false;
    }

    if (abs(atk.x - pSession->x) > dfERROR_RANGE ||
        abs(atk.y - pSession->y) > dfERROR_RANGE)
    {
        wchar_t buf[256];
        _snwprintf_s(buf, 256, _TRUNCATE, L"Attack dfERROR_RANGE Fail ID=%d IP=%s", pSession->sessionID, pSession->ipStr);
        Logger(buf);
        Disconnect(pSession);
        return false;
    }

    pSession->direction = NormalizeViewDir(atk.direction);

    PacketHeader packetHeader;
    PacketSCAttack3 sendMsg;
    MakePacket_Attack3(&packetHeader, &sendMsg, pSession->direction, pSession->sessionID, pSession->x, pSession->y);
    SendBroadcast(pSession, &packetHeader, reinterpret_cast<char*>(&sendMsg));

    const int centerX = pSession->x;
    const int centerY = pSession->y;

    for (auto& session : gSessionList)
    {
        if (session == pSession)
        {
            continue;
        }

        if (!IsHitDirectionalAttack(pSession, session, centerX, centerY, dfATTACK3_RANGE_X, dfATTACK3_RANGE_Y))
        {
            continue;
        }

        PacketHeader dmgHeader;
        PacketSCDamage dmgMsg;

        session->HP -= dfATTACK3_DAMAGE;
        if (session->HP < 0)
        {
            session->HP = 0;
        }

        MakePacket_Damage(&dmgHeader, &dmgMsg, pSession->sessionID, session->sessionID, session->HP);
        SendBroadcast(nullptr, &dmgHeader, reinterpret_cast<char*>(&dmgMsg));
    }

    return true;
}
