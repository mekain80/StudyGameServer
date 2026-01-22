#pragma once
#include <windows.h>
#include "RBTree.h"

namespace TreeRenderer
{
    // RBTree 전체를 넘겨서 nil(센티넬)까지 같이 알 수 있게 처리
    void DrawTree(HDC hdc, const RBTree::RBTree* tree, const RECT& clientRc);
}
