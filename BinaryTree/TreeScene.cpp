#include "TreeScene.h"
#include "TreeSceneConfig.h"
#include "RBTree.h"
#include "TreeRenderer.h"

enum class CommandMode
{
	None = -1,
	Clear = 0,
	Insert = 1,
	InsertRandom = 2,
	Delete = 3,
};

static CommandMode g_mode = CommandMode::None;
static bool g_ignoreNextChar = false;

// 값 입력이 필요한 명령인지
static bool IsValueInputMode(CommandMode mode) noexcept
{
	return (mode == CommandMode::Insert || mode == CommandMode::Delete);
}

static wchar_t g_inputBuf[32] = L"";
static int g_inputLen = 0;

static void ClearInput() noexcept
{
	g_inputLen = 0;
	g_inputBuf[0] = L'\0';
}

static void BeginInput(CommandMode mode) noexcept
{
	g_mode = mode;
	ClearInput();
}

static bool TryParseInput(int& outValue) noexcept
{
	if (g_inputLen <= 0)
		return false;

	outValue = _wtoi(g_inputBuf);
	return true;
}

static RBTree::RBTree* g_tree = nullptr;

static void ResetTree()
{
	delete g_tree;
	g_tree = new RBTree::RBTree();
}

void TreeScene::OnCreate(HWND)
{
	ResetTree();
}

void TreeScene::OnDestroy()
{
	delete g_tree;
	g_tree = nullptr;
}

void TreeScene::OnKeyDown(HWND hwnd, WPARAM key)
{
	if (g_tree == nullptr)
		return;

	// 대문자/소문자 모두 허용
	switch (key)
	{
	case 'I':
	case 'i':   // INSERT
	{
		BeginInput(CommandMode::Insert);
		g_ignoreNextChar = true;   // 다음 WM_CHAR 1회 무시
		InvalidateRect(hwnd, nullptr, TRUE);
		return;
	}

	case 'D':
	case 'd':   // DELETE
	{
		BeginInput(CommandMode::Delete);
		g_ignoreNextChar = true;   // 다음 WM_CHAR 1회 무시
		InvalidateRect(hwnd, nullptr, TRUE);
		return;
	}

	case 'R':
	case 'r':   // RESET
	{
		delete g_tree;
		g_tree = new RBTree::RBTree(50);
		g_tree->Insert(30);
		g_tree->Insert(70);
		g_tree->Insert(20);
		g_tree->Insert(40);
		g_tree->Insert(60);
		g_tree->Insert(80);

		g_mode = CommandMode::Clear;
		ClearInput();
		InvalidateRect(hwnd, nullptr, TRUE);
		return;
	}

	case 'Q':
	case 'q':   // RANDOM INSERT
	{
		bool bInsert = false;
		while (!bInsert)
		{
			const int value = rand() % TreeSceneConfig::RandomMax;
			bInsert = g_tree->Insert(value);
		}

		g_mode = CommandMode::InsertRandom;
		ClearInput();
		InvalidateRect(hwnd, nullptr, TRUE);
		return;
	}
	}

	// -----------------------------
	// 값 입력 모드일 때의 제어 키 처리
	// -----------------------------
	if (IsValueInputMode(g_mode))
	{
		if (key == VK_ESCAPE)
		{
			g_mode = CommandMode::None;
			ClearInput();
			InvalidateRect(hwnd, nullptr, TRUE);
			return;
		}

		if (key == VK_BACK)
		{
			if (g_inputLen > 0)
			{
				--g_inputLen;
				g_inputBuf[g_inputLen] = L'\0';
				InvalidateRect(hwnd, nullptr, TRUE);
			}
			return;
		}

		if (key == VK_RETURN)
		{
			int value = 0;
			if (TryParseInput(value))
			{
				if (g_mode == CommandMode::Insert)
					g_tree->Insert(value);
				else if (g_mode == CommandMode::Delete)
					g_tree->Delete(value);
			}

			g_mode = CommandMode::None;
			ClearInput();
			InvalidateRect(hwnd, nullptr, TRUE);
			return;
		}
	}
}

void TreeScene::OnInputData(HWND hwnd, WPARAM key)
{
	if (g_tree == nullptr)
		return;

	if (!IsValueInputMode(g_mode))
		return;

	if (g_ignoreNextChar)
	{
		g_ignoreNextChar = false;
		return;
	}

	const wchar_t ch = static_cast<wchar_t>(key);
	if (ch >= L'0' && ch <= L'9')
	{
		if (g_inputLen < static_cast<int>(_countof(g_inputBuf)) - 1)
		{
			g_inputBuf[g_inputLen] = ch;
			++g_inputLen;
			g_inputBuf[g_inputLen] = L'\0';
			InvalidateRect(hwnd, nullptr, TRUE);
		}
		return;
	}
	return;
}

void TreeScene::OnPaint(HWND hwnd)
{
	PAINTSTRUCT ps;
	HDC hdc = BeginPaint(hwnd, &ps);

	RECT rc;
	GetClientRect(hwnd, &rc);
	FillRect(hdc, &rc, (HBRUSH)(COLOR_WINDOW + 1));

	TreeRenderer::DrawTree(hdc, g_tree, rc);

	// 하단 안내
	{
		constexpr const wchar_t* kMenuText =
			L"C:CLEAR\r\n"
			L"I:INSERT\r\n"
			L"Q:INSERT RANDOM\r\n"
			L"D:DELETE";

		RECT rcMenu{};
		rcMenu.left = 20;
		rcMenu.top = 520;
		rcMenu.right = 20 + 260;   // 메뉴 폭 (필요하면 늘려)
		rcMenu.bottom = 520 + 120; // 메뉴 높이 (줄 수에 맞게)

		// ✅ DrawTextW는 줄바꿈을 잘 처리함
		DrawTextW(hdc, kMenuText, -1, &rcMenu, DT_LEFT | DT_TOP | DT_NOPREFIX);

		if (IsValueInputMode(g_mode))
		{
			wchar_t line[128];
			const wchar_t* prefix = (g_mode == CommandMode::Insert) ? L"INSERT value: " : L"DELETE value: ";
			wsprintfW(line, L"%s%s_", prefix, g_inputBuf);
			TextOutW(hdc, 20, 520 + 100, line, lstrlenW(line));
		}
	}


	EndPaint(hwnd, &ps);
}
