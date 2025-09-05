#pragma once
#include <Windows.h>
#include "Console.h"

// ----------------------------
// 게임 전역 상수 정의
// ----------------------------
constexpr int    kMaxEnemies = 1000;             // 최대 적 개수
constexpr int    kMaxBullets = 100;              // 최대 총알 개수
constexpr int    kBulletDamage = 1;              // 총알이 적에게 주는 피해량
constexpr int    kEnemyDamage = 1;               // 적이 플레이어에게 주는 피해량
constexpr int    kPlayerMaxHP = 5;               // 플레이어 최대 체력
constexpr char   kPlayerShape = 'A';             // 플레이어 표시 문자
constexpr char   kBulletShape = 'I';             // 총알 표시 문자
constexpr int    kMaxPatternCnt = 30;            // 적 패턴 최대 개수

// ----------------------------
// 화면 및 프레임 관련
// ----------------------------
constexpr int    kPlayerStartPosX = dfSCREEN_WIDTH / 2;		// 플레이어 시작 X 좌표 (가운데)
constexpr int    kPlayerStartPosY = dfSCREEN_HEIGHT - 1;	// 플레이어 시작 Y 좌표 (화면 하단)
constexpr int    kFps = 50;									// 목표 FPS (초당 프레임 수)
constexpr double kFrameTimeMs = 1000.0 / kFps;				// 1프레임 시간 (밀리초 단위, 약 20ms)
constexpr double kFrameTimeSec = 1.0 / kFps;				// 1프레임 시간 (초 단위)

// ----------------------------
// 플레이어 관련
// ----------------------------
constexpr int    kPlayerMoveSpeedCellsPerFrame = 5; // 플레이어 이동 속도 (프레임 단위, 5프레임마다 1칸 이동)
constexpr int    kPlayerAttackSpeed = 8;            // 플레이어 공격 속도 (총알 발사 간격/프레임)
constexpr int    kPlayerInvincibleFrames = 30;      // 피격 후 무적 유지 프레임 수
constexpr char   kPlayerInvincibleShape = 'B';      // 무적 상태일 때 플레이어 표시 문자

// ----------------------------
// 총알/로딩 관련
// ----------------------------
constexpr int    kDefaultBulletSpeed = 5;        // 총알 기본 속도 (프레임 단위, 5프레임마다 1칸 이동)
constexpr int    kMinWaitTimeLoadingScene = 1;   // 로딩 씬 최소 대기 시간(초 단위)
constexpr int    kLoadingBarLength = 10;         // 로딩 바 길이 (칸 수)

// ----------------------------
// 외부 데이터 파일 경로
// ----------------------------
constexpr const char* g_stageInfoPath = "stage_info.csv"; // 스테이지 정보 CSV 파일 경로
constexpr const char* g_enemyInfoPath = "enemy_info.csv"; // 적 정보 CSV 파일 경로

// ----------------------------
// 입력 가상 키
// ----------------------------
constexpr char kGameStart = 'Z';        // 게임 시작
constexpr char kAttack = 'Z';           // 공격
constexpr char kSceneQuit = 'Q';        // 씬 종료
constexpr int kUpKey = VK_UP;           // ↑
constexpr int kDownKey = VK_DOWN;       // ↓
constexpr int kLeftKey = VK_LEFT;       // ←
constexpr int kRightKey = VK_RIGHT;     // →
constexpr int kGameExit = VK_ESCAPE;    // ESC = 게임 종료

// 키 배열
constexpr int kKeyArray[] = {
    kGameStart,
    kAttack,
    kUpKey,
    kDownKey,
    kLeftKey,
    kRightKey,
    kSceneQuit,
    kGameExit
};