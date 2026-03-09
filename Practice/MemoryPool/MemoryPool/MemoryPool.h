#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <new>

template <class T>
class MemoryPool
{
public:
	static constexpr std::uint32_t kDefaultInitSize = 100;
	static constexpr std::uint32_t kDefaultMaxSize = UINT32_MAX;

	// @param placementNew true면 Alloc에서 생성자, Free에서 소멸자 호출
	// @param sizeInitialize 초기 노드 개수
	// @param sizeMax 풀이 보유할 수 있는 최대 노드 개수
	MemoryPool(bool placementNew = true,
		std::uint32_t sizeInitialize = kDefaultInitSize,
		std::uint32_t sizeMax = kDefaultMaxSize) noexcept;

	// 반환되지 않은 노드의 객체 소멸자는 호출하지 않음
	virtual ~MemoryPool() noexcept;

	// 노드 하나를 할당하고 데이터 포인터 반환
	T* Alloc(void) noexcept;

	// 사용 중인 데이터 포인터를 풀에 반환
	bool Free(T* data) noexcept;

	// 풀이 보유한 전체 노드 수 반환
	inline int GetCapacityCount(void) const noexcept { return mCapacity; }

	// 현재 사용 중인 노드 수 반환
	inline int GetUseCount(void) const noexcept { return mUseCount; }

	MemoryPool(const MemoryPool&) = delete;
	MemoryPool& operator=(const MemoryPool&) = delete;
	MemoryPool(MemoryPool&&) = delete;
	MemoryPool& operator=(MemoryPool&&) = delete;

private:
	struct Node
	{
		std::uint32_t bufferGuardFront = 0;
		T data;
		std::uint32_t bufferGuardEnd = 0;
		Node* next = nullptr;
	};

	bool mIsPlacementNew;				// 생성자/소멸자 호출 정책
	std::uint32_t mbufferGuardValue;	// 노드 무결성 검사용 가드 값
	std::uint32_t mSizeInitialize;		// 초기 노드 개수
	std::uint32_t mSizeMax;				// 최대 노드 개수
	Node* mFreeNode;					// 반환된(미사용) 노드 단일 연결 리스트의 헤드
	int mCapacity;						// 풀이 보유한 전체 노드 수
	int mUseCount;						// 현재 사용 중인 노드 수
};

template <class T>
inline MemoryPool<T>::MemoryPool(bool placementNew,
	std::uint32_t sizeInitialize,
	std::uint32_t sizeMax) noexcept
	: mIsPlacementNew(placementNew),
	mbufferGuardValue(static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(this))),
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

	for (std::uint32_t i = 0; i < mSizeInitialize; ++i)
	{
		Node* newNode = static_cast<Node*>(std::malloc(sizeof(Node)));
		newNode->bufferGuardFront = mbufferGuardValue;
		newNode->bufferGuardEnd = mbufferGuardValue;
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
		if (static_cast<std::uint32_t>(mCapacity) >= mSizeMax)
		{
			return nullptr;
		}

		returnNode = static_cast<Node*>(std::malloc(sizeof(Node)));
		returnNode->bufferGuardFront = mbufferGuardValue;
		returnNode->bufferGuardEnd = mbufferGuardValue;
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

	if (ptrNode->bufferGuardFront != mbufferGuardValue ||
		ptrNode->bufferGuardEnd != mbufferGuardValue)
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
