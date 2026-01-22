#pragma once
#include <Windows.h>

namespace TreeScene
{
	void OnCreate(HWND hwnd);
	void OnDestroy();
	void OnKeyDown(HWND hwnd, WPARAM key);
	void OnInputData(HWND hwnd, WPARAM key);
	void OnPaint(HWND hwnd);
}