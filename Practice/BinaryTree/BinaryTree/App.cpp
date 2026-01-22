#include "App.h"
#include "AppConfig.h"
#include "TreeScene.h"

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
	case WM_CREATE:
		TreeScene::OnCreate(hwnd);
		return 0;
	case WM_DESTROY:
		TreeScene::OnDestroy();
		PostQuitMessage(0);
		return 0;
	case WM_KEYDOWN:
		TreeScene::OnKeyDown(hwnd, wParam);
		return 0;
	case WM_CHAR:
		TreeScene::OnInputData(hwnd, wParam);
		return 0;
	case WM_PAINT:
		TreeScene::OnPaint(hwnd);
		return 0;
	}

	return DefWindowProc(hwnd, msg, wParam, lParam);
}

int App::Run(HINSTANCE hInst, int nCmdShow)
{
	const wchar_t* CLASS_NAME = L"MyWindowClass";

	WNDCLASS wc{};
	wc.lpfnWndProc = WndProc;
	wc.hInstance = hInst;
	wc.lpszClassName = CLASS_NAME;

	RegisterClass(&wc);

	HWND hwnd = CreateWindowEx(
		0, 
		AppConfig::WIndowClassName, 
		AppConfig::WindowTitle,
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT, 
		AppConfig::WindowWidth, 
		AppConfig::WindowHeight,
		nullptr, nullptr, hInst, nullptr);

	ShowWindow(hwnd, nCmdShow);

	MSG msg;
	while (GetMessage(&msg, nullptr, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	return 0;
}