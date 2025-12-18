#include <iostream>
#include "BinaryTree.h"

int main() 
{
	BinaryTree::BinaryTree tree(10);

	tree.Insert(15);
	tree.Insert(5);
	tree.Print();

	tree.Insert(8);
	tree.Insert(9);
	tree.Insert(7);
	tree.Insert(6);
	tree.Print();

	tree.Delete(7);
	tree.Delete(8);
	tree.Print();
}