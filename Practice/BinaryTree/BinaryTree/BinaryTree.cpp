#include <iostream>
#include "BinaryTree.h"

BinaryTree::BinaryTree::BinaryTree()
{
	root = new Node(0);
}

BinaryTree::BinaryTree::BinaryTree(int rootKey)
{
	root = new Node(rootKey);
}

BinaryTree::BinaryTree::~BinaryTree()
{
	destroy(root);
	root = nullptr;
}

void BinaryTree::BinaryTree::destroy(Node* node)
{
	if (node == nullptr)
		return;

	destroy(node->left);
	destroy(node->right);
	delete node;
}


bool BinaryTree::BinaryTree::Insert(int key)
{
	Node** link = &root;

	while (*link != nullptr)
	{
		if (key < (*link)->key)
			link = &((*link)->left);
		else if (key > (*link)->key)
			link = &((*link)->right);
		else
			return false;
	}

	*link = new Node(key);
	return true;
}

bool BinaryTree::BinaryTree::Delete(int key)
{
	Node* parent = nullptr;
	Node* cur = root;

	// 1) 삭제할 노드(cur) 찾기 + parent 추적
	while (cur != nullptr && cur->key != key)
	{
		parent = cur;
		if (key < cur->key) cur = cur->left;
		else cur = cur->right;
	}

	// 못 찾음
	if (cur == nullptr)
		return false;

	// helper: parent의 어떤 자식 포인터를 바꿀지
	auto ReplaceChild = [&](Node* newChild)
		{
			if (parent == nullptr)
			{
				// cur이 root인 경우
				root = newChild;
			}
			else if (parent->left == cur)
			{
				parent->left = newChild;
			}
			else
			{
				parent->right = newChild;
			}
		};

	// 2) 자식 0개(leaf)
	if (cur->left == nullptr && cur->right == nullptr)
	{
		ReplaceChild(nullptr);
		delete cur;
		return true;
	}

	// 3) 자식 1개
	if (cur->left == nullptr || cur->right == nullptr)
	{
		Node* child = (cur->left != nullptr) ? cur->left : cur->right;
		ReplaceChild(child);
		delete cur;
		return true;
	}

	// 4) 자식 2개
	//    오른쪽 서브트리에서 "최소 노드"(in-order successor) 찾기

	Node* succParent = cur;
	Node* succ = cur->right;
	while (succ->left != nullptr)
	{
		succParent = succ;
		succ = succ->left;
	}

	// successor의 key를 cur에 복사
	cur->key = succ->key;

	// successor는 왼쪽이 없고, 오른쪽만 있을 수 있음
	Node* succChild = succ->right; // nullptr일 수도 있음

	if (succParent->left == succ)
		succParent->left = succChild;
	else
		succParent->right = succChild;

	delete succ;
	return true;
}


bool BinaryTree::BinaryTree::Find(int key)
{
	Node* node = root;

	while (node != nullptr)
	{
		if (node->key == key)
			return true;

		node = (key < node->key) ? node->left : node->right;
	}

	return false;
}

void BinaryTree::BinaryTree::Print()
{
	PrintInOrder(root);
	std::cout << "\n";
}

void BinaryTree::BinaryTree::PrintInOrder(Node* node)
{
	if (node == nullptr)
	{
		return;
	}

	PrintInOrder(node->left);		// 1. left
	std::cout << node->key << " ";	// 2. self
	PrintInOrder(node->right);		// 3. right
}


BinaryTree::Node::Node(int key)
{
	this->key = key;
	left = nullptr;
	right = nullptr;
}
