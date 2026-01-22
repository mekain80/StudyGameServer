#pragma once
#include <Windows.h>

#define GRID_SIZE 16
#define GRID_WIDTH 100
#define GRID_HEIGHT 50

enum class Mode
{
	OBSTACLE = 1,
	START,
	END,
	OPENLIST,
	CLOSELIST,
	ROUTE,
};

struct Info {
	double mG;
	double mH;
};

extern char gTile[GRID_HEIGHT][GRID_WIDTH];
extern Info gTileInfo[GRID_HEIGHT][GRID_WIDTH];

extern HBRUSH gOpenBrush;
extern HBRUSH gCloseBrush;
extern HBRUSH gRouteBrush;
extern HBRUSH gStartBrush;
extern HBRUSH gEndBrush;

/// Open List 상태인 타일들을 gOpenBrush로 채워서 그림
void RenderOpen(HDC hdc);
// Close List 상태인 타일들을 gCloseBrush로 채워서 그림
void RenderClose(HDC hdc);
// Route(최종 경로) 상태인 타일들을 gRouteBrush로 채워서 그림
void RenderRoute(HDC hdc);
// Start(시작점) 상태인 타일을 gStartBrush로 채워서 그림
void RenderStart(HDC hdc);
// End(도착점) 상태인 타일을 gEndBrush로 채워서 그림
void RenderEnd(HDC hdc);

// 각 타일의 G/H(또는 필요한 정보)를 텍스트로 출력함
void RenderText(HDC hdc);   
