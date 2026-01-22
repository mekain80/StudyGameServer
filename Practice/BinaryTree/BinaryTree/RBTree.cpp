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

void RBTree::RBTree::destroy(Node* node) noexcept
{
	if (node == nullptr || node == nil) return;

	destroy(node->getLeft());
	destroy(node->getRight());
	delete node;
}


bool RBTree::RBTree::Insert(int key) noexcept
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

bool RBTree::RBTree::Delete(int key) noexcept
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
		deleteFixup(fixupNode);

	root->setColor(BLACK);
	return true;
}

void RBTree::RBTree::deleteFixup(Node* node) noexcept
{
	while (node != root && node->getColor() == BLACK)
	{
		Node* parent = node->getParent();

		if (node == parent->getLeft())
		{
			Node* sibling = parent->getRight();

			// Case 2) sibling is RED
			if (sibling->getColor() == RED)
			{
				sibling->setColor(BLACK);
				parent->setColor(RED);
				rotateL(parent);

				sibling = parent->getRight();
			}

			// Case 3) sibling is BLACK,
			// sibling's left child is BLACK,
			// sibling's right child is BLACK
			if (sibling->getLeft()->getColor() == BLACK &&
				sibling->getRight()->getColor() == BLACK)
			{
				sibling->setColor(RED);
				node = parent;
				continue;
			}

			// Case 4) sibling is BLACK,
			// sibling's left child is RED,
			// sibling's right child is BLACK
			if (sibling->getRight()->getColor() == BLACK)
			{
				sibling->getLeft()->setColor(BLACK);
				sibling->setColor(RED);
				rotateR(sibling);

				sibling = parent->getRight();
			}

			// Case 5) sibling is BLACK,
			// sibling's right child is RED
			sibling->setColor(parent->getColor());
			parent->setColor(BLACK);
			sibling->getRight()->setColor(BLACK);
			rotateL(parent);

			node = root;
		}
		else
		{
			Node* sibling = parent->getLeft();

			// Case 2) sibling is RED
			if (sibling->getColor() == RED)
			{
				sibling->setColor(BLACK);
				parent->setColor(RED);
				rotateR(parent);

				sibling = parent->getLeft();
			}

			// Case 3) sibling is BLACK,
			// sibling's left child is BLACK,
			// sibling's right child is BLACK
			if (sibling->getLeft()->getColor() == BLACK &&
				sibling->getRight()->getColor() == BLACK)
			{
				sibling->setColor(RED);
				node = parent;
				continue;
			}

			// Case 4) sibling is BLACK,
			// sibling's right child is RED,
			// sibling's left child is BLACK
			if (sibling->getLeft()->getColor() == BLACK)
			{
				sibling->getRight()->setColor(BLACK);
				sibling->setColor(RED);
				rotateL(sibling);

				sibling = parent->getLeft();
			}

			// Case 5) sibling is BLACK,
			// sibling's left child is RED
			sibling->setColor(parent->getColor());
			parent->setColor(BLACK);
			sibling->getLeft()->setColor(BLACK);
			rotateR(parent);

			node = root;
		}
	}

	// Case 1) node is RED
	node->setColor(BLACK);
}


bool RBTree::RBTree::Find(int key, Node*& outNode) noexcept
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


void RBTree::RBTree::insertFixup(Node* node) noexcept
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


void RBTree::RBTree::rotateR(Node* node) noexcept
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

void RBTree::RBTree::rotateL(Node* node) noexcept
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

