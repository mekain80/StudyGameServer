#pragma once
#include <Windows.h>

class SerializedBuffer
{
public:
	enum Constants
	{
		eBUFFER_DEFAULT = 1024 // 패킷의 기본 버퍼 사이즈.
	};

	SerializedBuffer();
	SerializedBuffer(int bufferSize);
	SerializedBuffer(const SerializedBuffer& src);
	virtual ~SerializedBuffer() noexcept;

	// 패킷 청소
	void Clear() noexcept;

	// 버퍼 사이즈 얻기.
	int GetBufferSize() const noexcept { return mBufferSize; }

	// 현재 사용중인 사이즈 얻기.
	int GetDataSize() const noexcept { return mDataSize; }

	// 버퍼 포인터 얻기.
	char* GetBufferPtr() noexcept { return mBuffer; }
	const char* GetBufferPtr() const noexcept { return mBuffer; }

	// 버퍼 Pos 이동. (음수이동 불가)
	int MoveWritePos(int size) noexcept;
	int MoveReadPos(int size) noexcept;

	/* ============================================================================= */
	// 연산자 오버로딩
	/* ============================================================================= */
	SerializedBuffer& operator=(const SerializedBuffer& src);

	SerializedBuffer& operator<<(unsigned char  byteValue)  noexcept;
	SerializedBuffer& operator<<(char           charValue)  noexcept;
	SerializedBuffer& operator<<(unsigned short ushortValue) noexcept;
	SerializedBuffer& operator<<(short          shortValue) noexcept;
	SerializedBuffer& operator<<(int            intValue)   noexcept;
	SerializedBuffer& operator<<(unsigned long  ulongValue) noexcept;
	SerializedBuffer& operator<<(long           longValue)  noexcept;
	SerializedBuffer& operator<<(float          floatValue) noexcept;
	SerializedBuffer& operator<<(__int64        int64Value) noexcept;
	SerializedBuffer& operator<<(double         doubleValue) noexcept;

	SerializedBuffer& operator>>(unsigned char&		byteValue)  noexcept;
	SerializedBuffer& operator>>(char&				charValue)  noexcept;
	SerializedBuffer& operator>>(unsigned short&	ushortValue) noexcept;
	SerializedBuffer& operator>>(short&				shortValue) noexcept;
	SerializedBuffer& operator>>(int&				intValue)   noexcept;
	SerializedBuffer& operator>>(unsigned long&		ulongValue) noexcept;
	SerializedBuffer& operator>>(long&				longValue)  noexcept;
	SerializedBuffer& operator>>(float&				floatValue) noexcept;
	SerializedBuffer& operator>>(__int64&			int64Value) noexcept;
	SerializedBuffer& operator>>(double&			doubleValue) noexcept;

	// 데이타 얻기.
	int GetData(char* dest, int size) noexcept;

	// 데이타 삽입
	int PutData(const char* src, int srcSize) noexcept;

private:
	int   mBufferSize;
	int   mDataSize;
	char* mBuffer;
};
