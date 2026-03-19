#pragma once

#include <cstddef>
#include <cstdlib>
#include <new>

template <class T>
class MemoryPool
{
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
	struct Node
	{
		unsigned int bufferGuardFront = 0;
		T data;
		unsigned int bufferGuardEnd = 0;
		Node* next = nullptr;
	};

	bool mIsPlacementNew;
	unsigned int mBufferGuardValue;
	unsigned int mSizeInitialize;
	unsigned int mSizeMax;
	Node* mFreeNode;
	unsigned int mCapacity;
	unsigned int mUseCount;
};

template <class T>
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
		newNode->bufferGuardFront = mBufferGuardValue;
		newNode->bufferGuardEnd = mBufferGuardValue;
		if (mIsPlacementNew == false)
		{
			new (&(newNode->data)) T;
		}
		newNode->next = mFreeNode;
		mFreeNode = newNode;
		++mCapacity;
	}
}

template <class T>
inline MemoryPool<T>::~MemoryPool() noexcept
{
	Node* deleteNode = mFreeNode;
	while (deleteNode != nullptr)
	{
		Node* nextNode = deleteNode->next;
		if (mIsPlacementNew == false)
		{
			deleteNode->data.~T();
		}
		std::free(deleteNode);
		deleteNode = nextNode;
	}
}

template <class T>
inline T* MemoryPool<T>::Alloc(void) noexcept
{
	Node* returnNode = nullptr;

	if (mFreeNode != nullptr)
	{
		returnNode = mFreeNode;
		mFreeNode = mFreeNode->next;
		if (mIsPlacementNew)
		{
			new (&(returnNode->data)) T;
		}
	}
	else
	{
		if (mCapacity >= mSizeMax)
		{
			return nullptr;
		}

		returnNode = static_cast<Node*>(std::malloc(sizeof(Node)));
		returnNode->bufferGuardFront = mBufferGuardValue;
		returnNode->bufferGuardEnd = mBufferGuardValue;
		returnNode->next = nullptr;
		new (&(returnNode->data)) T;
		++mCapacity;
	}

	++mUseCount;
	return &(returnNode->data);
}

template <class T>
inline bool MemoryPool<T>::Free(T* data) noexcept
{
	if (data == nullptr)
	{
		return false;
	}

	Node* ptrNode = reinterpret_cast<Node*>(
		reinterpret_cast<char*>(data) - offsetof(Node, data));

	if (ptrNode->bufferGuardFront != mBufferGuardValue ||
		ptrNode->bufferGuardEnd != mBufferGuardValue)
	{
		return false;
	}

	if (mIsPlacementNew)
	{
		ptrNode->data.~T();
	}

	ptrNode->next = mFreeNode;
	mFreeNode = ptrNode;
	--mUseCount;

	return true;
}
