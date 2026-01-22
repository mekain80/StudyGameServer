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

		bool Insert(int key) noexcept;
		bool Delete(int key) noexcept;
		bool Find(int key, Node*& outNode) noexcept;

		Node* FindNode(int key) const noexcept;
		Node* Minimum(Node* node) const noexcept;
		void Transplant(Node* u, Node* v) noexcept;


		Node* getRoot() const noexcept { return root; }
		Node* getNil() const noexcept { return nil; }


		void Print();
		void PrintInOrder(Node* node);

	private:
		void destroy(Node* node) noexcept;
		void deleteFixup(Node* x) noexcept;
		void insertFixup(Node* node) noexcept;

		void rotateR(Node* node) noexcept;
		void rotateL(Node* node) noexcept;

		Node* root;
		Node* nil;
	};
}