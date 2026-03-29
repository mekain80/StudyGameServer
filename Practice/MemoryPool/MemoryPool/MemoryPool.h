#pragma once

#include <cstddef>
#include <cstdlib>
#include <new>

template<typename T>
struct MemoryPoolNode
{
	unsigned int BufferGuardFront = 0;
	T Data;
	unsigned int BufferGuardEnd = 0;
	MemoryPoolNode* Next = nullptr;
};

template<typename T>
class MemoryPool
{
	using Node = MemoryPoolNode<T>;

public:
	static constexpr unsigned int kDefaultInitSize = 100;
	static constexpr unsigned int kDefaultMaxSize = static_cast<unsigned int>(-1);

	MemoryPool(
		bool placementNew = true,
		unsigned int sizeInitialize = kDefaultInitSize,
		unsigned int sizeMax = kDefaultMaxSize) noexcept;

	virtual ~MemoryPool() noexcept;

	T* Alloc(void) noexcept;
	bool Free(T* data) noexcept;

	inline unsigned int GetCapacityCount(void) const noexcept { return mCapacity; }
	inline unsigned int GetUseCount(void) const noexcept { return mUseCount; }

	MemoryPool(const MemoryPool&) = delete;
	MemoryPool& operator=(const MemoryPool&) = delete;
	MemoryPool(MemoryPool&&) = delete;
	MemoryPool& operator=(MemoryPool&&) = delete;

private:
	bool mIsPlacementNew;
	unsigned int mBufferGuardValue;
	unsigned int mSizeInitialize;
	unsigned int mSizeMax;
	Node* mFreeNode;
	unsigned int mCapacity;
	unsigned int mUseCount;
};

template<typename T>
inline MemoryPool<T>::MemoryPool(
	bool placementNew,
	unsigned int sizeInitialize,
	unsigned int sizeMax) noexcept
	: mIsPlacementNew(placementNew),
	mBufferGuardValue(static_cast<unsigned int>(reinterpret_cast<size_t>(this))),
	mSizeInitialize(sizeInitialize),
	mSizeMax(sizeMax),
	mFreeNode(nullptr),
	mCapacity(0),
	mUseCount(0)
{
	if (mSizeInitialize > mSizeMax)
	{
		mSizeInitialize = mSizeMax;
	}

	for (unsigned int i = 0; i < mSizeInitialize; ++i)
	{
		Node* newNode = static_cast<Node*>(std::malloc(sizeof(Node)));
		newNode->BufferGuardFront = mBufferGuardValue;
		newNode->BufferGuardEnd = mBufferGuardValue;
		if (mIsPlacementNew == false)
		{
			new (&(newNode->Data)) T;
		}
		newNode->Next = mFreeNode;
		mFreeNode = newNode;
		++mCapacity;
	}
}

template<typename T>
inline MemoryPool<T>::~MemoryPool() noexcept
{
	Node* deleteNode = mFreeNode;
	while (deleteNode != nullptr)
	{
		Node* nextNode = deleteNode->Next;
		if (mIsPlacementNew == false)
		{
			deleteNode->Data.~T();
		}
		std::free(deleteNode);
		deleteNode = nextNode;
	}
}

template<typename T>
inline T* MemoryPool<T>::Alloc(void) noexcept
{
	Node* returnNode = nullptr;

	if (mFreeNode != nullptr)
	{
		returnNode = mFreeNode;
		mFreeNode = mFreeNode->Next;
		if (mIsPlacementNew)
		{
			new (&(returnNode->Data)) T;
		}
	}
	else
	{
		if (mCapacity >= mSizeMax)
		{
			return nullptr;
		}

		returnNode = static_cast<Node*>(std::malloc(sizeof(Node)));
		returnNode->BufferGuardFront = mBufferGuardValue;
		returnNode->BufferGuardEnd = mBufferGuardValue;
		returnNode->Next = nullptr;
		new (&(returnNode->Data)) T;
		++mCapacity;
	}

	++mUseCount;
	return &(returnNode->Data);
}

template<typename T>
inline bool MemoryPool<T>::Free(T* data) noexcept
{
	if (data == nullptr)
	{
		return false;
	}

	Node* ptrNode = reinterpret_cast<Node*>(
		reinterpret_cast<char*>(data) - offsetof(Node, Data));

	if (ptrNode->BufferGuardFront != mBufferGuardValue ||
		ptrNode->BufferGuardEnd != mBufferGuardValue)
	{
		return false;
	}

	if (mIsPlacementNew)
	{
		ptrNode->Data.~T();
	}

	ptrNode->Next = mFreeNode;
	mFreeNode = ptrNode;
	--mUseCount;

	return true;
}
