#include "Astar.h"
#include "Tile.h"
#include <algorithm>

static constexpr int gDx[DIRECTION] = { 1,1,0,-1,-1,-1,0,1 };
static constexpr int gDy[DIRECTION] = { 0,-1,-1,-1,0,1,1,1 };

Astar::Astar()
{
	mStart = new Node;
	mEnd = new Node;
}

Astar::~Astar()
{
	delete mStart;
	delete mEnd;
}

void Astar::RoutingStart(HWND hWnd)
{
	HDC hdc = GetDC(hWnd);

	mOpenList.push_back(mStart);

	while (1)
	{
		Sleep(SLEEP_TIME);
		Node* node = mOpenList[(int)mOpenList.size() - 1];
		mOpenList.pop_back();

		// 목적지 도착 (기저 조건 확인)
		if (node->mX == mEnd->mX && node->mY == mEnd->mY)
		{
			node = node->mParent;
			while (mStart != node)
			{
				gTile[node->mY][node->mX] = (char)Mode::ROUTE;
				node = node->mParent;
			}

			gTile[mStart->mY][mStart->mX] = (char)Mode::START;
			RenderRoute(hdc);
			RenderStart(hdc);
			RenderText(hdc);

			Clear();
			mOpenList.clear();
			mCloseList.clear();
			return;
		}
		else
		{
			gTile[node->mY][node->mX] = (char)Mode::CLOSELIST;
			if (node != mStart)
				mCloseList.push_back(node);
			RenderClose(hdc);
		}

		int dx, dy;
		for (int i = 0; i < DIRECTION; i++)
		{
			dx = node->mX + gDx[i];
			dy = node->mY + gDy[i];

			// 경계 범위 밖으로 나가면 안됨.
			if (dx < 0 || dx >= GRID_WIDTH || dy < 0 || dy >= GRID_HEIGHT)
				continue;

			// 출발점 색깔 보존
			if (dx == mStart->mX && dy == mStart->mY)
				continue;

			if (gTile[dy][dx] == (char)Mode::CLOSELIST ||
				gTile[dy][dx] == (char)Mode::OBSTACLE)
				continue;

			if (isExistOpenList(dx, dy, node))
				continue;

			Node* newNode = new Node(dx, dy);
			newNode->mParent = node;
			// 유클리드 
			newNode->mG = node->mG + calUclide(newNode, node);
			// 맨하탄
			newNode->mH = calManhatan(newNode, mEnd);
			newNode->mF = newNode->mG + newNode->mH;

			gTileInfo[dy][dx].mG = newNode->mG;
			gTileInfo[dy][dx].mH = newNode->mH;
			mOpenList.push_back(newNode);
			
			if (dx != mEnd->mX || dy != mEnd->mY)
				gTile[newNode->mY][newNode->mX] = (char)Mode::OPENLIST;
			RenderOpen(hdc);
		}

		std::sort(mOpenList.begin(), mOpenList.end(), [](const Node* o1, const Node* o2) {
			return o1->mF > o2->mF;
		});
	}
}

double Astar::calUclide(Node* node1, Node* node2)
{
	double dx = node2->mX - node1->mX;
	double dy = node2->mY - node1->mY;

	return sqrt(dx * dx + dy * dy);
}

double Astar::calUclide(int x, int y, Node* node)
{
	double dx = x - node->mX;
	double dy = y - node->mY;

	return sqrt(dx * dx + dy * dy);
}

double Astar::calManhatan(Node* node1, Node* node2)
{
	double dx = abs(node2->mX - node1->mX);
	double dy = abs(node2->mY - node1->mY);

	return dx + dy;
}

bool Astar::isExistOpenList(int x, int y, Node* parent)
{
	for (int i = 0; i < mOpenList.size(); i++)
	{
		if (mOpenList[i]->mX == x && mOpenList[i]->mY == y)
		{
			double g = parent->mG + calUclide(x, y, parent);
			// 이번에 들어온 X, Y 가 G가 가깝다면 새롭게 Parent 설정
			if (g < mOpenList[i]->mG)
			{
				mOpenList[i]->mParent = parent;
				mOpenList[i]->mG = g;
				mOpenList[i]->mF = g + mOpenList[i]->mH;
			}
			return true;
		}
	}
	return false;
}

void Astar::Clear()
{
	for (int i = 0; i < mOpenList.size(); i++)
		delete mOpenList[i];

	for (int i = 0; i < mCloseList.size(); i++)
		delete mCloseList[i];
}
