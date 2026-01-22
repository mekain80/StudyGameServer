#include "Astar.h"
#include "framework.h"
#include "resource.h"
#include <windowsx.h>
#include "Tile.h"

#define MAX_LOADSTRING 100

// ============================================================
// Global Variables (Win32 템플릿 기본 전역)
// ============================================================
HINSTANCE hInst;                                // 현재 인스턴스 핸들(프로그램 실행 단위)
WCHAR szTitle[MAX_LOADSTRING];                  // 타이틀바 텍스트(리소스 문자열에서 로드)
WCHAR szWindowClass[MAX_LOADSTRING];            // 윈도우 클래스 이름(리소스 문자열에서 로드)

// ============================================================
// A* 에디터(맵 편집) 상태
// ============================================================

// 시작점/도착점은 최초 1회는 "그냥 찍고", 2번째부터는 "기존 위치 지우고 새 위치 갱신"을 해야 한다.
// 그래서 최초인지 여부를 플래그로 관리한다.
bool gFirstStart = true;
bool gFirstEnd = true;

// ============================================================
// GDI 리소스 (생성: WM_CREATE / 해제: WM_DESTROY)
// ============================================================

HPEN   gGridPen;      // 그리드 선을 그릴 펜
HBRUSH gTileBrush;    // 장애물 타일을 채울 브러시

// 더블 버퍼링(깜빡임 방지)용 메모리 DC
// - 화면에 바로 그리면 WM_PAINT마다 깜빡임이 생길 수 있어서
//   메모리 DC에 먼저 렌더링한 뒤 BitBlt로 한 번에 옮긴다.
HBITMAP gMemDCBitmap;
HBITMAP gMemDCBitmapOld;
HDC     gMemDC;
RECT    gMemDCRect;

// ============================================================
// 장애물 입력/제거 드래그 모드 플래그
// ============================================================
//
// 더블 클릭으로 "장애물 토글"만 하면, 드래그로 연속 입력/제거가 불편해진다.
// 그래서:
// - 첫 클릭한 타일이 장애물이라면 => 지우기 모드(gErase=true)
// - 첫 클릭한 타일이 빈칸이라면   => 입력 모드(gErase=false)
// 로 모드를 결정하고, 마우스 이동하면서 연속 적용한다.
//
bool gErase = false;  // true: 지우기 모드 / false: 입력 모드
bool gDrag = false;  // 마우스 드래그 중인지

// ============================================================
// 현재 편집 모드 및 A* 객체
// ============================================================
Mode  gMode = Mode::OBSTACLE;  // 우클릭으로 OBSTACLE -> START -> END 순환
Astar gAstar;

// ============================================================
// 라우팅 실행 여부
// - RoutingStart()가 돌고 있는 동안에는 맵 Clear를 막는다.
// ============================================================
static bool gIsRouting = false;

static void ClearMap(HWND hWnd)
{
	ZeroMemory(gTile, sizeof(gTile));
	ZeroMemory(gTileInfo, sizeof(gTileInfo));

	gFirstStart = true;
	gFirstEnd = true;

	InvalidateRect(hWnd, NULL, FALSE);
}

// ============================================================
// RenderGrid()
// - 그리드 선(격자)을 그려서 타일 경계를 눈에 보이게 한다.
// ============================================================
void RenderGrid(HDC hdc)
{
	int X = 0;
	int Y = 0;

	// 펜을 그리드 펜으로 교체하고, 끝나면 원복한다.
	HPEN OldPen = (HPEN)SelectObject(hdc, gGridPen);

	// 마지막 경계선까지 그리기 위해 <= 사용
	for (int i = 0; i <= GRID_WIDTH; i++)
	{
		MoveToEx(hdc, X, 0, NULL);
		LineTo(hdc, X, GRID_HEIGHT * GRID_SIZE);
		X += GRID_SIZE;
	}

	for (int i = 0; i <= GRID_HEIGHT; i++)
	{
		MoveToEx(hdc, 0, Y, NULL);
		LineTo(hdc, GRID_WIDTH * GRID_SIZE, Y);
		Y += GRID_SIZE;
	}

	SelectObject(hdc, OldPen);
}

static void FillTile(HDC hdc, int x, int y, HBRUSH brush)
{
	RECT rc;
	rc.left = x;
	rc.top = y;
	rc.right = x + GRID_SIZE;
	rc.bottom = y + GRID_SIZE;
	FillRect(hdc, &rc, brush);
}

// ============================================================
// RenderObstacle()
// - 장애물 타일만 찾아 회색 블록으로 채워 그린다.
// - 사각형 외곽선(펜)을 없애기 위해 NULL_PEN 사용.
// ============================================================
void RenderObstacle(HDC hdc)
{
	int X = 0;
	int Y = 0;

	// 장애물 브러시를 선택하고, 끝나면 원복한다.
	HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, gTileBrush);

	// 사각형의 테두리선을 없애기 위해 NULL_PEN 지정
	// GetStockObject(NULL_PEN)은 "시스템이 제공하는 고정 GDI 오브젝트"라서 DeleteObject가 필요 없다.
	HPEN oldPen = (HPEN)SelectObject(hdc, GetStockObject(NULL_PEN));

	for (int i = 0; i < GRID_WIDTH; i++)
	{
		for (int j = 0; j < GRID_HEIGHT; j++)
		{
			if (gTile[j][i] == (int)Mode::OBSTACLE)
			{
				X = i * GRID_SIZE;
				Y = j * GRID_SIZE;
				FillTile(hdc, X, Y, gTileBrush);
			}
		}
	}

	SelectObject(hdc, oldBrush);
	SelectObject(hdc, oldPen);
}

// ============================================================
// Win32 기본 함수 선언 (템플릿)
// ============================================================
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);

// ============================================================
// wWinMain()
// - 프로그램 진입점(Unicode 버전)
// - 메시지 루프를 돌며 Windows 메시지를 WndProc로 전달한다.
// ============================================================
int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPWSTR    lpCmdLine,
	_In_ int       nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

	// 리소스 문자열 로드(앱 타이틀, 윈도우 클래스 이름)
	LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
	LoadStringW(hInstance, IDC_ASTAR, szWindowClass, MAX_LOADSTRING);

	// 윈도우 클래스 등록
	MyRegisterClass(hInstance);

	// 메인 윈도우 생성/표시
	if (!InitInstance(hInstance, nCmdShow))
		return FALSE;

	// 단축키(Accelerator) 로드
	HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_ASTAR));

	MSG msg;

	// 메시지 루프: 입력/윈도우 이벤트를 받아서 WndProc로 전달
	while (GetMessage(&msg, nullptr, 0, 0))
	{
		// 단축키 처리 우선(해당되면 TranslateMessage/DispatchMessage 생략)
		if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	return (int)msg.wParam;
}

// ============================================================
// MyRegisterClass()
// - 창 클래스(WNDCLASSEX) 등록
// - 아이콘/메뉴/커서/배경색 등을 세팅한다.
// ============================================================
ATOM MyRegisterClass(HINSTANCE hInstance)
{
	WNDCLASSEXW wcex;

	wcex.cbSize = sizeof(WNDCLASSEX);

	// 창 크기 변경 시 다시 그리기
	wcex.style = CS_HREDRAW | CS_VREDRAW;

	// 메시지 처리 함수
	wcex.lpfnWndProc = WndProc;

	// 추가 메모리 공간(사용 안 함)
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;

	wcex.hInstance = hInstance;

	// 리소스(.rc)에 등록된 아이콘 사용
	wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ASTAR));
	wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

	wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);

	// 기본 배경(흰색 계열)
	wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);

	// 리소스(.rc)에 등록된 메뉴 사용
	wcex.lpszMenuName = MAKEINTRESOURCEW(IDC_ASTAR);

	// 클래스 이름
	wcex.lpszClassName = szWindowClass;

	return RegisterClassExW(&wcex);
}

// ============================================================
// InitInstance()
// - 메인 윈도우 생성/표시
// ============================================================
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
	hInst = hInstance; // 인스턴스 핸들 저장

	HWND hWnd = CreateWindowW(
		szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, 0, CW_USEDEFAULT, 0,
		nullptr, nullptr, hInstance, nullptr);

	if (!hWnd)
		return FALSE;

	ShowWindow(hWnd, nCmdShow);
	UpdateWindow(hWnd);

	return TRUE;
}

// ============================================================
// WndProc()
// - 윈도우 메시지 처리
// - 입력(마우스/키보드), 렌더링(WM_PAINT), 리소스 생성/해제 등을 담당
// ============================================================
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	PAINTSTRUCT ps;
	HDC hdc;

	switch (message)
	{
	case WM_LBUTTONDOWN:
	{
		// 마우스 좌표(픽셀) -> 타일 좌표(칸) 변환
		int xPos = GET_X_LPARAM(lParam);
		int yPos = GET_Y_LPARAM(lParam);
		int TileX = xPos / GRID_SIZE;
		int TileY = yPos / GRID_SIZE;

		// 맵 밖 클릭 방지
		if (TileX >= GRID_WIDTH || TileY >= GRID_HEIGHT)
			break;

		// 현재 편집 모드에 따라 동작이 달라진다.
		switch (gMode)
		{
		case Mode::OBSTACLE:
			// 장애물 모드: 드래그 시작
			// - 첫 클릭이 장애물이면 "지우기 모드"
			// - 첫 클릭이 빈칸이면 "입력 모드"
			gDrag = true;
			if (gTile[TileY][TileX] == (int)Mode::OBSTACLE)
				gErase = true;
			else
				gErase = false;
			break;

		case Mode::START:
		{
			// 시작점 지정:
			// - 두 번째부터는 이전 시작점 타일을 비워준다.
			if (gFirstStart)
			{
				gFirstStart = false;
			}
			else
			{
				int OldTileX = gAstar.mStart->mX;
				int OldTileY = gAstar.mStart->mY;
				gTile[OldTileY][OldTileX] = 0;
			}

			gAstar.mStart->mX = TileX;
			gAstar.mStart->mY = TileY;
			gTile[TileY][TileX] = (int)Mode::START;

			// 화면 갱신 요청(더블 버퍼링 중이므로 erase는 false로 깜빡임 최소화)
			InvalidateRect(hWnd, NULL, false);
		}
		break;

		case Mode::END:
		{
			// 도착점 지정:
			// - 두 번째부터는 이전 도착점 타일을 비워준다.
			if (gFirstEnd)
			{
				gFirstEnd = false;
			}
			else
			{
				int OldTileX = gAstar.mEnd->mX;
				int OldTileY = gAstar.mEnd->mY;
				gTile[OldTileY][OldTileX] = 0;
			}

			gAstar.mEnd->mX = TileX;
			gAstar.mEnd->mY = TileY;
			gTile[TileY][TileX] = (int)Mode::END;

			InvalidateRect(hWnd, NULL, false);
		}
		break;

		default:
			break;
		}
	}
	break;

	case WM_LBUTTONUP:
		// 장애물 모드에서만 드래그 종료
		if (gMode == Mode::OBSTACLE)
			gDrag = false;
		break;

	case WM_RBUTTONDOWN:
		// 우클릭으로 편집 모드 순환
		// OBSTACLE -> START -> END -> OBSTACLE ...
		switch (gMode)
		{
		case Mode::OBSTACLE: gMode = Mode::START; break;
		case Mode::START:    gMode = Mode::END;   break;
		case Mode::END:      gMode = Mode::OBSTACLE; break;
		default: break;
		}
		break;

	case WM_MOUSEMOVE:
	{
		// 장애물 모드 + 드래그 중이면, 마우스가 지나가는 타일에 연속 적용
		if (gMode == Mode::OBSTACLE && gDrag)
		{
			int xPos = GET_X_LPARAM(lParam);
			int yPos = GET_Y_LPARAM(lParam);
			int TileX = xPos / GRID_SIZE;
			int TileY = yPos / GRID_SIZE;

			if (TileX >= GRID_WIDTH || TileY >= GRID_HEIGHT)
				break;

			// gErase:
			// - true  => 장애물 지우기(0)
			// - false => 장애물 넣기(1)
			// 현재 코드는 !gErase를 그대로 넣는 방식(값의 의미가 0/1로 운영되는 전제)
			gTile[TileY][TileX] = !gErase;

			// 더블 버퍼링으로 전체를 다시 그리므로 erase=false로 깜빡임을 줄인다.
			InvalidateRect(hWnd, NULL, false);
		}
	}
	break;

	case WM_KEYDOWN:
		if (wParam == 0x52) // 'R'
		{
			if (!gIsRouting)
			{
				gIsRouting = true;
				gAstar.RoutingStart(hWnd);
				gIsRouting = false;
			}
		}
		break;

	case WM_CREATE:
	{
		// GDI 리소스 생성(프로그램 시작 시 한 번)
		gGridPen = CreatePen(PS_SOLID, 1, RGB(200, 200, 200));
		gTileBrush = CreateSolidBrush(RGB(100, 100, 100));

		// A* 상태 표시용 브러시들
		gStartBrush = CreateSolidBrush(RGB(0, 250, 0));
		gEndBrush = CreateSolidBrush(RGB(250, 0, 0));
		gOpenBrush = CreateSolidBrush(RGB(0, 200, 255));
		gCloseBrush = CreateSolidBrush(RGB(0, 120, 200));
		gRouteBrush = CreateSolidBrush(RGB(200, 200, 0));

		// 더블 버퍼링용 메모리 DC 생성(현재 클라이언트 영역 크기와 동일)
		HDC hdc = GetDC(hWnd);
		GetClientRect(hWnd, &gMemDCRect);

		gMemDCBitmap = CreateCompatibleBitmap(hdc, gMemDCRect.right, gMemDCRect.bottom);
		gMemDC = CreateCompatibleDC(hdc);

		ReleaseDC(hWnd, hdc);

		// 메모리 DC에 비트맵을 연결해 그림을 그릴 수 있는 상태로 만든다.
		gMemDCBitmapOld = (HBITMAP)SelectObject(gMemDC, gMemDCBitmap);
	}
	break;

	case WM_PAINT:
		// 1) 메모리 DC 클리어(흰 배경)
		PatBlt(gMemDC, 0, 0, gMemDCRect.right, gMemDCRect.bottom, WHITENESS);

		// 2) 메모리 DC에 모든 요소를 순서대로 렌더링
		RenderObstacle(gMemDC);
		RenderGrid(gMemDC);
		RenderStart(gMemDC);
		RenderEnd(gMemDC);
		RenderOpen(gMemDC);
		RenderClose(gMemDC);
		RenderRoute(gMemDC);
		RenderText(gMemDC);

		// 3) 메모리 DC -> 화면 DC로 한 번에 복사(깜빡임 방지)
		hdc = BeginPaint(hWnd, &ps);
		BitBlt(hdc, 0, 0, gMemDCRect.right, gMemDCRect.bottom, gMemDC, 0, 0, SRCCOPY);
		EndPaint(hWnd, &ps);
		break;

	case WM_SIZE:
	{
		// 윈도우 크기가 바뀌면 더블 버퍼링 비트맵/메모리DC도 새 크기로 재생성해야 한다.
		SelectObject(gMemDC, gMemDCBitmapOld);
		DeleteDC(gMemDC);
		DeleteObject(gMemDCBitmap);

		HDC hdc = GetDC(hWnd);

		GetClientRect(hWnd, &gMemDCRect);
		gMemDCBitmap = CreateCompatibleBitmap(hdc, gMemDCRect.right, gMemDCRect.bottom);
		gMemDC = CreateCompatibleDC(hdc);

		ReleaseDC(hWnd, hdc);

		gMemDCBitmapOld = (HBITMAP)SelectObject(gMemDC, gMemDCBitmap);
	}
	break;

	case WM_COMMAND:
	{
		// 메뉴 선택 처리
		int wmId = LOWORD(wParam);
		switch (wmId)
		{
		case IDM_ABOUT:
			DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
			break;

		case IDM_EXIT:
			DestroyWindow(hWnd);
			break;

		case IDM_CLEAR:
			// 라우팅 중에는 상태가 꼬일 수 있으므로 Clear를 무시한다.
			if (!gIsRouting)
				ClearMap(hWnd);
			break;

		default:
			return DefWindowProc(hWnd, message, wParam, lParam);
		}
	}
	break;

	case WM_DESTROY:
		// 더블 버퍼링 리소스 해제
		SelectObject(gMemDC, gMemDCBitmapOld);

		DeleteDC(gMemDC);
		DeleteObject(gMemDCBitmap);

		DeleteObject(gGridPen);
		DeleteObject(gTileBrush);
		DeleteObject(gStartBrush);
		DeleteObject(gEndBrush);
		DeleteObject(gOpenBrush);
		DeleteObject(gCloseBrush);
		DeleteObject(gRouteBrush);

		PostQuitMessage(0);
		break;

	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}

	return 0;
}

// ============================================================
// About()
// - About 대화상자 메시지 처리(템플릿)
// ============================================================
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(lParam);

	switch (message)
	{
	case WM_INITDIALOG:
		return (INT_PTR)TRUE;

	case WM_COMMAND:
		// OK / Cancel을 누르면 대화상자를 닫는다.
		if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
		{
			EndDialog(hDlg, LOWORD(wParam));
			return (INT_PTR)TRUE;
		}
		break;
	}

	return (INT_PTR)FALSE;
}
