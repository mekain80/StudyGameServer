#include <stdio.h>
#include <memory.h>
#include <Windows.h>
#include "Console.h"
#include <stdlib.h> 
#pragma comment(lib, "winmm.lib")

enum gameState { TITLE, PLAY, GAMEOVER };
gameState GAME_STATE;

const int MAX_ENEMY = 1000;
const int MAX_BULLET = 100;
const int BULLET_DEMAGE = 1;
const int PLAYER_MAX_HP = 5;
const int PLAYER_START_POS_X = dfSCREEN_WIDTH / 2;
const int PLAYER_START_POS_Y = dfSCREEN_HEIGHT - 1;
const int FPS = 60;
const double FRAME_TIME = 1000 / FPS; // 1프레임 = 약 16ms
const double FRAME_TIME_SEC = 1.0 / FPS;


int STAGE = 1;
bool isStageBegin = true;


struct Player
{
	int hp;
	int x, y;
	bool isVisible = false;
};

struct Enemy
{
	int hp;
	int x, y;
	bool isVisible = false;
};

struct Bullet
{
	int x, y;
	bool isEnemy;
	bool isVisible = false;
};

// 변수
Player player;
Enemy enemies[MAX_ENEMY];
Bullet bullets[MAX_BULLET];

void titleScene();
void playScene();
void gameoverScene();

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
	LARGE_INTEGER freq;
	QueryPerformanceFrequency(&freq);

	LARGE_INTEGER start, end;
	QueryPerformanceCounter(&start);

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
			// 1. 게임 타이틀
		case TITLE:
			titleScene();
			break;

		case PLAY:
			playScene();
			break;
		case GAMEOVER:
			gameoverScene();
			break;
		}


		// 하단은 게임씬의 로직 예시이며 
		// 이 부분에는 씬 표현을 위한 분기가 들어가시면 됩니다.
		// 
		// 
		// GameUpdate() 내부 예시
		// 
		// 1. 키보드 입력부
		// 2. 로직부 
		// 3. 랜더부
			/*  예시
				// 스크린 버퍼를 지움
				Buffer_Clear();
				// 스크린 버퍼에 객체들 출력
				Sprite_Draw(iX, 10, 'A');
				// 스크린 버퍼를 화면으로 출력
				Buffer_Flip();
			*/

			// 프레임 맞추기용 대기 Sleep(X)
		QueryPerformanceCounter(&end);
		double elapsed = static_cast<double>(end.QuadPart - start.QuadPart) / freq.QuadPart;

		if (elapsed < FRAME_TIME_SEC) {
			// 남은 시간(초 → 밀리초)
			DWORD sleepTime = static_cast<DWORD>((FRAME_TIME_SEC - elapsed) * 1000.0);
			Sleep(sleepTime);
		}
		// 기준 시간 갱신 (틱 단위로)
		start.QuadPart += static_cast<LONGLONG>(FRAME_TIME_SEC * freq.QuadPart);

		Buffer_Flip();
	}
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
	// 1. 키보드 입력부
	if (GetAsyncKeyState('A') != 0x00) {
		GAME_STATE = PLAY;
	}
	// 2. 로직부 

	// 3. 랜더부
	char startStr[] = "게임을 시작하려면 A키를 눌러주세요.";
	for (size_t i = 0; i < strlen(startStr); i++) {
		Sprite_Draw(i, 0, startStr[i]);
	}
}

void playScene() {
	// 스테이지 시작 판정
	if (isStageBegin == true) {
		player.hp = PLAYER_MAX_HP;
		player.isVisible = true;
		player.x = PLAYER_START_POS_X;
		player.y = PLAYER_START_POS_Y;

		// csv 파일로 대체 필요
		for (size_t i = 0; i < 22; i++)
		{
			enemies[i].hp = 1;
			enemies[i].x = (i % 10) * 2;
			enemies[i].y = 2 * (i / 10) + 1;
			enemies[i].isVisible = true;
		}

		for (size_t i = 0; i < MAX_BULLET; i++)
		{
			bullets[i].isVisible = false;
		}

		isStageBegin = false;
	}


	// 1. 키보드 입력부
	SHORT upKey = GetAsyncKeyState(VK_UP);
	SHORT downKey = GetAsyncKeyState(VK_DOWN);
	SHORT rightKey = GetAsyncKeyState(VK_RIGHT);
	SHORT leftKey = GetAsyncKeyState(VK_LEFT);
	if (upKey != 0 && downKey != 0) {}
	else if (upKey != 0) {
		if (player.y - 1 >= 0) {
			player.y -= 1;
		}
	}
	else if (downKey != 0) {
		if (player.y + 1 < dfSCREEN_HEIGHT) {
			player.y += 1;
		}
	}
	if (rightKey != 0 && leftKey != 0) {}
	else if (rightKey != 0) {
		if (player.x + 1 < dfSCREEN_WIDTH - 1) {
			player.x += 1;
		}
	}
	else if (leftKey != 0) {
		if (player.x - 1 >= 0) {
			player.x -= 1;
		}
	}

	if (GetAsyncKeyState('Z') != 0) {
		for (size_t i = 0; i < MAX_BULLET; i++)
		{
			if (bullets[i].isVisible == false) {
				bullets[i].isVisible = true;
				bullets[i].x = player.x;
				bullets[i].y = player.y;
				bullets[i].isEnemy = false;
				break;
			}
		}
	}


	// 2. 로직부	
	for (size_t i = 0; i < MAX_BULLET; i++)
	{
		if (bullets[i].isVisible) {
			//  총알 전진
			if (bullets[i].isEnemy) {
				if (bullets[i].y + 1 < dfSCREEN_HEIGHT) {
					bullets[i].y += 1;
				}
				else {
					bullets[i].isVisible = false;
				}
			}
			else {
				if (bullets[i].y - 1 >= 0) {
					bullets[i].y -= 1;
				}
				else {
					bullets[i].isVisible = false;
				}
			}

			// 적과 총알 충돌 판정
			for (size_t j = 0; j < MAX_ENEMY; j++)
			{
				if (enemies[j].isVisible) {
					if (bullets[i].isEnemy == false && bullets[i].x == enemies[j].x && bullets[i].y == enemies[j].y) {
						enemies[j].hp -= BULLET_DEMAGE;
						if (enemies[j].hp <= 0) {
							enemies[j].isVisible = false;
						}
						bullets[i].isVisible = false;
					}
				}
			}

			// 플레이어와 총알 충돌 판정
			if (player.isVisible) {
				if (bullets[i].isEnemy && bullets[i].x == player.x && bullets[i].y == player.y) {
					player.hp -= BULLET_DEMAGE;
					if (player.hp <= 0) {
						player.isVisible = false;
						GAME_STATE = GAMEOVER;
					}
					bullets[i].isVisible = false;
				}
			}
		}
	}



	// 3. 랜더부
	Sprite_Draw(player.x, player.y, 'A');
	for (size_t i = 0; i < MAX_ENEMY; i++)
	{
		if (enemies[i].isVisible) {
			char buf[16] = { 0 };               // hp가 충분히 들어갈 버퍼
			_itoa_s(enemies[i].hp, buf, 10);  // MSVC 안전 함수 (10진수)
			Sprite_Draw(enemies[i].x, enemies[i].y, buf[0]);
		}
	}
	for (size_t i = 0; i < MAX_BULLET; i++)
	{
		if (bullets[i].isVisible) {
			Sprite_Draw(bullets[i].x, bullets[i].y, 'I');
		}
	}
}

void gameoverScene()
{
	isStageBegin = true;
	GAME_STATE = TITLE;
}

