#pragma once
#include "Console.h"

constexpr int    kMaxEnemies = 1000;
constexpr int    kMaxBullets = 100;
constexpr int    kBulletDamage = 1;
constexpr int    kEnemyDamage = 1;
constexpr int    kPlayerMaxHP = 5;
constexpr char   kPlayerShape = 'A';
constexpr char   kBulletShape = 'I';
constexpr int    kMaxPatternCnt = 30;
constexpr int    kPlayerStartPosX = dfSCREEN_WIDTH / 2;
constexpr int    kPlayerStartPosY = dfSCREEN_HEIGHT - 1;
constexpr int    kFps = 50;
constexpr double kFrameTimeMs = 1000.0 / kFps;  // 1프레임 = 약 20ms
constexpr double kFrameTimeSec = 1.0 / kFps;
constexpr int    kPlayerMoveSpeedCellsPerFrame = 7;
constexpr int    kPlayerAttackSpeed = 8;
constexpr int    kPlayerInvincibleFrames = 30; // 데미지 받고 몇 프레임 무적으로 할것인지
constexpr char   kPlayerInvincibleShape = 'B';
constexpr int    kDefaultBulletSpeed = 5;
constexpr int    kMinWaitTimeLoadingScene = 1;
constexpr int    kLoadingBarLength = 10;

constexpr const char* g_stageInfoPath = "stage_info.csv";
constexpr const char* g_enemyInfoPath = "enemy_info.csv";
