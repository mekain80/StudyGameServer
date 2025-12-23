#include "TreeRenderer.h"
#include "TreeRenderConfig.h"
#include <unordered_map>

struct NodePos
{
    int x;
    int y;
};

using PosMap = std::unordered_map<const BinaryTree::Node*, NodePos>;


/// x = startX + (중위순회에서의 방문 순서) * xGap
/// y = startY + (트리 깊이) * yGap
static void LayoutInOrder(
    const BinaryTree::Node* node,
    int depth,
    int& xIndex,
    PosMap& outPos,
    int startX,
    int startY,
    int xGap,
    int yGap)
{
    if (node == nullptr)
        return;

    LayoutInOrder(node->left, depth + 1, xIndex, outPos, startX, startY, xGap, yGap);

    ++xIndex;
    outPos[node] = { startX + xIndex * xGap, startY + depth * yGap };

    LayoutInOrder(node->right, depth + 1, xIndex, outPos, startX, startY, xGap, yGap);
}

static void DrawEdge(HDC hdc, const NodePos& parent, const NodePos& child, int radius)
{
    MoveToEx(hdc, parent.x, parent.y + radius, nullptr);
    LineTo(hdc, child.x, child.y - radius);
}

static void DrawEdgesRecursive(HDC hdc, const BinaryTree::Node* node, const PosMap& pos, int radius)
{
    if (node == nullptr)
        return;

    const NodePos me = pos.at(node);

    if (node->left != nullptr)
        DrawEdge(hdc, me, pos.at(node->left), radius);

    if (node->right != nullptr)
        DrawEdge(hdc, me, pos.at(node->right), radius);

    DrawEdgesRecursive(hdc, node->left, pos, radius);
    DrawEdgesRecursive(hdc, node->right, pos, radius);
}

static void DrawNode(HDC hdc, int x, int y, int radius, int key)
{
    Ellipse(hdc, x - radius, y - radius, x + radius, y + radius);

    wchar_t buf[32];
    wsprintfW(buf, L"%d", key);

    RECT rc;
    rc.left = x - radius;
    rc.top = y - radius;
    rc.right = x + radius;
    rc.bottom = y + radius;

    DrawTextW(hdc, buf, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

static void DrawNodesRecursive(HDC hdc, const BinaryTree::Node* node, const PosMap& pos, int radius)
{
    if (node == nullptr) return;

    const NodePos p = pos.at(node);
    DrawNode(hdc, p.x, p.y, radius, node->key);

    DrawNodesRecursive(hdc, node->left, pos, radius);
    DrawNodesRecursive(hdc, node->right, pos, radius);
}

void TreeRenderer::DrawTree(HDC hdc, const BinaryTree::Node* root, const RECT& clientRc)
{
    (void)clientRc;

    PosMap pos;
    int xIndex = 0;

    // (원래 값 유지) 필요하면 clientRc 기반으로 자동 조절 가능
    LayoutInOrder(root, 0, xIndex, pos,
        TreeRenderConfig::StartX,
        TreeRenderConfig::StartY,
        TreeRenderConfig::XGap,
        TreeRenderConfig::YGap);

    const int radius = TreeRenderConfig::NodeRadius;
    HPEN pen = CreatePen(
        PS_SOLID,
        TreeRenderConfig::EdgeWidth,
        RGB(0, 0, 0));
    HBRUSH brush = CreateSolidBrush(RGB(240, 240, 240));
    HGDIOBJ oldPen = SelectObject(hdc, pen);
    HGDIOBJ oldBrush = SelectObject(hdc, brush);

    DrawEdgesRecursive(hdc, root, pos, radius);
    DrawNodesRecursive(hdc, root, pos, radius);

    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(brush);
    DeleteObject(pen);
}
