#pragma once
#include <iostream>
// TODO 
// 2) Rule of 3/5 위반(복사하면 터짐)
// 3) 헤더에 #include <iostream>은 과하게 무겁다

namespace BinaryTree
{
	class Node
	{
	public:
		Node(int key);
		int GetChildCnt();

		int key;
		Node* left;
		Node* right;
	};

	class BinaryTree
	{
	public:
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