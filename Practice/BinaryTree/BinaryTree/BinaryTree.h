#pragma once

namespace BinaryTree
{
	class Node
	{
	public:
		Node(int key);	

		int key;
		Node* left;
		Node* right;
	};

	class BinaryTree
	{
	public:
		BinaryTree();
		BinaryTree(int rootKey);
		~BinaryTree();


		bool Insert(int key);
		bool Delete(int key);
		bool Find(int key);

		void Print();
		void PrintInOrder(Node* node);
		Node* root;

	private:
		void destroy(Node* node);
	};
}