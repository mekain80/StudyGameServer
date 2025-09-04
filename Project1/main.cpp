#include <stdio.h>
#include <memory.h>
#include <Windows.h>
#include "Console.h"
#include <stdlib.h> 
#include "csvLoader.h" 
#pragma comment(lib, "winmm.lib")

enum gameState { TITLE, LOADING, PLAY, GAME_OVER, GAME_CLEAR, EXIT };
gameState GAME_STATE;

const int kMaxEnemies = 1000;
const int kMaxBullets = 100;
const int kBulletDamage = 1;
const int kEnemyDamage = 1;
const int kPlayerMaxHP = 5;
const char kPlayerShape = 'A';
const char kBulletShape = 'I';
const int kMaxPatternCnt = 30;
const int kPlayerStartPosX = dfSCREEN_WIDTH / 2;
const int kPlayerStartPosY = dfSCREEN_HEIGHT - 1;
const int kFps = 50;
const double kFrameTimeMs = 1000 / kFps; // 1프레임 = 약 20ms
const double kFrameTimeSec = 1.0 / kFps;
const int kPlayerSpeedCellsPerFrame = 2;
const int kPlayerAttackSpeed = 1; // TODO 약 8로 조정, 현재는 테스트를 위해 1로 설정
const int kPlayerBulletSpeed = 2;
const int kPlayerInvincibleFrames = 30; // 데미지 받고 몇 프레임 무적으로 할것인지
const char kPlayerInvincibleShape = 'B';
const int kDefaultBulletSpeed = 5;
const int kMinWaitTimeLoadingScene = 1;


size_t g_maxStage = 0;
int g_stage = 0;
int g_logicFpsCnt = 0;
int g_renderFpsCnt = 0;
bool g_isSceneBegin = true;
LARGE_INTEGER g_freq, g_frameStartTime;
LARGE_INTEGER g_sceneBeginTime;
const char g_stageInfoPath[] = "stage_info.csv";
const char g_enemyInfoPath[] = "enemy_info.csv";


struct Player
{
	int hp;
	int x, y;
	bool isVisible = false;
	int movedTick = 0;
	int atkedTick = 0;
	int damagedTick = 0;
};

struct Enemy
{
	int hp;
	int x, y;
	int type;
	bool isVisible = false;
	int moveSpeed = 0;
	int atkSpeed = 0;
	int currMovePattern = 0; 
	int movedTick = 0;
	int atkedTick = 0;
	char shape;
};

struct Bullet
{
	int x, y;
	bool isEnemy;
	bool isVisible = false;
	int speed = kDefaultBulletSpeed;
	int movedTick = 0;
};

// 변수
Player player;
Enemy enemies[kMaxEnemies];
Bullet bullets[kMaxBullets];

void titleScene();
void loadingScene();
void playScene();
void gameOverScene();
void gameClearScene();

void changeGameState(gameState);
void clearHUD();

void frameTest();

//--------------------------------------------------------------------
// 화면 깜빡임을 없애기 위한 화면 버퍼.
// 게임이 진행되는 상황을 매번 화면을 지우고 비행기 찍고, 지우고 찍고,
// 하게 되면 화면이 깜빡깜빡 거리게 된다.
//
// 그러므로 화면과 똑같은 크기의 메모리를 할당한 다음에 화면에 바로 찍지않고
// 메모리(버퍼)상에 그림을 그리고 메모리의 화면을 그대로 화면에 찍어준다.
//
// 이렇게 해서 화면을 매번 지우고, 그리고, 지우고, 그리고 하지 않고
// 메모리(버퍼)상의 그림을 화면에 그리는 작업만 하게 되어 깜박임이 없어진다.
//
// 버퍼의 각 줄 마지막엔 NULL 을 넣어 문자열로서 처리하며, 
// 한줄한줄을 printf 로 찍어나갈 것이다.
//
// for ( N = 0 ~ height )
// {
// 	  cs_MoveCursor(0, N);
//    printf(szScreenBuffer[N]);
// }
//
// 줄바꿈에 printf("\n") 을 쓰지 않고 커서좌표를 이동하는 이유는
// 화면을 꽉 차게 출력하고 줄바꿈을 하면 2칸이 내려가거나 화면이 밀릴 수 있으므로
// 매 줄 출력마다 좌표를 강제로 이동하여 확실하게 출력한다.
//--------------------------------------------------------------------
char szScreenBuffer[dfSCREEN_HEIGHT][dfSCREEN_WIDTH];


//--------------------------------------------------------------------
// GetAsyncKeyState(int iKey)  #include <Windows.h>
//
// 윈도우 API 로 키보드가 눌렸는지를 확인한다.
// 인자로 키보드 버튼에 대한 디파인 값을 넣으면 해당 키가 눌렸는지 (눌렸던적이 있는지) 를 확인 해준다.
// 모든 키에대한 확인이 가능하고, 논블럭 체크가 되므로 게임에서도 쓰기 좋다.
//
// Virtual-Key Codes
//
// VK_SPACE / VK_ESCAPE / VK_LEFT / VK_UP / 키보드 문자는 대문자 아스키 코드와 같음.
// winuser.h 파일에 위와 같이 디파인 되어 있다.
//
//
// GetAsyncKeyState(VK_LEFT) 호출시 결과값은
//
// 0x0001  > *이전 체크 이후 눌린적이 있음
// 0x8000  > 지금 눌려있음
// 0x8001  > *이전 체크 이후 눌린적도 있고 지금도 눌려 있음
//
// * 이전 체크라는건 이전에 GetAsyncKeyState 를 호출한 때를 말 한다.
// 
// 10프레임 짜리 게임이라면 1초에 10회의 키 체크를 하게 되므로 체크 간격은 20ms 가 된다.
// 빠른 커맨드 입력이 필요한 게임에서는 20ms 이내에 여러개의 키입력이 있다면 체크하지 못하는 키 입력이 발생 할 수 있다.
// 그래서 0x0001 비트에 대한 처리도 필요하다.
//


//--------------------------------------------------------------------
// 버퍼의 내용을 화면으로 찍어주는 함수.
//
// 적군,아군,총알 등을 szScreenBuffer 에 넣어주고, 
// 1 프레임이 끝나는 마지막에 본 함수를 호출하여 버퍼 -> 화면 으로 그린다.
//--------------------------------------------------------------------
void Buffer_Flip(void);
//--------------------------------------------------------------------
// 화면 버퍼를 지워주는 함수
//
// 매 프레임 그림을 그리기 직전에 버퍼를 지워 준다. 
// 안그러면 이전 프레임의 잔상이 남으니까
//--------------------------------------------------------------------
void Buffer_Clear(void);

//--------------------------------------------------------------------
// 버퍼의 특정 위치에 원하는 문자를 출력.
//
// 입력 받은 X,Y 좌표에 아스키코드 하나를 출력한다. (버퍼에 그림)
//--------------------------------------------------------------------
void Sprite_Draw(int iX, int iY, char chSprite);



void main(void)
{
	timeBeginPeriod(1);
	QueryPerformanceFrequency(&g_freq);
	QueryPerformanceCounter(&g_frameStartTime);

	LARGE_INTEGER end;

	cs_Initial();

	//--------------------------------------------------------------------
	// 게임의 메인 루프
	// 이 루프가  1번 돌면 1프레임 이다.
	//--------------------------------------------------------------------
	while (1)
	{
		Buffer_Clear();
		switch (GAME_STATE)
		{
		case TITLE:
			titleScene();
			break;
		case LOADING:
			loadingScene();
			break;
		case PLAY:
			playScene();
			break;
		case GAME_OVER:
			gameOverScene();
			break;
		case GAME_CLEAR:
			gameClearScene();
			break;
		case EXIT:
			return;
		}

		QueryPerformanceCounter(&end);
		double elapsed = static_cast<double>(end.QuadPart - g_frameStartTime.QuadPart) / g_freq.QuadPart;

		if (elapsed < kFrameTimeSec) {
			DWORD sleepTime = static_cast<DWORD>((kFrameTimeSec - elapsed) * 1000.0);
			Sleep(sleepTime);
		}
		g_frameStartTime.QuadPart += static_cast<LONGLONG>(kFrameTimeSec * g_freq.QuadPart);

		Buffer_Flip();
	}

	return;
}



//--------------------------------------------------------------------
// 버퍼의 내용을 화면으로 찍어주는 함수.
//
// 적군,아군,총알 등을 szScreenBuffer 에 넣어주고, 
// 1 프레임이 끝나는 마지막에 본 함수를 호출하여 버퍼 -> 화면 으로 그린다.
//--------------------------------------------------------------------
void Buffer_Flip(void)
{
	int iCnt;
	for (iCnt = 0; iCnt < dfSCREEN_HEIGHT; iCnt++)
	{
		cs_MoveCursor(0, iCnt);
		printf(szScreenBuffer[iCnt]);
	}
}


//--------------------------------------------------------------------
// 화면 버퍼를 지워주는 함수
//
// 매 프레임 그림을 그리기 직전에 버퍼를 지워 준다. 
// 안그러면 이전 프레임의 잔상이 남으니까
//--------------------------------------------------------------------
void Buffer_Clear(void)
{
	int iCnt;
	memset(szScreenBuffer, ' ', dfSCREEN_WIDTH * dfSCREEN_HEIGHT);

	for (iCnt = 0; iCnt < dfSCREEN_HEIGHT; iCnt++)
	{
		szScreenBuffer[iCnt][dfSCREEN_WIDTH - 1] = '\0';
	}

}

//--------------------------------------------------------------------
// 버퍼의 특정 위치에 원하는 문자를 출력.
//
// 입력 받은 X,Y 좌표에 아스키코드 하나를 출력한다. (버퍼에 그림)
//--------------------------------------------------------------------
void Sprite_Draw(int iX, int iY, char chSprite)
{
	if (iX < 0 || iY < 0 || iX >= dfSCREEN_WIDTH - 1 || iY >= dfSCREEN_HEIGHT)
		return;

	szScreenBuffer[iY][iX] = chSprite;
}


void titleScene() {
	LARGE_INTEGER logicBeginTicks;
	QueryPerformanceCounter(&logicBeginTicks);

	// 1. 키보드 입력부
	if (GetAsyncKeyState('A') != 0x00) {
		changeGameState(LOADING);
	}
	if (GetAsyncKeyState(VK_ESCAPE) != 0) {
		changeGameState(EXIT);
	}

	// 2. 로직부 
	++g_logicFpsCnt;

	// 3. 랜더부

	// 한 프레임 이상 지연된 경우, 프레임 스킵
	const double secsSinceLastFrame = static_cast<double>(logicBeginTicks.QuadPart - g_frameStartTime.QuadPart) / static_cast<double>(g_freq.QuadPart);
	if (kFrameTimeSec < secsSinceLastFrame) {
		g_frameStartTime.QuadPart += static_cast<LONGLONG>(kFrameTimeSec * g_freq.QuadPart);
		return;
	}

	char titleText[] = "Terminal Shooter Game";
	char startGuideText[] = "Press the A key to start the game.";

	// 문자열 길이 (null 제외)
	int titleLength = sizeof(titleText) - 1;
	int startGuideLength = sizeof(startGuideText) - 1;

	// 중앙 좌표 계산
	int titleStartX = (dfSCREEN_WIDTH - titleLength) / 2;
	int titleStartY = (dfSCREEN_HEIGHT / 2) - 2;
	int startGuideStartX = (dfSCREEN_WIDTH - startGuideLength) / 2;
	int startGuideStartY = (dfSCREEN_HEIGHT / 2) + 5;

	// "Terminal Shooter Game" 출력
	// "Press the A key to start the game." 출력
	for (int i = 0; i < titleLength; i++) {
		Sprite_Draw(titleStartX + i, titleStartY, titleText[i]);
	}
	for (int i = 0; i < startGuideLength; i++) {
		Sprite_Draw(startGuideStartX + i, startGuideStartY, startGuideText[i]);
	}
	++g_renderFpsCnt;
}

void loadingScene() {
	LARGE_INTEGER logicBeginTicks;
	QueryPerformanceCounter(&logicBeginTicks);

	if (g_isSceneBegin) {
		g_sceneBeginTime = logicBeginTicks;
	}

	// 1. 키보드 입력부

	// 2. 로직부 
	char stage_name[256];
	if (g_isSceneBegin == true) {
		++g_stage;
		if (csvGetValueInTable(g_stageInfoPath, g_stage, "stage_name", stage_name, sizeof(stage_name))) {
			csvLoadAll(stage_name);
			if (g_maxStage == 0) {
				getCsvIdArray(g_stageInfoPath, g_maxStage);
			}
		}

		csvLoadAll(g_enemyInfoPath);
		g_isSceneBegin = false;
	}
	const double sceneElapedtime = static_cast<double>(logicBeginTicks.QuadPart - g_sceneBeginTime.QuadPart) / static_cast<double>(g_freq.QuadPart);

	// 한 프레임 이상 지연된 경우, 프레임 스킵
	const double secsSinceLastFrame = static_cast<double>(logicBeginTicks.QuadPart - g_frameStartTime.QuadPart) / static_cast<double>(g_freq.QuadPart);
	if (kFrameTimeSec < secsSinceLastFrame) {
		g_frameStartTime.QuadPart += static_cast<LONGLONG>(kFrameTimeSec * g_freq.QuadPart);
		return;
	}

	// 3. 랜더부
	char loadingText[64];
	if (g_maxStage == g_stage - 2) {
		snprintf(loadingText, sizeof(loadingText), "LAST STAGE, NOW LOADING...");
	} else {
		snprintf(loadingText, sizeof(loadingText), "STAGE %d, NOW LOADING...", g_stage);
	}

	int loadingTextLength = strlen(loadingText);

	// 중앙 좌표 계산
	int loadingStartX = (dfSCREEN_WIDTH - loadingTextLength) / 2;
	int loadingStartY = (dfSCREEN_HEIGHT / 2);

	for (int i = 0; i < loadingTextLength; i++) {
		Sprite_Draw(loadingStartX + i, loadingStartY, loadingText[i]);
	}

	// 단순히 최소 시간으로만 로딩 진척도 구현
	const int loadingBarLength = 10;
	const int loadingBarStartX = (dfSCREEN_WIDTH - loadingBarLength) / 2;
	const int loadingBarStartY = (dfSCREEN_HEIGHT / 2) + 2;

	// 경과 시간 비율 계산
	float ratio = (float)sceneElapedtime / (float)kMinWaitTimeLoadingScene;
	if (ratio > 1.f) ratio = 1.f;
	if (ratio < 0.f) ratio = 0.f;
	int filled = (int)(ratio * loadingBarLength);

	// 출력
	for (int i = 0; i < loadingBarLength; i++) {
		Sprite_Draw(loadingBarStartX + i, loadingBarStartY, (i < filled) ? '#' : '.');
	}

	// 로딩 창 최소 시간 초과 & 데이터 로드 완료 시 PLAY SCENE으로 전환
	if (kMinWaitTimeLoadingScene < sceneElapedtime && csvLoadAll(g_stageInfoPath) && csvLoadAll(g_enemyInfoPath)) {
		changeGameState(PLAY);
		return;
	}
}

void playScene() {
	LARGE_INTEGER logicBeginTicks;
	QueryPerformanceCounter(&logicBeginTicks);


	// 스테이지 시작 판정
	if (g_isSceneBegin == true) {
		player.hp = kPlayerMaxHP;
		player.isVisible = true;
		player.x = kPlayerStartPosX;
		player.y = kPlayerStartPosY;

		char stageName[256];
		csvGetValueInTable(g_stageInfoPath, g_stage, "stage_name", stageName, sizeof(stageName));
		csvLoadAll(stageName);

		size_t enemyIArraySize;
		int* enemyInfoArray = getCsvIdArray(stageName, enemyIArraySize);
		for (size_t i = 0; i < enemyIArraySize; i++)
		{
			int id = enemyInfoArray[i];
			char buf[256];
			csvGetValueInTable(stageName, id, "x", buf, sizeof(buf));
			enemies[i].x = atoi(buf);
			csvGetValueInTable(stageName, id, "y", buf, sizeof(buf));
			enemies[i].y = atoi(buf);
			csvGetValueInTable(stageName, id, "type", buf, sizeof(buf));
			int type = atoi(buf);
			enemies[i].type = type;

			csvGetValueInTable(g_enemyInfoPath, type, "hp", buf, sizeof(buf));
			enemies[i].hp = atoi(buf);
			csvGetValueInTable(g_enemyInfoPath, type, "move_speed", buf, sizeof(buf));
			enemies[i].moveSpeed = atoi(buf);
			csvGetValueInTable(g_enemyInfoPath, type, "atk_speed", buf, sizeof(buf));
			enemies[i].atkSpeed = atoi(buf);
			csvGetValueInTable(g_enemyInfoPath, type, "shape", buf, sizeof(buf));
			enemies[i].shape = buf[0];

			enemies[i].isVisible = true;
		}

		for (size_t i = 0; i < kMaxBullets; i++)
		{
			bullets[i].isVisible = false;
		}

		g_isSceneBegin = false;
	}


	// 1. 키보드 입력부
	SHORT upKey = GetAsyncKeyState(VK_UP);
	SHORT downKey = GetAsyncKeyState(VK_DOWN);
	SHORT rightKey = GetAsyncKeyState(VK_RIGHT);
	SHORT leftKey = GetAsyncKeyState(VK_LEFT);
	if (player.movedTick + kPlayerSpeedCellsPerFrame < g_logicFpsCnt) {
		if (upKey != 0 && downKey != 0) {}
		else if (upKey != 0) {
			if (player.y - 1 >= 0) {
				player.y -= 1;
				player.movedTick = g_logicFpsCnt;
			}
		}
		else if (downKey != 0) {
			if (player.y + 1 < dfSCREEN_HEIGHT) {
				player.y += 1;
				player.movedTick = g_logicFpsCnt;
			}
		}

		if (rightKey != 0 && leftKey != 0) {}
		else if (rightKey != 0) {
			if (player.x + 1 < dfSCREEN_WIDTH - 1) {
				player.x += 1;
				player.movedTick = g_logicFpsCnt;
			}
		}
		else if (leftKey != 0) {
			if (player.x - 1 >= 0) {
				player.x -= 1;
				player.movedTick = g_logicFpsCnt;
			}
		}
	}
	// 총알 충돌 확인
	for (size_t i = 0; i < kMaxBullets; i++)
	{
		if (bullets[i].isVisible) {
			if (player.isVisible) {
				if (bullets[i].isEnemy && bullets[i].x == player.x && bullets[i].y == player.y && player.damagedTick + kPlayerInvincibleFrames <= g_logicFpsCnt) {
					player.hp -= kBulletDamage;
					player.damagedTick = g_logicFpsCnt;
					if (player.hp <= 0) {
						player.isVisible = false;
						changeGameState(GAME_OVER);
					}
					bullets[i].isVisible = false;
				}
			}
		}
	}
	// 적과의 충돌 확인 로직
	for (size_t i = 0; i < kMaxEnemies; i++)
	{
		if (enemies[i].isVisible) {
			if (player.isVisible) {
				if (enemies[i].x == player.x && enemies[i].y == player.y && player.damagedTick + kPlayerInvincibleFrames <= g_logicFpsCnt) {
					player.hp -= kEnemyDamage;
					player.damagedTick = g_logicFpsCnt;
					if (player.hp <= 0) {
						player.isVisible = false;
						changeGameState(GAME_OVER);
					}
				}
			}
		}
	}

	// 총알 발사
	if (GetAsyncKeyState('Z') != 0 and player.atkedTick + kPlayerAttackSpeed < g_logicFpsCnt) {
		for (size_t i = 0; i < kMaxBullets; i++)
		{
			if (bullets[i].isVisible == false) {
				bullets[i].isVisible = true;
				bullets[i].x = player.x;
				bullets[i].y = player.y - 1;
				bullets[i].isEnemy = false;
				bullets[i].movedTick = g_logicFpsCnt;

				player.atkedTick = g_logicFpsCnt;
				break;
			}
		}
	}

	// esc로 타이틀로 이동
	if (GetAsyncKeyState(VK_ESCAPE) != 0) {
		changeGameState(TITLE);
		return;
	}


	// 2. 로직부	
	// 적의 이동 & 공격
	for (size_t i = 0; i < kMaxEnemies; i++)
	{
		if (enemies[i].isVisible) {
			if (enemies[i].movedTick + enemies[i].moveSpeed < g_logicFpsCnt) {

				int type = enemies[i].type;
				char movePattern[kMaxPatternCnt] = "";
				csvGetValueInTable(g_enemyInfoPath, type, "move_pattern", movePattern, sizeof(movePattern));
				
				// 초기값을 현재 좌표로
				int nx = enemies[i].x;
				int ny = enemies[i].y;
				switch (movePattern[enemies[i].currMovePattern]) {
				case 'w': ny -= 1; break;
				case 'a': nx -= 1; break;
				case 'x': ny += 1; break;
				case 'd': nx += 1; break;
				case 'q': nx -= 1; ny -= 1; break;
				case 'e': nx += 1; ny -= 1; break;
				case 'z': nx -= 1; ny += 1; break;
				case 'c': nx += 1; ny += 1; break;
				case 's': /* stay */ break;
				default:
					printf("\n%s\n", movePattern);
					perror("move pattrean error");
					break;
				}

				// 이동 가능 범위로 가지 않는다면, 죽음 처리
				if (0 < ny && ny < dfSCREEN_HEIGHT && 0 < nx && nx < dfSCREEN_WIDTH) {
					enemies[i].x = nx;
					enemies[i].y = ny;
				}
				else {
					enemies[i].isVisible = false;
				}

				++enemies[i].currMovePattern;
				if (movePattern[enemies[i].currMovePattern] == '\0') {
					enemies[i].currMovePattern = 0;
				}

				enemies[i].movedTick = g_logicFpsCnt;

				// 적이 움직였으므로 플레이어와 충돌 로직 확인
				if (player.isVisible) {
					if (enemies[i].x == player.x && enemies[i].y == player.y && player.damagedTick + kPlayerInvincibleFrames <= g_logicFpsCnt) {
						player.hp -= kEnemyDamage;
						player.damagedTick = g_logicFpsCnt;
						if (player.hp <= 0) {
							player.isVisible = false;
							changeGameState(GAME_OVER);
						}
					}
				}
			}

			if (enemies[i].atkedTick + enemies[i].atkSpeed < g_logicFpsCnt) {
				for (size_t j = 0; j < kMaxBullets; j++)
				{
					if (bullets[j].isVisible == false) {
						bullets[j].isVisible = true;
						bullets[j].x = enemies[i].x;
						bullets[j].y = enemies[i].y + 1;
						bullets[j].isEnemy = true;
						bullets[j].movedTick = g_logicFpsCnt;

						enemies[i].atkedTick = g_logicFpsCnt;
						break;
					}
				}
			}
		}
	}
	// 총알 충돌 확인
	for (size_t i = 0; i < kMaxBullets; i++)
	{
		if (bullets[i].isVisible) {
			for (size_t j = 0; j < kMaxEnemies; j++)
			{
				if (enemies[j].isVisible) {
					if (bullets[i].isEnemy == false && bullets[i].x == enemies[j].x && bullets[i].y == enemies[j].y) {
						enemies[j].hp -= kBulletDamage;
						if (enemies[j].hp <= 0) {
							player.isVisible = false;
						}
						bullets[i].isVisible = false;
					}
				}
			}
		}
	}

	for (size_t i = 0; i < kMaxBullets; i++)
	{
		if (bullets[i].isVisible) {
			//  총알 전진
			if (bullets[i].isEnemy) {
				if (bullets[i].movedTick + bullets[i].speed < g_logicFpsCnt) {
					if (bullets[i].y + 1 < dfSCREEN_HEIGHT) {
						bullets[i].y += 1;
					}
					else {
						bullets[i].isVisible = false;
					}
					// 총알이 멈추는 경우는 없어 무조건 갱신
					bullets[i].movedTick = g_logicFpsCnt;
				}
			}
			else {
				if (bullets[i].movedTick + bullets[i].speed < g_logicFpsCnt) {
					if (bullets[i].y - 1 >= 0) {
						bullets[i].y -= 1;
					}
					else {
						bullets[i].isVisible = false;
					}

					// 총알이 멈추는 경우는 없어 무조건 갱신
					bullets[i].movedTick = g_logicFpsCnt;
				}
			}

			// 적과 총알 충돌 판정
			for (size_t j = 0; j < kMaxEnemies; j++)
			{
				if (enemies[j].isVisible) {
					if (bullets[i].isEnemy == false && bullets[i].x == enemies[j].x && bullets[i].y == enemies[j].y) {
						enemies[j].hp -= kBulletDamage;
						if (enemies[j].hp <= 0) {
							enemies[j].isVisible = false;
						}
						bullets[i].isVisible = false;
					}
				}
			}

			// 플레이어와 총알 충돌 판정
			if (player.isVisible) {
				if (bullets[i].isEnemy && bullets[i].x == player.x && bullets[i].y == player.y && player.damagedTick + kPlayerInvincibleFrames <= g_logicFpsCnt) {
					player.hp -= kBulletDamage;
					player.damagedTick = g_logicFpsCnt;
					if (player.hp <= 0) {
						player.isVisible = false;
						changeGameState(GAME_OVER);
					}
					bullets[i].isVisible = false;
				}
			}
		}
	}
	++g_logicFpsCnt;



	// 3. 랜더부
	// 한 프레임 이상 지연된 경우, 프레임 스킵
	const double secsSinceLastFrame = static_cast<double>(logicBeginTicks.QuadPart - g_frameStartTime.QuadPart) / static_cast<double>(g_freq.QuadPart);
	if (kFrameTimeSec < secsSinceLastFrame) {
		return;
	}
	// 무적 시간에 따른 플레이어 모양 변경
	if (player.damagedTick != 0 && g_logicFpsCnt - 1 <= player.damagedTick + kPlayerInvincibleFrames) {
		Sprite_Draw(player.x, player.y, kPlayerInvincibleShape);
	}
	else {
		Sprite_Draw(player.x, player.y, kPlayerShape);
	}

	for (size_t i = 0; i < kMaxEnemies; i++)
	{
		if (enemies[i].isVisible) {
			Sprite_Draw(enemies[i].x, enemies[i].y, enemies[i].shape);
		}
	}
	for (size_t i = 0; i < kMaxBullets; i++)
	{
		if (bullets[i].isVisible) {
			Sprite_Draw(bullets[i].x, bullets[i].y, kBulletShape);
		}
	}
	++g_renderFpsCnt;


	// 클리어 확인
	bool isClear = true;
	for (size_t i = 0; i < kMaxEnemies; i++)
	{
		if (enemies[i].isVisible) {
			isClear = false;
		}
	}
	if (isClear) {
		if (g_stage != g_maxStage) {
			changeGameState(LOADING);
		}
		else {
			changeGameState(GAME_CLEAR);
		}
		return;
	}


	// HUD 문자열 생성
	if (player.isVisible) {
		// 1번째 줄 : STAGE 왼쪽 + move 오른쪽
		{
			char left[64];
			char right[64];
			snprintf(left, sizeof(left), "STAGE : %d", g_stage);
			snprintf(right, sizeof(right), "move : arrows");

			int leftLen = (int)strlen(left);
			int rightLen = (int)strlen(right);
			int spaces = dfSCREEN_WIDTH - leftLen - rightLen - 1; // -1은 '\0' 고려

			cs_MoveCursor(0, dfSCREEN_HEIGHT);
			printf("%s%*s%s\n", left, spaces, "", right);
		}

		// 2번째 줄 : HP 왼쪽 + attack 오른쪽
		{
			char left[64];
			char right[64];
			snprintf(left, sizeof(left), "HP : %d", player.hp);
			snprintf(right, sizeof(right), "attack : Z");

			int leftLen = (int)strlen(left);
			int rightLen = (int)strlen(right);
			int spaces = dfSCREEN_WIDTH - leftLen - rightLen - 1;

			cs_MoveCursor(0, dfSCREEN_HEIGHT + 1);
			printf("%s%*s%s", left, spaces, "", right);
		}
	}
}

void gameOverScene()
{
	LARGE_INTEGER logicBeginTicks;
	QueryPerformanceCounter(&logicBeginTicks);
	if (g_isSceneBegin) {
		g_sceneBeginTime = logicBeginTicks;
		g_isSceneBegin = false;
	}

	// 1. 키보드 입력부
	if (GetAsyncKeyState(VK_ESCAPE) != 0x00) {
		g_stage = 0;
		changeGameState(TITLE);
	}
	// 2. 로직부
	++g_logicFpsCnt;

	// 한 프레임 이상 지연된 경우, 프레임 스킵
	const double secsSinceLastFrame = static_cast<double>(logicBeginTicks.QuadPart - g_frameStartTime.QuadPart) / static_cast<double>(g_freq.QuadPart);
	if (kFrameTimeSec < secsSinceLastFrame) {
		g_frameStartTime.QuadPart += static_cast<LONGLONG>(kFrameTimeSec * g_freq.QuadPart);
		return;
	}

	// 3. 랜더부
	char gameOverText[] = "GAME OVER";
	char exitGuideText[] = "Press ESC to quit the game.";

	// 문자열 길이 (null 제외)
	int gameOverLength = sizeof(gameOverText) - 1;
	int exitGuideLength = sizeof(exitGuideText) - 1;

	// 좌표 계산
	int gameOverStartX = (dfSCREEN_WIDTH - gameOverLength) / 2;
	int gameOverStartY = (dfSCREEN_HEIGHT / 2) - 1;
	int exitGuideStartX = (dfSCREEN_WIDTH - exitGuideLength) / 2;
	int exitGuideStartY = (dfSCREEN_HEIGHT / 2) + 1;

	// "GAME OVER" 출력
	for (int i = 0; i < gameOverLength; i++) {
		Sprite_Draw(gameOverStartX + i, gameOverStartY, gameOverText[i]);
	}
	for (int i = 0; i < exitGuideLength; i++) {
		Sprite_Draw(exitGuideStartX + i, exitGuideStartY, exitGuideText[i]);
	}
	++g_renderFpsCnt;
}

void gameClearScene()
{
	LARGE_INTEGER logicBeginTicks;
	QueryPerformanceCounter(&logicBeginTicks);
	if (g_isSceneBegin) {
		g_sceneBeginTime = logicBeginTicks;
		g_isSceneBegin = false;

	}

	// 1. 키보드 입력부
	if (GetAsyncKeyState(VK_ESCAPE) != 0x00) {
		g_stage = 0;
		changeGameState(TITLE);
		GetAsyncKeyState('A'); // A를 이전에 눌렀다면, esc 클릭 후 바로 게임이 실행됨으로 예외처리
	}

	// 2. 로직부
	++g_logicFpsCnt;

	// 한 프레임 이상 지연된 경우, 프레임 스킵
	const double secsSinceLastFrame = static_cast<double>(logicBeginTicks.QuadPart - g_frameStartTime.QuadPart) / static_cast<double>(g_freq.QuadPart);
	if (kFrameTimeSec < secsSinceLastFrame) {
		g_frameStartTime.QuadPart += static_cast<LONGLONG>(kFrameTimeSec * g_freq.QuadPart);
		return;
	}

	// 3. 랜더부
	char gameClearText[] = "GAME CLEAR!";
	char exitGuideText[] = "Press ESC to quit the game.";

	// 문자열 길이 (null 제외)
	int gameOverLength = strlen(gameClearText);
	int exitGuideLength = strlen(exitGuideText);

	// 좌표 계산
	int gameOverStartX = (dfSCREEN_WIDTH - gameOverLength) / 2;
	int gameOverStartY = (dfSCREEN_HEIGHT / 2) - 1;
	int exitGuideStartX = (dfSCREEN_WIDTH - exitGuideLength) / 2;
	int exitGuideStartY = (dfSCREEN_HEIGHT / 2) + 1;

	// "GAME OVER" 출력
	for (int i = 0; i < gameOverLength; i++) {
		Sprite_Draw(gameOverStartX + i, gameOverStartY, gameClearText[i]);
	}
	for (int i = 0; i < exitGuideLength; i++) {
		Sprite_Draw(exitGuideStartX + i, exitGuideStartY, exitGuideText[i]);
	}
}

void changeGameState(gameState nextState) {
	g_isSceneBegin = true;
	GAME_STATE = nextState;
	clearHUD();
}

void clearHUD()
{
	cs_MoveCursor(0, dfSCREEN_HEIGHT);
	for (int i = 0; i < dfSCREEN_WIDTH; i++) putchar(' ');
	cs_MoveCursor(0, dfSCREEN_HEIGHT + 1);
	for (int i = 0; i < dfSCREEN_WIDTH; i++) putchar(' ');
}

void testFps() { // main으로 대체해서 test
	LARGE_INTEGER windowStart; // FPS 집계 기준
	int logic_fps = 0, render_fps = 0;

	timeBeginPeriod(1);
	QueryPerformanceFrequency(&g_freq);
	QueryPerformanceCounter(&g_frameStartTime);
	QueryPerformanceCounter(&windowStart);

	LARGE_INTEGER start, end;


	while (true) {
		QueryPerformanceCounter(&start);

		double checkOneSec = static_cast<double>(g_frameStartTime.QuadPart - windowStart.QuadPart) / g_freq.QuadPart;
		if (1 < checkOneSec) {
			printf("logic FPS: %d, Render FPS: %d\n", g_logicFpsCnt - logic_fps, g_renderFpsCnt - render_fps);
			logic_fps = g_logicFpsCnt;
			render_fps = g_renderFpsCnt;
			QueryPerformanceCounter(&windowStart);
		}

		// 1. 로직
		for (volatile size_t i = 0; i < 10000; i++)
		{
			for (volatile size_t j = 0; j < 100; j++) {
				// busy waiting
			}
		}
		++g_logicFpsCnt;

		// 한 프레임 이상 지연된 경우, 프레임 스킵
		LARGE_INTEGER logicEndTicks;
		QueryPerformanceCounter(&logicEndTicks);
		const double secsSinceLastFrame = static_cast<double>(start.QuadPart - g_frameStartTime.QuadPart) / static_cast<double>(g_freq.QuadPart);
		if (kFrameTimeSec < secsSinceLastFrame) {
			g_frameStartTime.QuadPart += static_cast<LONGLONG>(kFrameTimeSec * g_freq.QuadPart);
			continue;
		}

		// 2. 렌더
		QueryPerformanceCounter(&end);
		double elapsed = static_cast<double>(end.QuadPart - g_frameStartTime.QuadPart) / g_freq.QuadPart;
		if (elapsed < kFrameTimeSec) {
			// 남은 시간(초 → 밀리초)
			DWORD sleepTime = static_cast<DWORD>((kFrameTimeSec - elapsed) * 1000.0);
			Sleep(sleepTime);
		}
		// 기준 시간 갱신 (틱 단위로)
		g_frameStartTime.QuadPart += static_cast<LONGLONG>(kFrameTimeSec * g_freq.QuadPart);
		++g_renderFpsCnt;
	}
}