#include "TreeRenderer.h"
#include "TreeRenderConfig.h"

#include <unordered_map>
#include <string>

struct NodePos
{
    int x;
    int y;
};

using PosMap = std::unordered_map<const RBTree::Node*, NodePos>;

// 중위 순회(in-order)로 x 좌표를 배치한다.
// - nil(센티넬) 노드는 그리지 않는다.
// - y = startY + (트리 깊이) * yGap
static void LayoutInOrder(
    const RBTree::Node* node,
    const RBTree::Node* nil,
    int depth,
    int& xIndex,
    PosMap& outPos,
    int startX,
    int startY,
    int xGap,
    int yGap)
{
    if (node == nil)
        return;

    LayoutInOrder(node->getLeft(), nil, depth + 1, xIndex, outPos, startX, startY, xGap, yGap);

    ++xIndex;
    outPos[node] = { startX + xIndex * xGap, startY + depth * yGap };

    LayoutInOrder(node->getRight(), nil, depth + 1, xIndex, outPos, startX, startY, xGap, yGap);
}

static void DrawEdge(HDC hdc, const NodePos& parent, const NodePos& child, int radius)
{
    MoveToEx(hdc, parent.x, parent.y + radius, nullptr);
    LineTo(hdc, child.x, child.y - radius);
}

static void DrawEdgesRecursive(HDC hdc, const RBTree::Node* node, const RBTree::Node* nil, const PosMap& pos, int radius)
{
    if (node == nil)
        return;

    const NodePos me = pos.at(node);

    const RBTree::Node* left = node->getLeft();
    const RBTree::Node* right = node->getRight();

    if (left != nil)
        DrawEdge(hdc, me, pos.at(left), radius);

    if (right != nil)
        DrawEdge(hdc, me, pos.at(right), radius);

    DrawEdgesRecursive(hdc, left, nil, pos, radius);
    DrawEdgesRecursive(hdc, right, nil, pos, radius);
}

static void DrawNode(HDC hdc, int x, int y, int radius, int key)
{
    Ellipse(hdc, x - radius, y - radius, x + radius, y + radius);

    wchar_t buf[32];
    wsprintfW(buf, L"%d", key);

    const int len = lstrlenW(buf);
    SIZE textSize{};
    GetTextExtentPoint32W(hdc, buf, len, &textSize);

    const int tx = x - (textSize.cx / 2);
    const int ty = y - (textSize.cy / 2);
    TextOutW(hdc, tx, ty, buf, len);
}

static void DrawNodesRecursive(HDC hdc, const RBTree::Node* node, const RBTree::Node* nil, const PosMap& pos, int radius)
{
    if (node == nil)
        return;

    const NodePos me = pos.at(node);

    // 색상: RED/BLACK을 시각적으로 구분
    // (펜/브러시는 DrawTree에서 세팅하지만, 노드별 색을 바꾸려면 여기서 선택)
    // -> 간단히: 노드 색에 따라 브러시만 교체
    HBRUSH brush = nullptr;
    COLORREF textColor = RGB(0, 0, 0);

    if (node->getColor() == RBTree::RED)
    {
        brush = CreateSolidBrush(RGB(220, 80, 80));
        textColor = RGB(255, 255, 255);
    }
    else
    {
        brush = CreateSolidBrush(RGB(60, 60, 60));
        textColor = RGB(255, 255, 255);
    }

    HGDIOBJ oldBrush = SelectObject(hdc, brush);
    const COLORREF oldText = SetTextColor(hdc, textColor);
    const int oldBkMode = SetBkMode(hdc, TRANSPARENT);

    DrawNode(hdc, me.x, me.y, radius, node->getKey());

    SetBkMode(hdc, oldBkMode);
    SetTextColor(hdc, oldText);
    SelectObject(hdc, oldBrush);
    DeleteObject(brush);

    DrawNodesRecursive(hdc, node->getLeft(), nil, pos, radius);
    DrawNodesRecursive(hdc, node->getRight(), nil, pos, radius);
}

void TreeRenderer::DrawTree(HDC hdc, const RBTree::RBTree* tree, const RECT& clientRc)
{
    if (tree == nullptr)
        return;

    const RBTree::Node* root = tree->getRoot();
    const RBTree::Node* nil = tree->getNil();

    if (root == nil)
        return;

    // 1) Layout
    PosMap pos;
    pos.reserve(128);

    int xIndex = 0;
    LayoutInOrder(
        root,
        nil,
        0,
        xIndex,
        pos,
        TreeRenderConfig::StartX,
        TreeRenderConfig::StartY,
        TreeRenderConfig::XGap,
        TreeRenderConfig::YGap);

    // 2) Pen for edges
    HPEN pen = CreatePen(PS_SOLID, TreeRenderConfig::EdgeWidth, RGB(60, 60, 60));
    HGDIOBJ oldPen = SelectObject(hdc, pen);

    const int radius = TreeRenderConfig::NodeRadius;

    DrawEdgesRecursive(hdc, root, nil, pos, radius);
    DrawNodesRecursive(hdc, root, nil, pos, radius);

    SelectObject(hdc, oldPen);
    DeleteObject(pen);
}
