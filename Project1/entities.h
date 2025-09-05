#pragma once

// ----------------------------
// 플레이어 정보 구조체
// ----------------------------
struct Player {
    int hp;                 // 플레이어 체력
    int x, y;               // 현재 좌표
    bool isVisible = false; // 화면에 표시 여부 (죽었을 때/숨겨질 때 false)
    int movedTick = 0;      // 마지막으로 이동한 시각(프레임 단위)
    int atkedTick = 0;      // 마지막 공격 시각(프레임 단위)
    int damagedTick = 0;    // 마지막 피격 시각(프레임 단위)
};

// ----------------------------
// 적 정보 구조체
// ----------------------------
struct Enemy {
    int hp;                     // 적 체력
    int x, y;                   // 현재 좌표
    int type;                   // 적 종류
    bool isVisible = false;     // 화면에 표시 여부 (죽었거나 아직 등장 전이면 false)
    int moveSpeed = 0;          // 이동 속도 (프레임 단위, n프레임마다 1칸 이동)
    int atkSpeed = 0;           // 공격 속도 (공격 간격 프레임 단위)
    int currMovePattern = 0;    // 현재 적용 중인 이동 패턴 인덱스
    int movedTick = 0;          // 마지막 이동 시각(프레임 단위)
    int atkedTick = 0;          // 마지막 공격 시각(프레임 단위)
    char shape;                 // 화면에 출력할 적 문자 모양
};

// ----------------------------
// 총알 정보 구조체
// ----------------------------
struct Bullet {
    int x, y;               // 현재 좌표
    bool isEnemy;           // true = 적 총알, false = 플레이어 총알
    bool isVisible = false; // 화면에 표시 여부 (발사되면 true, 소멸 시 false)
    int speed;              // 총알 속도 (프레임 단위, n프레임마다 1칸 이동)
    int movedTick = 0;      // 마지막 이동 시각(프레임 단위, 이동 간격 제어)
};
