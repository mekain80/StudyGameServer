#pragma comment(lib, "ws2_32")
#pragma comment(lib, "winmm.lib")

#include <WinSock2.h>
#include <Windows.h>
#include <WS2tcpip.h>
#include <stdio.h>
#include <tchar.h>
#include <list>

#include "RingBuffer.h"
#include "PacketDefine.h"
#include "CPacket.h"

// 게임프레임 : 50fps
#define SERVER_PORT 5000
#define BUFFER_SIZE 10000
#define dfNETWORK_PACKET_CODE 0x89

// 화면 이동영역--------------------------
#define dfRANGE_MOVE_TOP    50
#define dfRANGE_MOVE_LEFT   10
#define dfRANGE_MOVE_RIGHT  630
#define dfRANGE_MOVE_BOTTOM 470

//-----------------------------------------------------------------
// 프레임당 이동 단위
//-----------------------------------------------------------------
#define dfMOVE_X 3
#define dfMOVE_Y 2

//-----------------------------------------------------------------
// 이동 오류체크 범위
//-----------------------------------------------------------------
#define dfERROR_RANGE 50

//---------------------------------------------------------------
// 공격범위.
//---------------------------------------------------------------
#define dfATTACK1_RANGE_X 80
#define dfATTACK2_RANGE_X 90
#define dfATTACK3_RANGE_X 100
#define dfATTACK1_RANGE_Y 10
#define dfATTACK2_RANGE_Y 10
#define dfATTACK3_RANGE_Y 20

//---------------------------------------------------------------
// 공격 데미지.
//---------------------------------------------------------------
#define dfATTACK1_DAMAGE 3
#define dfATTACK2_DAMAGE 5
#define dfATTACK3_DAMAGE 10

// 캐릭터 초기 설정
#define MAX_HP          100
#define dfACTION_STOP   0xFFFFFFFF  // 이동 안 하는 상태

struct Session
{
    SOCKET      socket;
    SOCKADDR_IN addr;

    wchar_t     ipStr[32];
    USHORT      port;

    DWORD       sessionID;
    RingBuffer  recvQ;
    RingBuffer  sendQ;

    DWORD       action;
    BYTE        direction;

    short       x;
    short       y;

    char        HP;
};

bool                gbShutdown = false;
DWORD               gAllocID = 1;
SOCKET              gListenSocket = INVALID_SOCKET;
std::list<Session*> gSessionList;

// 타이밍(단위: QPC counts)
LARGE_INTEGER gFreq;
LARGE_INTEGER gFrameStartTick;
LARGE_INTEGER gFrameEndTick;

//---------------------------------------------------------------
// 헬퍼 함수 선언
//---------------------------------------------------------------
BYTE NormalizeViewDir(BYTE direction) noexcept;
void GetMoveDelta(BYTE direction, int& dx, int& dy) noexcept;

bool IsHitAttack1(const Session* attacker, const Session* target,
    int centerX, int centerY) noexcept;
bool IsInAttackRect(int centerX, int centerY,
    int targetX, int targetY,
    int rangeX, int rangeY) noexcept;

void InitHeader(PacketHeader* pHeader, WORD type, WORD bodySize) noexcept;
bool EnqueuePacket(Session* session, PacketHeader* pHeader, char* pPacket) noexcept;

//---------------------------------------------------------------

void Update() noexcept;
bool MoveCheck(BYTE direction, int x, int y) noexcept;

void NetIOProcess() noexcept;
void NetProc_Accept() noexcept;
void NetProc_Recv(Session*) noexcept;
void NetProc_Send(Session*) noexcept;

// 직렬화 버퍼 기반 패킷 처리
bool PacketProc(Session* pSession, BYTE byPacketType, char* pPacket, WORD packetSize);
bool NetPacketProc_MoveStart(Session* pSession, CPacket& packet);
bool NetPacketProc_MoveStop(Session* pSession, CPacket& packet);
bool NetPacketProc_Attack1(Session* pSession, CPacket& packet);
bool NetPacketProc_Attack2(Session* pSession, CPacket& packet);
bool NetPacketProc_Attack3(Session* pSession, CPacket& packet);

// 패킷 생성
void MakePacket_CreateMyCharacter(PacketHeader* pHeader, PacketSCCreateMyCharacter* pPacket, BYTE direction, DWORD ID, int x, int y, int HP);
void MakePacket_CreateOtherCharacter(PacketHeader* pHeader, PacketSCCreateOtherCharacter* pPacket, BYTE direction, DWORD ID, int x, int y, int HP);
void MakePacket_DeleteCharacter(PacketHeader* pHeader, PacketSCDeleteCharacter* pPacket, DWORD ID);

void MakePacket_MoveStart(PacketHeader* pHeader, PacketSCMoveStart* pPacket, DWORD ID, BYTE direction, int x, int y);
void MakePacket_MoveStop(PacketHeader* pHeader, PacketSCMoveStop* pPacket, DWORD ID, BYTE direction, int x, int y);

void MakePacket_Damage(PacketHeader* pHeader, PacketSCDamage* pPacket, DWORD attackID, DWORD damageID, BYTE damageHP);
void MakePacket_Attack1(PacketHeader* pHeader, PacketSCAttack1* pPacket, BYTE direction, DWORD ID, int x, int y);
void MakePacket_Attack2(PacketHeader* pHeader, PacketSCAttack2* pPacket, BYTE direction, DWORD ID, int x, int y);
void MakePacket_Attack3(PacketHeader* pHeader, PacketSCAttack3* pPacket, BYTE direction, DWORD ID, int x, int y);

void SendUnicast(Session* pSession, PacketHeader* pHeader, char* pPacket) noexcept;
void SendBroadcast(Session* pSession, PacketHeader* pHeader, char* pPacket) noexcept;
void Disconnect(Session* pSession) noexcept;

void ErrorHandler(const wchar_t* msg) noexcept;
void Logger(const wchar_t* msg) noexcept;

//===============================================================
// main
//===============================================================
int _tmain(int argc, _TCHAR* argv[])
{
    // 콘솔 출력 UTF-8
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    system("chcp 65001 > nul");

    timeBeginPeriod(1);
    QueryPerformanceFrequency(&gFreq);
    QueryPerformanceCounter(&gFrameStartTick);

    // 윈속 초기화
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
    {
        ErrorHandler(L"WSAStartup fail");
    }
    Logger(L"WSAStartup #");

    gListenSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (gListenSocket == INVALID_SOCKET)
        ErrorHandler(L"socket fail");

    u_long on = 1;
    int ioctRet = ioctlsocket(gListenSocket, FIONBIO, &on);
    if (ioctRet == SOCKET_ERROR)
        ErrorHandler(L"ioctlsocket fail");

    SOCKADDR_IN serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(SERVER_PORT);
    serverAddr.sin_addr.S_un.S_addr = htonl(INADDR_ANY);

    int bindRet = bind(gListenSocket, (SOCKADDR*)&serverAddr, sizeof(serverAddr));
    if (bindRet == SOCKET_ERROR)
        ErrorHandler(L"bind fail");

    {
        wchar_t buf[256];
        _snwprintf_s(buf, 256, _TRUNCATE,
            L"BIND OK # Port:%d", SERVER_PORT);
        Logger(buf);
    }

    int listenRet = listen(gListenSocket, SOMAXCONN);
    if (listenRet == SOCKET_ERROR)
        ErrorHandler(L"listen() fail");

    Logger(L"Listen OK #");

    while (!gbShutdown)
    {
        NetIOProcess();
        Update();
    }

    return 0;
}

//===============================================================
// 네트워크 I/O
//===============================================================
void NetIOProcess() noexcept
{
    FD_SET readSet;
    FD_SET writeSet;

    FD_ZERO(&readSet);
    FD_ZERO(&writeSet);

    FD_SET(gListenSocket, &readSet);

    for (Session* session : gSessionList)
    {
        FD_SET(session->socket, &readSet);
        if (session->sendQ.GetUseSize() > 0)
        {
            FD_SET(session->socket, &writeSet);
        }
    }

    timeval time;
    time.tv_sec = 0;
    time.tv_usec = 0;

    int result = select(0, &readSet, &writeSet, nullptr, &time);
    if (result == SOCKET_ERROR)
    {
        ErrorHandler(L"Select fail");
        return;
    }

    if (result == 0)
        return;

    if (FD_ISSET(gListenSocket, &readSet))
    {
        --result;
        NetProc_Accept();
        if (result <= 0)
            return;
    }

    for (auto it = gSessionList.begin(); it != gSessionList.end() && result > 0; )
    {
        Session* session = *it;
        SOCKET s = session->socket;
        ++it;

        if (FD_ISSET(s, &readSet))
        {
            --result;
            NetProc_Recv(session);
            if (result <= 0)
                break;
        }

        if (result > 0 && FD_ISSET(s, &writeSet))
        {
            --result;
            NetProc_Send(session);
            if (result <= 0)
                break;
        }
    }
}

//===============================================================
// 게임 로직 업데이트
//===============================================================
void Update() noexcept
{
    QueryPerformanceCounter(&gFrameEndTick);
    double elapsed = static_cast<double>(gFrameEndTick.QuadPart - gFrameStartTick.QuadPart) / gFreq.QuadPart;

    // 50 fps
    if (elapsed <= 0.02)
        return;

    gFrameStartTick = gFrameEndTick;

    for (auto it = gSessionList.begin(); it != gSessionList.end(); )
    {
        Session* session = *it;

        if (session->HP <= 0)
        {
            ++it;
            Disconnect(session);
            continue;
        }

        // 정지 상태 등은 MoveCheck에서 false를 리턴해서 스킵
        if (!MoveCheck(session->action, session->x, session->y))
        {
            ++it;
            continue;
        }

        int dx = 0;
        int dy = 0;
        GetMoveDelta(session->action, dx, dy);
        session->x += static_cast<short>(dx);
        session->y += static_cast<short>(dy);

        const wchar_t* dirStr = L"STOP";
        switch (session->action)
        {
        case dfPACKET_MOVE_DIR_UU: dirStr = L"UU"; break;
        case dfPACKET_MOVE_DIR_DD: dirStr = L"DD"; break;
        case dfPACKET_MOVE_DIR_RR: dirStr = L"RR"; break;
        case dfPACKET_MOVE_DIR_LL: dirStr = L"LL"; break;
        case dfPACKET_MOVE_DIR_RU: dirStr = L"RU"; break;
        case dfPACKET_MOVE_DIR_RD: dirStr = L"RD"; break;
        case dfPACKET_MOVE_DIR_LU: dirStr = L"LU"; break;
        case dfPACKET_MOVE_DIR_LD: dirStr = L"LD"; break;
        default:                   dirStr = L"STOP"; break;
        }

        wchar_t buf[256];
        _snwprintf_s(buf, 256, _TRUNCATE,
            L"# gameRun : %s # SessionID : %u / X : %d / Y : %d",
            dirStr, session->sessionID, session->x, session->y);
        Logger(buf);

        ++it;
    }
}

//===============================================================
// 이동 가능 체크
//===============================================================
bool MoveCheck(BYTE direction, int x, int y) noexcept
{
    int dx = 0;
    int dy = 0;

    GetMoveDelta(direction, dx, dy);

    // 정지 상태 등
    if (dx == 0 && dy == 0 && direction != dfPACKET_MOVE_DIR_UU &&
        direction != dfPACKET_MOVE_DIR_DD &&
        direction != dfPACKET_MOVE_DIR_RR &&
        direction != dfPACKET_MOVE_DIR_LL &&
        direction != dfPACKET_MOVE_DIR_RU &&
        direction != dfPACKET_MOVE_DIR_RD &&
        direction != dfPACKET_MOVE_DIR_LU &&
        direction != dfPACKET_MOVE_DIR_LD)
    {
        return false;
    }

    int nx = x + dx;
    int ny = y + dy;

    if (ny < dfRANGE_MOVE_TOP || ny > dfRANGE_MOVE_BOTTOM)
        return false;

    if (nx < dfRANGE_MOVE_LEFT || nx > dfRANGE_MOVE_RIGHT)
        return false;

    return true;
}

//===============================================================
// Accept
//===============================================================
void NetProc_Accept() noexcept
{
    SOCKADDR_IN clientAddr;
    int addrlen = sizeof(clientAddr);

    SOCKET clientSocket = accept(gListenSocket, (SOCKADDR*)&clientAddr, &addrlen);
    if (clientSocket == INVALID_SOCKET)
    {
        Logger(L"clientSocket accept fail");
        return;
    }

    Session* pSession = new Session;
    pSession->socket = clientSocket;
    pSession->addr = clientAddr;

    InetNtopW(AF_INET, &clientAddr.sin_addr, pSession->ipStr, _countof(pSession->ipStr));
    pSession->port = ntohs(clientAddr.sin_port);

    pSession->sessionID = gAllocID++;
    pSession->y = static_cast<short>(dfRANGE_MOVE_TOP + rand() % (dfRANGE_MOVE_BOTTOM - dfRANGE_MOVE_TOP + 1));
    pSession->x = static_cast<short>(dfRANGE_MOVE_LEFT + rand() % (dfRANGE_MOVE_RIGHT - dfRANGE_MOVE_LEFT + 1));
    pSession->direction = dfPACKET_MOVE_DIR_LL;
    pSession->action = dfACTION_STOP;
    pSession->HP = MAX_HP;

    gSessionList.push_back(pSession);

    // 나 자신 생성 패킷
    PacketHeader           packetHeader;
    PacketSCCreateMyCharacter createMyCharacter;
    MakePacket_CreateMyCharacter(&packetHeader, &createMyCharacter,
        pSession->direction, pSession->sessionID,
        pSession->x, pSession->y, pSession->HP);

    wprintf(L"[Server] CreateMyCharacter header : code=%u size=%u type=%u\n",
        packetHeader.code, packetHeader.size, packetHeader.type);
    wprintf(L"[Server] CreateMyCharacter body   : ID=%u Dir=%u X=%d Y=%d HP=%d\n",
        createMyCharacter.ID,
        createMyCharacter.direction,
        createMyCharacter.x,
        createMyCharacter.y,
        createMyCharacter.HP);

    SendUnicast(pSession, &packetHeader, (char*)&createMyCharacter);

    // 기존 유저들 → 신규 유저
    for (auto& other : gSessionList)
    {
        if (other == pSession)
            continue;

        PacketHeader              otherHeader;
        PacketSCCreateOtherCharacter otherPacket;
        MakePacket_CreateOtherCharacter(&otherHeader, &otherPacket,
            other->direction, other->sessionID,
            other->x, other->y, other->HP);
        SendUnicast(pSession, &otherHeader, (char*)&otherPacket);
    }

    // 신규 유저 → 전체 브로드캐스트
    PacketHeader                broadHeader;
    PacketSCCreateOtherCharacter broadPacket;
    MakePacket_CreateOtherCharacter(&broadHeader, &broadPacket,
        pSession->direction, pSession->sessionID,
        pSession->x, pSession->y, pSession->HP);
    SendBroadcast(pSession, &broadHeader, (char*)&broadPacket);

    wchar_t buf[256];
    _snwprintf_s(buf, 256, _TRUNCATE,
        L"Connect # IP:%s / SessionID:%d", pSession->ipStr, pSession->sessionID);
    Logger(buf);
    _snwprintf_s(buf, 256, _TRUNCATE,
        L"# PACKET_CONNECT # SessionID:%d", pSession->sessionID);
    Logger(buf);
    _snwprintf_s(buf, 256, _TRUNCATE,
        L"Create Character # SessionID:%d    X:%d    Y:%d",
        pSession->sessionID, pSession->x, pSession->y);
    Logger(buf);
}

//===============================================================
// Recv
//===============================================================
void NetProc_Recv(Session* pSession) noexcept
{
    char buffer[BUFFER_SIZE] = { 0 };

    int recvRet = recv(pSession->socket, buffer, sizeof(buffer), 0);

    if (recvRet == 0)
    {
        Disconnect(pSession);
        return;
    }
    else if (recvRet == SOCKET_ERROR)
    {
        if (WSAGetLastError() == WSAEWOULDBLOCK)
            return;

        Disconnect(pSession);
        return;
    }

    pSession->recvQ.Enqueue(buffer, recvRet);

    while (true)
    {
        size_t headerSize = sizeof(PacketHeader);
        if (pSession->recvQ.GetUseSize() < headerSize)
            break;

        PacketHeader packetHeader;
        pSession->recvQ.Peek(reinterpret_cast<char*>(&packetHeader), headerSize);

        if (packetHeader.code != dfNETWORK_PACKET_CODE)
        {
            Disconnect(pSession);
            return;
        }

        int totalSize = static_cast<int>(headerSize) + packetHeader.size;
        if (pSession->recvQ.GetUseSize() < totalSize)
            return;

        pSession->recvQ.MoveFront(headerSize);

        if (packetHeader.size > BUFFER_SIZE)
        {
            Disconnect(pSession);
            return;
        }

        char packetBuffer[BUFFER_SIZE];
        pSession->recvQ.Dequeue(packetBuffer, packetHeader.size);

        // packetHeader.size 만큼의 바디를 CPacket으로 감싸서 처리
        PacketProc(pSession, packetHeader.type, packetBuffer, packetHeader.size);
    }
}

//===============================================================
// Send
//===============================================================
void NetProc_Send(Session* pSession) noexcept
{
    char buffer[BUFFER_SIZE];

    while (true)
    {
        int useSize = static_cast<int>(pSession->sendQ.GetUseSize());
        if (useSize <= 0)
            break;

        int sendSize = useSize;
        if (sendSize > BUFFER_SIZE)
            sendSize = BUFFER_SIZE;

        pSession->sendQ.Peek(buffer, sendSize);

        int sendRet = send(pSession->socket, buffer, sendSize, 0);
        if (sendRet == 0)
        {
            Disconnect(pSession);
            return;
        }
        else if (sendRet == SOCKET_ERROR)
        {
            int err = WSAGetLastError();
            if (err == WSAEWOULDBLOCK)
                return;

            Disconnect(pSession);
            return;
        }

        pSession->sendQ.MoveFront(sendRet);
    }
}

//===============================================================
// 패킷 처리 (직렬화 버퍼 사용)
//===============================================================
bool PacketProc(Session* pSession, BYTE byPacketType, char* pPacket, WORD packetSize)
{
    // 패킷 바디를 직렬화 버퍼에 적재
    CPacket packet(static_cast<int>(packetSize));
    int putRet = packet.PutData(pPacket, packetSize);
    if (putRet != packetSize)
    {
        Logger(L"CPacket PutData fail");
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
    }
    return true;
}

//===============================================================
// MoveStart (직렬화 버퍼)
//===============================================================
bool NetPacketProc_MoveStart(Session* pSession, CPacket& packet)
{
    PacketCSMoveStart moveStart{};

    // 클라 쪽에서 보낸 순서대로 읽어야 함 (pack(1) 가정)
    packet >> moveStart.direction
        >> moveStart.x
        >> moveStart.y;

    {
        wchar_t buf[256];
        _snwprintf_s(buf, 256, _TRUNCATE,
            L"# PACKET_MOVESTART # SessionID:%u / Direction:%u / X:%d / Y:%d",
            pSession->sessionID,
            moveStart.direction,
            moveStart.x,
            moveStart.y);
        Logger(buf);
    }

    if (abs(moveStart.x - pSession->x) > dfERROR_RANGE ||
        abs(moveStart.y - pSession->y) > dfERROR_RANGE)
    {
        wchar_t buf[256];
        _snwprintf_s(buf, 256, _TRUNCATE,
            L"dfERROR_RANGE Fail ID=%d IP=%s",
            pSession->sessionID, pSession->ipStr);
        Logger(buf);
        Disconnect(pSession);
        return false;
    }

    pSession->action = moveStart.direction;
    pSession->direction = NormalizeViewDir(moveStart.direction);

    pSession->x = static_cast<short>(moveStart.x);
    pSession->y = static_cast<short>(moveStart.y);

    PacketHeader      packetHeader;
    PacketSCMoveStart sendMsg;
    MakePacket_MoveStart(&packetHeader, &sendMsg,
        pSession->sessionID,
        moveStart.direction,
        pSession->x, pSession->y);
    SendBroadcast(pSession, &packetHeader, (char*)&sendMsg);

    return true;
}

//===============================================================
// MoveStop (직렬화 버퍼)
//===============================================================
bool NetPacketProc_MoveStop(Session* pSession, CPacket& packet)
{
    PacketCSMoveStop moveStop{};

    packet >> moveStop.direction
        >> moveStop.x
        >> moveStop.y;

    {
        wchar_t buf[256];
        _snwprintf_s(buf, 256, _TRUNCATE,
            L"# PACKET_MOVESTOP # SessionID:%u / Direction:%u / X:%d / Y:%d",
            pSession->sessionID,
            moveStop.direction,
            moveStop.x,
            moveStop.y);
        Logger(buf);
    }

    if (abs(moveStop.x - pSession->x) > dfERROR_RANGE ||
        abs(moveStop.y - pSession->y) > dfERROR_RANGE)
    {
        wchar_t buf[256];
        _snwprintf_s(buf, 256, _TRUNCATE,
            L"dfERROR_RANGE Fail ID=%d IP=%s",
            pSession->sessionID, pSession->ipStr);
        Logger(buf);
        Disconnect(pSession);
        return false;
    }

    pSession->action = dfACTION_STOP;
    pSession->direction = NormalizeViewDir(moveStop.direction);

    pSession->x = static_cast<short>(moveStop.x);
    pSession->y = static_cast<short>(moveStop.y);

    PacketHeader     packetHeader;
    PacketSCMoveStop sendMsg;
    MakePacket_MoveStop(&packetHeader, &sendMsg,
        pSession->sessionID,
        pSession->direction,
        pSession->x, pSession->y);
    SendBroadcast(pSession, &packetHeader, (char*)&sendMsg);

    return true;
}

//===============================================================
// Attack1 (좌우 방향에 따라 판정범위 다름, 직렬화 버퍼)
//===============================================================
bool NetPacketProc_Attack1(Session* pSession, CPacket& packet)
{
    PacketCSAttack1 atk{};
    packet >> atk.direction
        >> atk.x
        >> atk.y;

    {
        wchar_t buf[256];
        _snwprintf_s(buf, 256, _TRUNCATE,
            L"# PACKET_ATTACK1 # SessionID:%u / Direction:%u / X:%d / Y:%d",
            pSession->sessionID,
            atk.direction,
            atk.x,
            atk.y);
        Logger(buf);
    }

    if (abs(atk.x - pSession->x) > dfERROR_RANGE ||
        abs(atk.y - pSession->y) > dfERROR_RANGE)
    {
        wchar_t buf[256];
        _snwprintf_s(buf, 256, _TRUNCATE,
            L"Attack dfERROR_RANGE Fail ID=%d IP=%s",
            pSession->sessionID, pSession->ipStr);
        Logger(buf);
        Disconnect(pSession);
        return false;
    }

    pSession->direction = NormalizeViewDir(atk.direction);

    PacketHeader     packetHeader;
    PacketSCAttack1  sendMsg;
    MakePacket_Attack1(&packetHeader, &sendMsg,
        pSession->direction,
        pSession->sessionID,
        pSession->x, pSession->y);
    SendBroadcast(pSession, &packetHeader, (char*)&sendMsg);

    const int centerX = atk.x;
    const int centerY = atk.y;

    for (auto& session : gSessionList)
    {
        if (session == pSession)
            continue;

        if (!IsHitAttack1(pSession, session, centerX, centerY))
            continue;

        PacketHeader   dmgHeader;
        PacketSCDamage dmgMsg;

        session->HP -= dfATTACK1_DAMAGE;
        MakePacket_Damage(&dmgHeader, &dmgMsg,
            pSession->sessionID, session->sessionID,
            session->HP);
        SendBroadcast(nullptr, &dmgHeader, (char*)&dmgMsg);
    }

    return true;
}

//===============================================================
// Attack2 (중심 기준 직사각형, 직렬화 버퍼)
//===============================================================
bool NetPacketProc_Attack2(Session* pSession, CPacket& packet)
{
    PacketCSAttack2 atk{};
    packet >> atk.direction
        >> atk.x
        >> atk.y;

    {
        wchar_t buf[256];
        _snwprintf_s(buf, 256, _TRUNCATE,
            L"# PACKET_ATTACK2 # SessionID:%u / Direction:%u / X:%d / Y:%d",
            pSession->sessionID,
            atk.direction,
            atk.x,
            atk.y);
        Logger(buf);
    }

    if (abs(atk.x - pSession->x) > dfERROR_RANGE ||
        abs(atk.y - pSession->y) > dfERROR_RANGE)
    {
        wchar_t buf[256];
        _snwprintf_s(buf, 256, _TRUNCATE,
            L"Attack dfERROR_RANGE Fail ID=%d IP=%s",
            pSession->sessionID, pSession->ipStr);
        Logger(buf);
        Disconnect(pSession);
        return false;
    }

    pSession->direction = NormalizeViewDir(atk.direction);

    PacketHeader     packetHeader;
    PacketSCAttack2  sendMsg;
    MakePacket_Attack2(&packetHeader, &sendMsg,
        pSession->direction,
        pSession->sessionID,
        pSession->x, pSession->y);
    SendBroadcast(pSession, &packetHeader, (char*)&sendMsg);

    const int centerX = atk.x;
    const int centerY = atk.y;

    for (auto& session : gSessionList)
    {
        if (session == pSession)
            continue;

        if (!IsInAttackRect(centerX, centerY,
            session->x, session->y,
            dfATTACK2_RANGE_X, dfATTACK2_RANGE_Y))
        {
            continue;
        }

        PacketHeader   dmgHeader;
        PacketSCDamage dmgMsg;

        session->HP -= dfATTACK2_DAMAGE;
        MakePacket_Damage(&dmgHeader, &dmgMsg,
            pSession->sessionID, session->sessionID,
            session->HP);
        SendBroadcast(nullptr, &dmgHeader, (char*)&dmgMsg);
    }

    return true;
}

//===============================================================
// Attack3 (중심 기준 더 넓은 직사각형, 직렬화 버퍼)
//===============================================================
bool NetPacketProc_Attack3(Session* pSession, CPacket& packet)
{
    PacketCSAttack3 atk{};
    packet >> atk.direction
        >> atk.x
        >> atk.y;

    {
        wchar_t buf[256];
        _snwprintf_s(buf, 256, _TRUNCATE,
            L"# PACKET_ATTACK3 # SessionID:%u / Direction:%u / X:%d / Y:%d",
            pSession->sessionID,
            atk.direction,
            atk.x,
            atk.y);
        Logger(buf);
    }

    if (abs(atk.x - pSession->x) > dfERROR_RANGE ||
        abs(atk.y - pSession->y) > dfERROR_RANGE)
    {
        wchar_t buf[256];
        _snwprintf_s(buf, 256, _TRUNCATE,
            L"Attack dfERROR_RANGE Fail ID=%d IP=%s",
            pSession->sessionID, pSession->ipStr);
        Logger(buf);
        Disconnect(pSession);
        return false;
    }

    pSession->direction = NormalizeViewDir(atk.direction);

    PacketHeader     packetHeader;
    PacketSCAttack3  sendMsg;
    MakePacket_Attack3(&packetHeader, &sendMsg,
        pSession->direction,
        pSession->sessionID,
        pSession->x, pSession->y);
    SendBroadcast(pSession, &packetHeader, (char*)&sendMsg);

    const int centerX = atk.x;
    const int centerY = atk.y;

    for (auto& session : gSessionList)
    {
        if (session == pSession)
            continue;

        if (!IsInAttackRect(centerX, centerY,
            session->x, session->y,
            dfATTACK3_RANGE_X, dfATTACK3_RANGE_Y))
        {
            continue;
        }

        PacketHeader   dmgHeader;
        PacketSCDamage dmgMsg;

        session->HP -= dfATTACK3_DAMAGE;
        MakePacket_Damage(&dmgHeader, &dmgMsg,
            pSession->sessionID, session->sessionID,
            session->HP);
        SendBroadcast(nullptr, &dmgHeader, (char*)&dmgMsg);
    }

    return true;
}

//===============================================================
// 에러 / 로그
//===============================================================
void ErrorHandler(const wchar_t* msg) noexcept
{
    int err = WSAGetLastError();
    wprintf(L"ERROR: %s, WSAGetLastError : %d\n", msg, err);
    ::WSACleanup();
    abort();
}

void Logger(const wchar_t* msg) noexcept
{
    wprintf(L"%s\n", msg);
}

//===============================================================
// 패킷 빌더
//===============================================================
void InitHeader(PacketHeader* pHeader, WORD type, WORD bodySize) noexcept
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
    pPacket->x = x;
    pPacket->y = y;
    pPacket->HP = HP;
}

void MakePacket_CreateOtherCharacter(PacketHeader* pHeader, PacketSCCreateOtherCharacter* pPacket, BYTE direction, DWORD ID, int x, int y, int HP)
{
    InitHeader(pHeader, dfPACKET_SC_CREATE_OTHER_CHARACTER, sizeof(PacketSCCreateOtherCharacter));
    pPacket->ID = ID;
    pPacket->direction = direction;
    pPacket->x = x;
    pPacket->y = y;
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
    pPacket->x = x;
    pPacket->y = y;
}

void MakePacket_MoveStop(PacketHeader* pHeader, PacketSCMoveStop* pPacket, DWORD ID, BYTE direction, int x, int y)
{
    InitHeader(pHeader, dfPACKET_SC_MOVE_STOP, sizeof(PacketSCMoveStop));
    pPacket->ID = ID;
    pPacket->direction = direction;
    pPacket->x = x;
    pPacket->y = y;
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
    pPacket->x = x;
    pPacket->y = y;
}

void MakePacket_Attack2(PacketHeader* pHeader, PacketSCAttack2* pPacket, BYTE direction, DWORD ID, int x, int y)
{
    InitHeader(pHeader, dfPACKET_SC_ATTACK2, sizeof(PacketSCAttack2));
    pPacket->ID = ID;
    pPacket->direction = direction;
    pPacket->x = x;
    pPacket->y = y;
}

void MakePacket_Attack3(PacketHeader* pHeader, PacketSCAttack3* pPacket, BYTE direction, DWORD ID, int x, int y)
{
    InitHeader(pHeader, dfPACKET_SC_ATTACK3, sizeof(PacketSCAttack3));
    pPacket->ID = ID;
    pPacket->direction = direction;
    pPacket->x = x;
    pPacket->y = y;
}

//===============================================================
// 송신 큐 처리
//===============================================================
bool EnqueuePacket(Session* session, PacketHeader* pHeader, char* pPacket) noexcept
{
    const int totalSize = static_cast<int>(sizeof(PacketHeader)) + pHeader->size;

    if (session->sendQ.GetFreeSize() < totalSize)
    {
        Logger(L"sendQ is full, disconnect");
        wchar_t buf[256];
        _snwprintf_s(buf, 256, _TRUNCATE,
            L"sendQ is full ID=%d IP=%s",
            session->sessionID, session->ipStr);
        Logger(buf);

        Disconnect(session);
        return false;
    }

    session->sendQ.Enqueue(reinterpret_cast<char*>(pHeader), sizeof(PacketHeader));
    session->sendQ.Enqueue(pPacket, pHeader->size);
    return true;
}

void SendUnicast(Session* pSession, PacketHeader* pHeader, char* pPacket) noexcept
{
    if (pSession == nullptr)
        return;

    EnqueuePacket(pSession, pHeader, pPacket);
}

void SendBroadcast(Session* pSession, PacketHeader* pHeader, char* pPacket) noexcept
{
    for (auto it = gSessionList.begin(); it != gSessionList.end(); )
    {
        Session* session = *it;
        ++it;

        if (session == pSession)
            continue;

        EnqueuePacket(session, pHeader, pPacket);
    }
}

//===============================================================
// Disconnect
//===============================================================
void Disconnect(Session* pSession) noexcept
{
    if (pSession == nullptr)
        return;

    PacketHeader        header;
    PacketSCDeleteCharacter packet;
    MakePacket_DeleteCharacter(&header, &packet, pSession->sessionID);

    SendBroadcast(pSession, &header, (char*)&packet);

    closesocket(pSession->socket);

    gSessionList.remove(pSession);
    delete pSession;
}

//===============================================================
// 방향/공통 헬퍼
//===============================================================
BYTE NormalizeViewDir(BYTE direction) noexcept
{
    switch (direction)
    {
    case dfPACKET_MOVE_DIR_RR:
    case dfPACKET_MOVE_DIR_RU:
    case dfPACKET_MOVE_DIR_RD:
        return dfPACKET_MOVE_DIR_RR;

    case dfPACKET_MOVE_DIR_LU:
    case dfPACKET_MOVE_DIR_LL:
    case dfPACKET_MOVE_DIR_LD:
        return dfPACKET_MOVE_DIR_LL;

    default:
        return dfPACKET_MOVE_DIR_RR;
    }
}

void GetMoveDelta(BYTE direction, int& dx, int& dy) noexcept
{
    dx = 0;
    dy = 0;

    switch (direction)
    {
    case dfPACKET_MOVE_DIR_UU: dy = -dfMOVE_Y; break;
    case dfPACKET_MOVE_DIR_DD: dy = +dfMOVE_Y; break;
    case dfPACKET_MOVE_DIR_RR: dx = +dfMOVE_X; break;
    case dfPACKET_MOVE_DIR_LL: dx = -dfMOVE_X; break;
    case dfPACKET_MOVE_DIR_RU: dx = +dfMOVE_X; dy = -dfMOVE_Y; break;
    case dfPACKET_MOVE_DIR_RD: dx = +dfMOVE_X; dy = +dfMOVE_Y; break;
    case dfPACKET_MOVE_DIR_LU: dx = -dfMOVE_X; dy = -dfMOVE_Y; break;
    case dfPACKET_MOVE_DIR_LD: dx = -dfMOVE_X; dy = +dfMOVE_Y; break;
    default:
        break;
    }
}

//===============================================================
// 공격 판정 헬퍼
//===============================================================
bool IsHitAttack1(const Session* attacker, const Session* target,
    int centerX, int centerY) noexcept
{
    if (abs(centerY - target->y) > dfATTACK1_RANGE_Y)
        return false;

    if (attacker->direction == dfPACKET_MOVE_DIR_RR)
    {
        // 오른쪽 : [centerX, centerX + RANGE_X]
        if (target->x < centerX || target->x > centerX + dfATTACK1_RANGE_X)
            return false;
    }
    else
    {
        // 왼쪽 : [centerX - RANGE_X, centerX]
        if (target->x > centerX || target->x < centerX - dfATTACK1_RANGE_X)
            return false;
    }

    return true;
}

bool IsInAttackRect(int centerX, int centerY,
    int targetX, int targetY,
    int rangeX, int rangeY) noexcept
{
    if (abs(centerX - targetX) > rangeX)
        return false;
    if (abs(centerY - targetY) > rangeY)
        return false;
    return true;
}
