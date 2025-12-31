#pragma once
#include <vector>
#include <Windows.h>

#define DIRECTION 8
#define SLEEP_TIME 300

struct Node {
	int mX;
	int mY;
	double mG; // 누적 실제 비용
	double mH; // 휴리스틱/추정 비용
	double mF; // 평가 함수 : mF = mG + mH
	Node* mParent;

	Node()
	{
		mX = 0;
		mY = 0;
		mG = 0;
		mH = 0;
		mF = 0;
		mParent = nullptr;
	}

	Node(int x, int y)
	{
		mX = x;
		mY = y;
		mG = 0;
		mH = 0;
		mF = 0;
		mParent = nullptr;
	}
};

class Astar
{
public:
	Astar();
	virtual ~Astar();

	void RoutingStart(HWND hwnd);

	Node* mStart;
	Node* mEnd;
	std::vector<Node*> mOpenList;
	std::vector<Node*> mCloseList;

private:
	double calUclide(Node* node1, Node* node2);
	// TODO 이거 개선하기
	double calUclide(int x, int y, Node* node); 

	double calManhatan(Node* node1, Node* node2);

	bool isExistOpenList(int x, int y, Node* parent);

	void Clear();
};