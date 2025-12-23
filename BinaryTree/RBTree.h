#pragma once

namespace RBTree
{
	enum NODE_COLOR
	{
		BLACK = 0,
		RED
	};

	class Node
	{
	public:
		Node(int key, Node* nil, Node* parent, NODE_COLOR color = RED);

		int getKey() const noexcept;
		NODE_COLOR getColor() const noexcept;
		Node* getParent() const noexcept;
		Node* getLeft() const noexcept;
		Node* getRight() const noexcept;
		void setKey(int key);
		void setColor(NODE_COLOR color);
		void setLeft(Node* p) noexcept { left = p; }
		void setRight(Node* p) noexcept { right = p; }
		void setParent(Node* p) noexcept { parent = p; }

	private:
		int key;
		Node* parent;
		Node* left;
		Node* right;
		NODE_COLOR color;
	};

	class RBTree
	{
	public:
		RBTree();
		RBTree(int rootKey);
		~RBTree();

		bool Insert(int key);
		bool Delete(int key);
		bool Find(int key, Node*& outNode);

		Node* FindNode(int key) const noexcept;
		Node* Minimum(Node* node) const noexcept;
		void Transplant(Node* u, Node* v) noexcept;

		void DeleteFixup(Node* x) noexcept; // 삭제 보정

		Node* getRoot() const noexcept { return root; }
		Node* getNil() const noexcept { return nil; }


		void Print();
		void PrintInOrder(Node* node);

	private:
		void destroy(Node* node);
		void insertFixup(Node* node);

		void rotateR(Node* node);
		void rotateL(Node* node);

		Node* root;
		Node* nil;
	};
}