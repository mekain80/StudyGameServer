#pragma once

#include "Common.h"

class SerializedBuffer
{
public:
	enum class Constants
	{
		HeaderSize = sizeof(USHORT),
		BufferDefault = SERIALIZED_BUFFER_SIZE
	};

	SerializedBuffer();
	explicit SerializedBuffer(int bufferSize);
	SerializedBuffer(const SerializedBuffer& src);
	virtual ~SerializedBuffer() noexcept;

	void Clear() noexcept;

	bool EnqueueHeader(const char* buffer, int size) noexcept;
	bool Enqueue(const char* buffer, int size) noexcept;
	bool Dequeue(char* buffer, int size) noexcept;

	int GetBufferSize() const noexcept { return mMaxSize; }
	int GetDataSize() const noexcept { return mRear - mFront; }
	int GetHeaderSize() const noexcept { return static_cast<int>(Constants::HeaderSize); }
	int GetFullSize() const noexcept { return GetDataSize() + GetHeaderSize(); }
	int GetFreeSize() const noexcept { return mMaxSize - mRear; }

	char* GetBufferPtr() noexcept { return mBuffer; }
	const char* GetBufferPtr() const noexcept { return mBuffer; }
	char* GetContentBufferPtr() noexcept { return mBuffer + mFront; }
	const char* GetContentBufferPtr() const noexcept { return mBuffer + mFront; }

	int MoveWritePos(int size) noexcept;
	int MoveReadPos(int size) noexcept;

	SerializedBuffer& operator=(const SerializedBuffer& src);

	SerializedBuffer& operator<<(unsigned char byteValue) noexcept;
	SerializedBuffer& operator<<(char charValue) noexcept;
	SerializedBuffer& operator<<(unsigned short ushortValue) noexcept;
	SerializedBuffer& operator<<(short shortValue) noexcept;
	SerializedBuffer& operator<<(int intValue) noexcept;
	SerializedBuffer& operator<<(unsigned long ulongValue) noexcept;
	SerializedBuffer& operator<<(long longValue) noexcept;
	SerializedBuffer& operator<<(float floatValue) noexcept;
	SerializedBuffer& operator<<(__int64 int64Value) noexcept;
	SerializedBuffer& operator<<(double doubleValue) noexcept;

	SerializedBuffer& operator>>(unsigned char& byteValue) noexcept;
	SerializedBuffer& operator>>(char& charValue) noexcept;
	SerializedBuffer& operator>>(unsigned short& ushortValue) noexcept;
	SerializedBuffer& operator>>(short& shortValue) noexcept;
	SerializedBuffer& operator>>(int& intValue) noexcept;
	SerializedBuffer& operator>>(unsigned long& ulongValue) noexcept;
	SerializedBuffer& operator>>(long& longValue) noexcept;
	SerializedBuffer& operator>>(float& floatValue) noexcept;
	SerializedBuffer& operator>>(__int64& int64Value) noexcept;
	SerializedBuffer& operator>>(double& doubleValue) noexcept;

	static SerializedBuffer* Alloc() noexcept;
	static void Free(SerializedBuffer* buffer) noexcept;

private:
	char* mBuffer;
	int mHeaderFront;
	int mFront;
	int mRear;
	int mMaxSize;
	bool mPoolAllocated;

	static MemoryPool<SerializedBuffer> sBufferPool;
};
