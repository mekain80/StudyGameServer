#pragma once
#include <windows.h>
#include "BinaryTree.h"

namespace TreeRenderer
{
    void DrawTree(HDC hdc, const BinaryTree::Node* root, const RECT& clientRc);
}
