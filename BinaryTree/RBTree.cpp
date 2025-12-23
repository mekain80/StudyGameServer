#include <iostream>
#include "RBTree.h"

RBTree::RBTree::RBTree()
{
	nil = new Node(0, nullptr, nullptr, BLACK);
	nil->setLeft(nil);
	nil->setRight(nil);
	nil->setParent(nil);

	root = nil;
}

RBTree::RBTree::RBTree(int rootKey)
{
	nil = new Node(0, nullptr, nullptr, BLACK);
	nil->setLeft(nil);
	nil->setRight(nil);
	nil->setParent(nil);

	root = new Node(rootKey, nil, nil, BLACK);
}


RBTree::RBTree::~RBTree()
{
	destroy(root);
	root = nil;
	delete nil;
	nil = nullptr;
}

void RBTree::RBTree::destroy(Node* node)
{
	if (node == nullptr || node == nil) return;

	destroy(node->getLeft());
	destroy(node->getRight());
	delete node;
}


bool RBTree::RBTree::Insert(int key)
{
	Node* link = root;
	Node* parent = nil;

	while (link != nil)
	{
		parent = link;
		if (key < link->getKey()) link = link->getLeft();
		else if (key > link->getKey()) link = link->getRight();
		else return false;
	}

	Node* node = new Node(key, nil, parent, RED);
	node->setParent(parent);

	if (parent == nil)
	{
		root = node;
	}
	else if (key < parent->getKey())
	{
		parent->setLeft(node);
	}
	else
	{
		parent->setRight(node);
	}

	insertFixup(node);
	root->setColor(BLACK);
	return true;
}

bool RBTree::RBTree::Delete(int key)
{
	Node* node = FindNode(key);
	if (node == nil) return false;

	Node* delNode = node;                     // 실제로 삭제될 노드(처음엔 z)
	NODE_COLOR originalColor = delNode->getColor();

	Node* fixupNode = nil;                   // fixup 시작 노드

	if (node->getLeft() == nil)
	{
		fixupNode = node->getRight();
		Transplant(node, node->getRight());
		delete node;
	}
	else if (node->getRight() == nil)
	{
		fixupNode = node->getLeft();
		Transplant(node, node->getLeft());
		delete node;
	}
	else
	{
		// successor = z의 오른쪽 서브트리 최소값
		delNode = Minimum(node->getRight());
		originalColor = delNode->getColor();
		fixupNode = delNode->getRight();

		if (delNode->getParent() == node)
		{
			fixupNode->setParent(delNode);
		}
		else
		{
			Transplant(delNode, delNode->getRight());
			delNode->setRight(node->getRight());
			delNode->getRight()->setParent(delNode);
		}

		Transplant(node, delNode);
		delNode->setLeft(node->getLeft());
		delNode->getLeft()->setParent(delNode);
		delNode->setColor(node->getColor());

		delete node;
	}

	// 삭제된 노드가 BLACK이면 double-black 보정 필요
	if (originalColor == BLACK)
		DeleteFixup(fixupNode);

	root->setColor(BLACK);
	return true;
}

void RBTree::RBTree::DeleteFixup(Node* node) noexcept
{
	while (node != root && node->getColor() == BLACK)
	{
		Node* parent = node->getParent();

		if (node == parent->getLeft())
		{
			Node* sibling = parent->getRight();

			// 2.2) 형제가 레드
			if (sibling->getColor() == RED)
			{
				sibling->setColor(BLACK);
				parent->setColor(RED);
				rotateL(parent);

				sibling = parent->getRight();
			}

			// 2.3) 형제가 블랙이고 형제의 양쪽 자식이 블랙
			if (sibling->getLeft()->getColor() == BLACK &&
				sibling->getRight()->getColor() == BLACK)
			{
				sibling->setColor(RED);
				node = parent;
				continue;
			}

			// 2.4) 형제가 블랙이고 왼자식 RED, 오른자식 BLACK
			if (sibling->getRight()->getColor() == BLACK)
			{
				sibling->getLeft()->setColor(BLACK);
				sibling->setColor(RED);
				rotateR(sibling);

				sibling = parent->getRight();
			}

			// 2.5) 형제가 블랙이고 형제의 오른자식이 레드
			sibling->setColor(parent->getColor());
			parent->setColor(BLACK);
			sibling->getRight()->setColor(BLACK);
			rotateL(parent);

			node = root;
		}
		else
		{
			Node* sibling = parent->getLeft();

			// 2.2) 형제가 레드
			if (sibling->getColor() == RED)
			{
				sibling->setColor(BLACK);
				parent->setColor(RED);
				rotateR(parent);
				sibling = parent->getLeft();
			}

			// 2.3) 형제가 블랙이고 형제의 양쪽 자식이 블랙
			if (sibling->getRight()->getColor() == BLACK &&
				sibling->getLeft()->getColor() == BLACK)
			{
				sibling->setColor(RED);
				node = parent;
				continue;
			}

			// 2.4) 형제가 블랙이고 오른자식 RED, 왼자식 BLACK
			if (sibling->getLeft()->getColor() == BLACK)
			{
				sibling->getRight()->setColor(BLACK);
				sibling->setColor(RED);
				rotateL(sibling);
				sibling = parent->getLeft();
			}

			// 2.5) 형제가 블랙이고 형제의 왼자식이 레드
			sibling->setColor(parent->getColor());
			parent->setColor(BLACK);
			sibling->getLeft()->setColor(BLACK);
			rotateR(parent);

			node = root;
		}
	}

	// 2.1) 기준 노드가 레드인 경우 → BLACK으로 마무리
	node->setColor(BLACK);
}


bool RBTree::RBTree::Find(int key, Node*& outNode)
{
	Node* node = root;

	while (node != nil)
	{
		if (node->getKey() == key)
		{
			outNode = node;
			return true;
		}

		node = (key < node->getKey()) ? node->getLeft() : node->getRight();
	}

	return false;
}

RBTree::Node* RBTree::RBTree::FindNode(int key) const noexcept
{
	Node* cur = root;
	while (cur != nil)
	{
		if (key == cur->getKey()) return cur;
		cur = (key < cur->getKey()) ? cur->getLeft() : cur->getRight();
	}
	return nil;
}

RBTree::Node* RBTree::RBTree::Minimum(Node* node) const noexcept
{
	Node* cur = node;
	while (cur->getLeft() != nil)
		cur = cur->getLeft();
	return cur;
}

void RBTree::RBTree::Transplant(Node* u, Node* v) noexcept
{
	Node* up = u->getParent();
	if (up == nil)
		root = v;
	else if (u == up->getLeft())
		up->setLeft(v);
	else
		up->setRight(v);

	v->setParent(up);
}


void RBTree::RBTree::insertFixup(Node* node)
{
	while (node != root && node->getParent() != nil && node->getParent()->getColor() == RED)
	{
		Node* parent = node->getParent();
		Node* grand = parent->getParent();
		if (grand == nil) break;

		const bool parentIsLeft = (grand->getLeft() == parent);
		Node* uncle = parentIsLeft ? grand->getRight() : grand->getLeft();
		const NODE_COLOR uncleColor = uncle->getColor();

		// Case 1) uncle RED -> recolor
		if (uncleColor == RED)
		{
			parent->setColor(BLACK);
			if (uncle != nil) uncle->setColor(BLACK);
			grand->setColor(RED);
			node = grand;
			continue;
		}

		// Case 2/3) uncle BLACK -> rotate
		if (parentIsLeft)
		{
			// LR
			if (node == parent->getRight())
			{
				rotateL(parent);
				node = parent;
				parent = node->getParent();
				grand = parent->getParent();
			}

			// LL
			parent->setColor(BLACK);
			grand->setColor(RED);
			rotateR(grand);
		}
		else
		{
			// RL
			if (node == parent->getLeft())
			{
				rotateR(parent);
				node = parent;
				parent = node->getParent();
				grand = parent->getParent();
			}

			// RR
			parent->setColor(BLACK);
			grand->setColor(RED);
			rotateL(grand);
		}
	}

	root->setColor(BLACK);
}


void RBTree::RBTree::rotateR(Node* node)
{
	Node* parent = node->getParent();
	Node* lChild = node->getLeft();
	if (lChild == nil)
		return;

	Node* temp = lChild->getRight();

	node->setLeft(temp);
	if (temp != nil)
		temp->setParent(node);

	lChild->setRight(node);
	node->setParent(lChild);

	// parent -> lChild connect
	lChild->setParent(parent);
	if (parent == nil)
	{
		root = lChild;
	}
	else if (parent->getLeft() == node)
	{
		parent->setLeft(lChild);
	}
	else
	{
		parent->setRight(lChild);
	}
}

void RBTree::RBTree::rotateL(Node* node)
{
	Node* parent = node->getParent();
	Node* rChild = node->getRight();
	if (rChild == nil) return;

	Node* temp = rChild->getLeft();

	node->setRight(temp);
	if (temp != nil) temp->setParent(node);

	rChild->setLeft(node);
	node->setParent(rChild);

	// parent -> rChild connect
	rChild->setParent(parent);
	if (parent == nil)
	{
		root = rChild;
	}
	else if (parent->getLeft() == node)
	{
		parent->setLeft(rChild);
	}
	else
	{
		parent->setRight(rChild);
	}
}



void RBTree::RBTree::Print()
{
	PrintInOrder(root);
	std::cout << "\n";
}

void RBTree::RBTree::PrintInOrder(Node* node)
{
	if (node == nil) return;

	PrintInOrder(node->getLeft());
	std::cout << node->getKey() << " ";
	PrintInOrder(node->getRight());
}


// ================================================
// Node

RBTree::Node::Node(int key, Node* nil, Node* parent, NODE_COLOR color)
{
	this->key = key;
	this->parent = parent ? parent : nil;
	left = nil;
	right = nil;
	this->color = color;
}

int RBTree::Node::getKey() const noexcept
{
	return key;
}

RBTree::NODE_COLOR RBTree::Node::getColor() const noexcept
{
	return color;
}

RBTree::Node* RBTree::Node::getParent() const noexcept
{
	return parent;
}

RBTree::Node* RBTree::Node::getLeft() const noexcept
{
	return left;
}

RBTree::Node* RBTree::Node::getRight() const noexcept
{
	return right;
}


void RBTree::Node::setKey(int key)
{
	this->key = key;
}

void RBTree::Node::setColor(NODE_COLOR color)
{
	this->color = color;
}

