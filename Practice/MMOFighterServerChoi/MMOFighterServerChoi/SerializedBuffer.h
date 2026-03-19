#pragma once
#include <Windows.h>

class SerializedBuffer
{
public:
	enum Constants
	{
		eBUFFER_DEFAULT = 1024
	};

	SerializedBuffer();
	SerializedBuffer(int bufferSize);
	SerializedBuffer(const SerializedBuffer& src);
	virtual ~SerializedBuffer() noexcept;

	void Clear() noexcept;

	int GetBufferSize() const noexcept { return mBufferSize; }
	int GetDataSize() const noexcept { return static_cast<int>(mWrite - mRead); }
	int GetFreeSize() const noexcept { return static_cast<int>(mEnd - mWrite); }

	const char* GetBufferPtr() noexcept { return mBuffer; }
	const char* GetBufferPtr() const noexcept { return mBuffer; }

	const char* GetBufferBegin() noexcept { return mBegin; }
	const char* GetBufferBegin() const noexcept { return mBegin; }
	const char* GetBufferRead() noexcept { return mRead; }
	const char* GetBufferRead() const noexcept { return mRead; }
	const char* GetBufferWrite() noexcept { return mWrite; }
	const char* GetBufferWrite() const noexcept { return mWrite; }

	int MoveWritePos(int size) noexcept;
	int MoveReadPos(int size) noexcept;
	int MoveWritePointer(int size) noexcept { return MoveWritePos(size); }
	int MoveReadPointer(int size) noexcept { return MoveReadPos(size); }

	SerializedBuffer& operator=(const SerializedBuffer& src);

	SerializedBuffer& operator<<(unsigned char  byteValue) noexcept;
	SerializedBuffer& operator<<(char           charValue) noexcept;
	SerializedBuffer& operator<<(unsigned short ushortValue) noexcept;
	SerializedBuffer& operator<<(short          shortValue) noexcept;
	SerializedBuffer& operator<<(int            intValue) noexcept;
	SerializedBuffer& operator<<(unsigned long  ulongValue) noexcept;
	SerializedBuffer& operator<<(long           longValue) noexcept;
	SerializedBuffer& operator<<(float          floatValue) noexcept;
	SerializedBuffer& operator<<(__int64        int64Value) noexcept;
	SerializedBuffer& operator<<(double         doubleValue) noexcept;

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

	int GetData(char* dest, int size) noexcept;
	int PutData(const char* src, int srcSize) noexcept;

private:
	void AllocateBuffer(int bufferSize);
	void ReleaseBuffer() noexcept;
	void InitializePointers() noexcept;
	void ResetPointers() noexcept;

private:
	char* mBuffer;
	char* mBegin;
	char* mRead;
	char* mWrite;
	char* mEnd;
	int   mBufferSize;
	void* mPoolBlock;
};
