#include <stdio.h>
#include <memory.h>
#include <Windows.h>
#include "Console.h"
#include <stdlib.h> 
#include "csvLoader.h" 
#pragma comment(lib, "winmm.lib")

enum gameState { TITLE, LOADING, PLAY, GAME_OVER, GAME_CLEAR, EXIT };
gameState g_gameState = TITLE;

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
const int kPlayerInvincibleFrames = 30; // 데미지 받고 몇 프레임 무적으로 할것인지
const char kPlayerInvincibleShape = 'B';
const int kDefaultBulletSpeed = 5;
const int kMinWaitTimeLoadingScene = 1;
const int kLoadingBarLength = 10;

size_t g_maxStage = 0;
int g_stage = 0;
int g_logicFpsCnt = 0;
int g_renderFpsCnt = 0;
bool g_isSceneBegin = true;
LARGE_INTEGER g_freq;
LARGE_INTEGER g_frameStartTick;
LARGE_INTEGER g_sceneFrameStartTick;
LARGE_INTEGER g_sceneBeginTick; // 이거 없애야겠는데?
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
bool isFrameSkip();
bool isKeyDown(SHORT);
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
	QueryPerformanceCounter(&g_frameStartTick);

	LARGE_INTEGER end;

	cs_Initial();

	//--------------------------------------------------------------------
	// 게임의 메인 루프
	// 이 루프가  1번 돌면 1프레임 이다.
	//--------------------------------------------------------------------
	while (1)
	{
		Buffer_Clear();
		switch (g_gameState)
		{
		case TITLE:      titleScene();      break;
		case LOADING:    loadingScene();    break;
		case PLAY:       playScene();       break;
		case GAME_OVER:  gameOverScene();   break;
		case GAME_CLEAR: gameClearScene();  break;
		case EXIT:
			timeEndPeriod(1);
			return;
		}

		QueryPerformanceCounter(&end);
		double elapsed = static_cast<double>(end.QuadPart - g_frameStartTick.QuadPart) / g_freq.QuadPart;

		if (elapsed < kFrameTimeSec) {
			DWORD sleepTime = static_cast<DWORD>((kFrameTimeSec - elapsed) * 1000.0);
			Sleep(sleepTime);
		}
		g_frameStartTick.QuadPart += static_cast<LONGLONG>(kFrameTimeSec * g_freq.QuadPart);

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
	QueryPerformanceCounter(&g_sceneFrameStartTick);

	// 1. 키보드 입력부
	if (isKeyDown('A')) {
		changeGameState(LOADING);
	}
	if (isKeyDown(VK_ESCAPE)) {
		changeGameState(EXIT);
	}

	// 2. 로직부 
	++g_logicFpsCnt;

	// 한 프레임 이상 지연된 경우, 렌더 스킵
	bool needFrameSkip = isFrameSkip();
	if (needFrameSkip) {
		return;
	}

	// 3. 랜더부
	// "Terminal Shooter Game"
	// "Press the A key to start the game."
	char titleText[] = "Terminal Shooter Game";
	char startGuideText[] = "Press the A key to start the game.";

	int titleLength = sizeof(titleText) - 1;
	int startGuideLength = sizeof(startGuideText) - 1;

	int titleStartX = (dfSCREEN_WIDTH - titleLength) / 2;
	int titleStartY = (dfSCREEN_HEIGHT / 2) - 2;
	int startGuideStartX = (dfSCREEN_WIDTH - startGuideLength) / 2;
	int startGuideStartY = (dfSCREEN_HEIGHT / 2) + 5;

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

	// 1. 키보드 입력부

	// 2. 로직부 
	char stage_name[256];
	if (g_isSceneBegin) {
		g_sceneBeginTick = logicBeginTicks;
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

	// 로딩 창 최소 시간 초과 & 데이터 로드 완료 시 PLAY SCENE으로 전환
	const double sceneElapedtime = static_cast<double>(logicBeginTicks.QuadPart - g_sceneBeginTick.QuadPart) / static_cast<double>(g_freq.QuadPart);
	if (kMinWaitTimeLoadingScene < sceneElapedtime && csvLoadAll(g_stageInfoPath) && csvLoadAll(g_enemyInfoPath)) {
		changeGameState(PLAY);
	}

	// 한 프레임 이상 지연된 경우, 렌더 스킵
	bool needFrameSkip = isFrameSkip();
	if (needFrameSkip) {
		return;
	}

	// 3. 랜더부
	// 로딩 메세지 출력
	char loadingText[64];
	if (g_maxStage == g_stage) {
		snprintf(loadingText, sizeof(loadingText), "LAST STAGE, NOW LOADING...");
	} else {
		snprintf(loadingText, sizeof(loadingText), "STAGE %d, NOW LOADING...", g_stage);
	}
	int loadingTextLength = strlen(loadingText);
	int loadingStartX = (dfSCREEN_WIDTH - loadingTextLength) / 2;
	int loadingStartY = (dfSCREEN_HEIGHT / 2);
	for (int i = 0; i < loadingTextLength; i++) {
		Sprite_Draw(loadingStartX + i, loadingStartY, loadingText[i]);
	}

	// 로딩 진척도 바 구현(최소 시간 기준)
	const int loadingBarStartX = (dfSCREEN_WIDTH - kLoadingBarLength) / 2;
	const int loadingBarStartY = (dfSCREEN_HEIGHT / 2) + 2;

	// 경과 시간 비율 계산
	float ratio = sceneElapedtime / kMinWaitTimeLoadingScene;
	if (ratio > 1.f) ratio = 1.f;
	if (ratio < 0.f) ratio = 0.f;
	int filled = static_cast<int>(ratio * kLoadingBarLength);

	// 출력
	for (int i = 0; i < kLoadingBarLength; i++) {
		Sprite_Draw(loadingBarStartX + i, loadingBarStartY, (i < filled) ? '#' : '.');
	}
}

void playScene() {
	// 스테이지 시작 처리
	if (g_isSceneBegin == true) {
		// 플레이어 초기화
		player.hp = kPlayerMaxHP;
		player.isVisible = true;
		player.x = kPlayerStartPosX;
		player.y = kPlayerStartPosY;
		player.damagedTick = 0;
		player.movedTick = 0;
		player.atkedTick = 0;

		// 적/총알 초기화
		for (size_t i = 0; i < kMaxEnemies; ++i) {
			enemies[i].isVisible = false;
			enemies[i].currMovePattern = 0;
			enemies[i].movedTick = 0;
			enemies[i].atkedTick = 0;
		}
		for (size_t i = 0; i < kMaxBullets; ++i) {
			bullets[i].isVisible = false;
			bullets[i].movedTick = 0;
		}

		// 스테이지 데이터 로드
		char stageName[256];
		csvGetValueInTable(g_stageInfoPath, g_stage, "stage_name", stageName, sizeof(stageName));
		csvLoadAll(stageName);

		size_t enemyIdArraySize = 0;
		int* enemyInfoArray = getCsvIdArray(stageName, enemyIdArraySize);
		for (size_t i = 0; i < enemyIdArraySize; i++)
		{
			int id = enemyInfoArray[i];
			char buf[256];
			csvGetValueInTable(stageName, id, "x", buf, sizeof(buf));
			enemies[i].x = atoi(buf);
			csvGetValueInTable(stageName, id, "y", buf, sizeof(buf));
			enemies[i].y = atoi(buf);
			csvGetValueInTable(stageName, id, "type", buf, sizeof(buf));
			enemies[i].type = atoi(buf);

			// 타입 별 스탯 주입
			int type = enemies[i].type;
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

		g_isSceneBegin = false;
	}


	// 1. 키보드 입력부
	const bool isUp = isKeyDown(VK_UP);
	const bool isDown = isKeyDown(VK_DOWN);
	const bool isRight = isKeyDown(VK_RIGHT);
	const bool isLeft = isKeyDown(VK_LEFT);

	// 플레이어 이동
	if (player.movedTick + kPlayerSpeedCellsPerFrame < g_logicFpsCnt) {
		if (isUp && isDown) {}
		else if (isUp && (player.y - 1 >= 0)) {
			player.y -= 1;
			player.movedTick = g_logicFpsCnt;
		}
		else if (isDown && (player.y + 1 < dfSCREEN_HEIGHT)) {
			player.y += 1;
			player.movedTick = g_logicFpsCnt;
		}

		if (isRight && isLeft) {}
		else if (isRight && (player.x + 1 < dfSCREEN_WIDTH - 1)) {
			player.x += 1;
			player.movedTick = g_logicFpsCnt;
		}
		else if (isLeft && (player.x - 1 >= 0)) {
			player.x -= 1;
			player.movedTick = g_logicFpsCnt;
		}
	}

	// 총알 발사
	if (isKeyDown('Z') and player.atkedTick + kPlayerAttackSpeed < g_logicFpsCnt) {
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
	}


	// 2. 로직부	
	// 적의 이동 & 공격
	for (size_t i = 0; i < kMaxEnemies; i++)
	{
		if (enemies[i].isVisible == false) continue;

		// 이동
		if (enemies[i].movedTick + enemies[i].moveSpeed < g_logicFpsCnt) {
			char movePattern[kMaxPatternCnt] = "";
			csvGetValueInTable(g_enemyInfoPath, enemies[i].type, "move_pattern", movePattern, sizeof(movePattern));

			// 초기값을 현재 좌표로
			int nx = enemies[i].x;
			int ny = enemies[i].y;
			switch (movePattern[enemies[i].currMovePattern]) {
			// s를 기준으로 8가지 방향 이동 가능
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

			if (0 < ny && ny < dfSCREEN_HEIGHT && 0 < nx && nx < dfSCREEN_WIDTH) {
				enemies[i].x = nx;
				enemies[i].y = ny;
			}
			// 화면 밖으로 나가면 제거
			else {
				enemies[i].isVisible = false; 
			}

			// 이동 패턴 순환
			if (movePattern[enemies[i].currMovePattern] != '\0')
				++enemies[i].currMovePattern;
			if (movePattern[enemies[i].currMovePattern] == '\0')
				enemies[i].currMovePattern = 0;

			enemies[i].movedTick = g_logicFpsCnt;

			// 이동 직후 플레이어와 충돌 로직 확인
			if (player.isVisible &&
				enemies[i].x == player.x && enemies[i].y == player.y &&
				player.damagedTick + kPlayerInvincibleFrames <= g_logicFpsCnt) {

				player.hp -= kEnemyDamage;
				player.damagedTick = g_logicFpsCnt;
				if (player.hp <= 0) {
					player.isVisible = false;
					changeGameState(GAME_OVER);
					return;
				}
			}
		}

		// 적군 공격
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

	// 총알 이동, 충돌 확인
	for (size_t bulletIdx = 0; bulletIdx < kMaxBullets; ++bulletIdx) {
		if (!bullets[bulletIdx].isVisible) continue;

		// 이동 쿨다운
		if (bullets[bulletIdx].movedTick + bullets[bulletIdx].speed < g_logicFpsCnt) {
			if (bullets[bulletIdx].isEnemy) {
				if (bullets[bulletIdx].y + 1 < dfSCREEN_HEIGHT) bullets[bulletIdx].y += 1;
				else bullets[bulletIdx].isVisible = false;
			}
			else {
				if (bullets[bulletIdx].y - 1 >= 0) bullets[bulletIdx].y -= 1;
				else bullets[bulletIdx].isVisible = false;
			}
			bullets[bulletIdx].movedTick = g_logicFpsCnt;
		}

		if (!bullets[bulletIdx].isVisible) continue;

		// 적과 충돌(플레이어 총알만)
		if (bullets[bulletIdx].isEnemy == false) {
			for (size_t enemyIdx = 0; enemyIdx < kMaxEnemies; ++enemyIdx) {
				if (enemies[enemyIdx].isVisible == false) continue;
				if (bullets[bulletIdx].x == enemies[enemyIdx].x && bullets[bulletIdx].y == enemies[enemyIdx].y) {
					enemies[enemyIdx].hp -= kBulletDamage;
					if (enemies[enemyIdx].hp <= 0) {
						enemies[enemyIdx].isVisible = false; // ← BUG 수정: player가 아니라 enemy 제거
					}
					bullets[bulletIdx].isVisible = false;
					break;
				}
			}
		}

		// 플레이어와 충돌(적 총알만)
		if (bullets[bulletIdx].isEnemy && player.isVisible) {
			if (bullets[bulletIdx].x == player.x && bullets[bulletIdx].y == player.y &&
				player.damagedTick + kPlayerInvincibleFrames <= g_logicFpsCnt) {

				player.hp -= kBulletDamage;
				player.damagedTick = g_logicFpsCnt;
				if (player.hp <= 0) {
					player.isVisible = false;
					changeGameState(GAME_OVER);
					return;
				}
				bullets[bulletIdx].isVisible = false;
			}
		}
	}
	++g_logicFpsCnt;

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
	}

	// 한 프레임 이상 지연된 경우, 렌더 스킵
	bool needFrameSkip = isFrameSkip();
	if (needFrameSkip) {
		return;
	}

	// 3. 랜더부
	// 무적 시간에 따른 플레이어 모양 변경
	if (player.isVisible) {
		const bool invincible = (player.damagedTick != 0) && ((g_logicFpsCnt - player.damagedTick) < kPlayerInvincibleFrames);
		if (invincible) Sprite_Draw(player.x, player.y, kPlayerInvincibleShape);
		else Sprite_Draw(player.x, player.y, kPlayerShape);
	}

	for (size_t i = 0; i < kMaxEnemies; i++)
	{
		if (enemies[i].isVisible)
			Sprite_Draw(enemies[i].x, enemies[i].y, enemies[i].shape);
	}
	for (size_t i = 0; i < kMaxBullets; i++)
	{
		if (bullets[i].isVisible)
			Sprite_Draw(bullets[i].x, bullets[i].y, kBulletShape);
	}

	// HUD 문자열 생성
	// STAGE : %d	 +		move : arrows
	// HP : %d		 +		attack : Z
	if (player.isVisible and (isClear == false)) {
		char left[64];
		char right[64];
		{
			snprintf(left, sizeof(left), "STAGE : %d", g_stage);
			snprintf(right, sizeof(right), "move : arrows");

			int leftLen = (int)strlen(left);
			int rightLen = (int)strlen(right);
			int spaces = dfSCREEN_WIDTH - leftLen - rightLen - 1;

			cs_MoveCursor(0, dfSCREEN_HEIGHT);
			printf("%s%*s%s\n", left, spaces, "", right);
		}

		// 2번째 줄 : HP 왼쪽 + attack 오른쪽
		{
			snprintf(left, sizeof(left), "HP : %d", player.hp);
			snprintf(right, sizeof(right), "attack : Z");

			int leftLen = (int)strlen(left);
			int rightLen = (int)strlen(right);
			int spaces = dfSCREEN_WIDTH - leftLen - rightLen - 1;

			cs_MoveCursor(0, dfSCREEN_HEIGHT + 1);
			printf("%s%*s%s", left, spaces, "", right);
		}
	}
	++g_renderFpsCnt;
}

void gameOverScene()
{
	// 1. 키보드 입력부
	if (GetAsyncKeyState(VK_ESCAPE) != 0x00) {
		g_stage = 0;
		changeGameState(TITLE);
	}
	// 2. 로직부
	++g_logicFpsCnt;

	// 한 프레임 이상 지연된 경우, 렌더 스킵
	bool needFrameSkip = isFrameSkip();
	if (needFrameSkip) {
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
	// 1. 키보드 입력부
	if (GetAsyncKeyState('Q') != 0x00) {
		g_stage = 0;
		changeGameState(TITLE);
		GetAsyncKeyState('A'); // A를 이전에 눌렀다면, esc 클릭 후 바로 게임이 실행됨으로 예외처리
	}

	// 2. 로직부
	++g_logicFpsCnt;

	// 한 프레임 이상 지연된 경우, 렌더 스킵
	bool needFrameSkip = isFrameSkip();
	if (needFrameSkip) {
		return;
	}

	// 3. 랜더부
	char gameClearText[] = "GAME CLEAR!";
	char exitGuideText[] = "Press Q to quit the game.";

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
	g_gameState = nextState;
	clearHUD();
}

bool isFrameSkip() {
	const long frameTicks = g_freq.QuadPart / kFps;
	const long behind = g_sceneFrameStartTick.QuadPart - g_frameStartTick.QuadPart;
	return (behind >= frameTicks);
}

bool isKeyDown(SHORT vKey)
{
	return (GetAsyncKeyState(vKey) != 0);
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
	QueryPerformanceCounter(&g_frameStartTick);
	QueryPerformanceCounter(&windowStart);

	LARGE_INTEGER start, end;


	while (true) {
		QueryPerformanceCounter(&start);

		double checkOneSec = static_cast<double>(g_frameStartTick.QuadPart - windowStart.QuadPart) / g_freq.QuadPart;
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
		const double secsSinceLastFrame = static_cast<double>(start.QuadPart - g_frameStartTick.QuadPart) / static_cast<double>(g_freq.QuadPart);
		if (kFrameTimeSec < secsSinceLastFrame) {
			g_frameStartTick.QuadPart += static_cast<LONGLONG>(kFrameTimeSec * g_freq.QuadPart);
			continue;
		}

		// 2. 렌더
		QueryPerformanceCounter(&end);
		double elapsed = static_cast<double>(end.QuadPart - g_frameStartTick.QuadPart) / g_freq.QuadPart;
		if (elapsed < kFrameTimeSec) {
			// 남은 시간(초 → 밀리초)
			DWORD sleepTime = static_cast<DWORD>((kFrameTimeSec - elapsed) * 1000.0);
			Sleep(sleepTime);
		}
		// 기준 시간 갱신 (틱 단위로)
		g_frameStartTick.QuadPart += static_cast<LONGLONG>(kFrameTimeSec * g_freq.QuadPart);
		++g_renderFpsCnt;
	}
}