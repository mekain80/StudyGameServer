#include <Windows.h>

/*
    패킷 데이터 정의

    자신의 캐릭터에 대한 패킷은 클라이언트가 먼저 액션을 수행한 뒤
    즉시 서버로 전송한다.

    - 이동 키 입력 시 이동 동작과 동시에 이동 패킷을 전송한다.
    - 공격 키 입력 시 공격 동작과 동시에 공격 패킷을 전송한다.
    - 충돌 처리와 데미지 결과는 서버가 계산한 뒤 통보한다.
*/

#ifndef __PACKET_DEFINE__
#define __PACKET_DEFINE__

// 패킷 헤더
/*
    BYTE	byCode;			// 패킷 코드. 0x89 고정
    BYTE	bySize;			// 패킷 크기
    BYTE	byType;			// 패킷 타입
*/


#define	dfPACKET_SC_CREATE_MY_CHARACTER			0
// 클라이언트 자신의 캐릭터 생성 패킷 (Server -> Client)
// 서버 접속 직후 한 번 수신하며, 자신의 ID / 초기 위치 / HP 정보를 담는다.
// 이 패킷을 받으면 ID, X, Y, HP를 저장하고 캐릭터를 생성해야 한다.
//
//	4	-	ID
//	1	-	Direction (LL / RR)
//	2	-	X
//	2	-	Y
//	1	-	HP (100)


#define	dfPACKET_SC_CREATE_OTHER_CHARACTER		1
// 다른 클라이언트의 캐릭터 생성 패킷 (Server -> Client)
// 처음 접속했을 때 이미 존재하던 캐릭터 정보 또는
// 게임 도중 새로 접속한 클라이언트의 생성 정보를 전달한다.
//
//	4	-	ID
//	1	-	Direction (LL / RR)
//	2	-	X
//	2	-	Y
//	1	-	HP


#define	dfPACKET_SC_DELETE_CHARACTER			2
// 캐릭터 삭제 패킷 (Server -> Client)
// 캐릭터가 접속을 종료했거나 사망했을 때 전송된다.
//
//	4	-	ID


#define	dfPACKET_CS_MOVE_START					10
// 캐릭터 이동 시작 패킷 (Client -> Server)
// 자신의 캐릭터가 이동을 시작할 때 전송한다.
// 이동 중에는 계속 보내지 않으며, 입력 방향이 바뀌는 경우에만 다시 전송한다.
// 예: 왼쪽 이동 중 위 입력, 왼쪽 이동 중 왼쪽 위 입력
//
//	1	-	Direction (방향 define 값, 8방향 사용)
//	2	-	X
//	2	-	Y

#define dfPACKET_MOVE_DIR_LL					0
#define dfPACKET_MOVE_DIR_LU					1
#define dfPACKET_MOVE_DIR_UU					2
#define dfPACKET_MOVE_DIR_RU					3
#define dfPACKET_MOVE_DIR_RR					4
#define dfPACKET_MOVE_DIR_RD					5
#define dfPACKET_MOVE_DIR_DD					6
#define dfPACKET_MOVE_DIR_LD					7


#define	dfPACKET_SC_MOVE_START					11
// 캐릭터 이동 시작 패킷 (Server -> Client)
// 다른 유저의 캐릭터가 이동을 시작할 때 수신한다.
// 수신 시 해당 캐릭터를 찾아 이동 상태로 전환하고,
// 해당 방향 키가 계속 눌린 상태처럼 지속 이동 처리해야 한다.
//
//	4	-	ID
//	1	-	Direction (방향 define 값, 8방향 사용)
//	2	-	X
//	2	-	Y


#define	dfPACKET_CS_MOVE_STOP					12
// 캐릭터 이동 중지 패킷 (Client -> Server)
// 이동 중 키보드 입력이 사라져 정지했을 때 전송한다.
// 이동 중 방향 전환 시에는 stop 패킷을 보내지 않는다.
//
//	1	-	Direction (방향 define 값, 좌/우만 사용)
//	2	-	X
//	2	-	Y


#define	dfPACKET_SC_MOVE_STOP					13
// 캐릭터 이동 중지 패킷 (Server -> Client)
// ID에 해당하는 캐릭터가 이동을 멈췄음을 의미한다.
// 해당 캐릭터를 찾아 방향과 좌표를 갱신한 뒤 정지 처리한다.
//
//	4	-	ID
//	1	-	Direction (방향 define 값, 좌/우만 사용)
//	2	-	X
//	2	-	Y


#define	dfPACKET_CS_ATTACK1						20
// 캐릭터 공격 1 패킷 (Client -> Server)
// 공격 키 입력 시 서버로 전송한다.
// 충돌 및 데미지 결과는 서버가 계산해 통보한다.
// 공격 동작이 시작될 때 한 번만 전송해야 한다.
//
//	1	-	Direction (방향 define 값, 좌/우만 사용)
//	2	-	X
//	2	-	Y

#define	dfPACKET_SC_ATTACK1						21
// 캐릭터 공격 1 패킷 (Server -> Client)
// 수신 시 해당 캐릭터를 찾아 공격 1 동작을 수행시킨다.
// 방향이 다르면 방향을 먼저 맞춘 뒤 처리한다.
//
//	4	-	ID
//	1	-	Direction (방향 define 값, 좌/우만 사용)
//	2	-	X
//	2	-	Y


#define	dfPACKET_CS_ATTACK2						22
// 캐릭터 공격 2 패킷 (Client -> Server)
// 공격 키 입력 시 서버로 전송한다.
// 충돌 및 데미지 결과는 서버가 계산해 통보한다.
// 공격 동작이 시작될 때 한 번만 전송해야 한다.
//
//	1	-	Direction (방향 define 값, 좌/우만 사용)
//	2	-	X
//	2	-	Y

#define	dfPACKET_SC_ATTACK2						23
// 캐릭터 공격 2 패킷 (Server -> Client)
// 수신 시 해당 캐릭터를 찾아 공격 2 동작을 수행시킨다.
// 방향이 다르면 방향을 먼저 맞춘 뒤 처리한다.
//
//	4	-	ID
//	1	-	Direction (방향 define 값, 좌/우만 사용)
//	2	-	X
//	2	-	Y


#define	dfPACKET_CS_ATTACK3						24
// 캐릭터 공격 3 패킷 (Client -> Server)
// 공격 키 입력 시 서버로 전송한다.
// 충돌 및 데미지 결과는 서버가 계산해 통보한다.
// 공격 동작이 시작될 때 한 번만 전송해야 한다.
//
//	1	-	Direction (방향 define 값, 좌/우만 사용)
//	2	-	X
//	2	-	Y

#define	dfPACKET_SC_ATTACK3						25
// 캐릭터 공격 3 패킷 (Server -> Client)
// 수신 시 해당 캐릭터를 찾아 공격 3 동작을 수행시킨다.
// 방향이 다르면 방향을 먼저 맞춘 뒤 처리한다.
//
//	4	-	ID
//	1	-	Direction (방향 define 값, 좌/우만 사용)
//	2	-	X
//	2	-	Y


#define	dfPACKET_SC_DAMAGE						30
// 캐릭터 데미지 패킷 (Server -> Client)
// 공격에 맞은 캐릭터의 결과 정보를 전달한다.
//
//	4	-	AttackID (공격자 ID)
//	4	-	DamageID (피해자 ID)
//	1	-	DamageHP (피해자의 남은 HP)


// 현재 사용하지 않음
#define	dfPACKET_CS_SYNC						250
// 동기화 패킷 (Client -> Server)
//
//	2	-	X
//	2	-	Y


#define	dfPACKET_SC_SYNC						251
// 동기화 패킷 (Server -> Client)
// 서버에서 동기화 패킷을 받으면 해당 캐릭터를 찾아
// 좌표를 보정한다.
//
//	4	-	ID
//	2	-	X
//	2	-	Y

#pragma pack(push, 1)

// 네트워크 패킷 헤더
struct PacketHeader
{
    BYTE code = 0x89;   // 0x89 고정
    BYTE size;          // 이 패킷(페이로드) 전체 길이
    BYTE type;          // 패킷 타입
};


// 클라이언트 자신의 캐릭터 생성 패킷 (Server -> Client)
struct PacketSCCreateMyCharacter
{
    UINT ID;
    BYTE direction;
    WORD x;
    WORD y;
    BYTE HP;
};


// 다른 클라이언트의 캐릭터 생성 패킷 (Server -> Client)
struct PacketSCCreateOtherCharacter
{
    UINT ID;
    BYTE direction;
    WORD x;
    WORD y;
    BYTE HP;
};


// 캐릭터 삭제 패킷 (Server -> Client)
struct PacketSCDeleteCharacter
{
    UINT ID;
};


// 캐릭터 이동 시작 패킷 (Client -> Server)
struct PacketCSMoveStart
{
    BYTE direction;
    WORD x;
    WORD y;
};


// 캐릭터 이동 시작 패킷 (Server -> Client)
struct PacketSCMoveStart
{
    UINT ID;
    BYTE direction; // pSession->action(이동 방향)
    WORD x;
    WORD y;
};


// 캐릭터 이동 중지 패킷 (Client -> Server)
struct PacketCSMoveStop
{
    BYTE direction;
    WORD x;
    WORD y;
};


// 캐릭터 이동 중지 패킷 (Server -> Client)
struct PacketSCMoveStop
{
    UINT ID;
    BYTE direction;
    WORD x;
    WORD y;
};


// 캐릭터 공격 1 패킷 (Client -> Server)
struct PacketCSAttack1
{
    BYTE direction;
    WORD x;
    WORD y;
};


// 캐릭터 공격 1 패킷 (Server -> Client)
struct PacketSCAttack1
{
    UINT ID;
    BYTE direction;
    WORD x;
    WORD y;
};


// 캐릭터 공격 2 패킷 (Client -> Server)
struct PacketCSAttack2
{
    BYTE direction;
    WORD x;
    WORD y;
};


// 캐릭터 공격 2 패킷 (Server -> Client)
struct PacketSCAttack2
{
    UINT ID;
    BYTE direction;
    WORD x;
    WORD y;
};


// 캐릭터 공격 3 패킷 (Client -> Server)
struct PacketCSAttack3
{
    BYTE direction;
    WORD x;
    WORD y;
};


// 캐릭터 공격 3 패킷 (Server -> Client)
struct PacketSCAttack3
{
    UINT ID;
    BYTE direction;
    WORD x;
    WORD y;
};


// 캐릭터 데미지 패킷 (Server -> Client)
struct PacketSCDamage
{
    UINT attackID;
    UINT damageID;
    BYTE damageHP;
};

#pragma pack(pop)

#endif
