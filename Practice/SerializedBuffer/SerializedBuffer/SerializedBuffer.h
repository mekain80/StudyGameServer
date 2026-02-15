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
	SerializedBuffer(int iBUfferSize);
	virtual ~SerializedBuffer();

	/// 패킷 청소
	void Clear();

	/// 버퍼 사이즈 얻기.
	/// Return: (int)패킷 버퍼 사이즈 얻기.
	int	GetBufferSize() { return mBufferSize; }

	/// 현재 사용중인 사이즈 얻기.
	/// Return: (int)사용중인 데이타 사이즈.
	int GetDataSize() { return mDataSize; }

	/// 버퍼 포인터 얻기.
	/// Return: (char *)버퍼 포인터.
	char* GetBufferPtr() { return mBuffer; }

	/// 버퍼 Pos 이동. (음수이동은 안됨)
	/// GetBufferPtr 함수를 이용하여 외부에서 강제로 버퍼 내용을 수정할 경우 사용. 
	int		MoveWritePos(int iSize);
	int		MoveReadPos(int iSize);

	/* ============================================================================= */
	// 연산자 오버로딩
	/* ============================================================================= */
	SerializedBuffer& operator = (SerializedBuffer& clSrcPacket);

	SerializedBuffer& operator << (unsigned char byValue);
	SerializedBuffer& operator << (char chValue);
	SerializedBuffer& operator << (unsigned short wValue);
	SerializedBuffer& operator << (short shValue);
	SerializedBuffer& operator << (int iValue);
	SerializedBuffer& operator << (unsigned long lValue);
	SerializedBuffer& operator << (long lValue);
	SerializedBuffer& operator << (float fValue);
	SerializedBuffer& operator << (__int64 iValue);
	SerializedBuffer& operator << (double dValue);

	//SerializedBuffer& operator<<(std::uint8_t  v) { writePod(v); return *this; }
	//SerializedBuffer& operator<<(std::int8_t   v) { writePod(v); return *this; }
	//SerializedBuffer& operator<<(char          v) { writePod(v); return *this; }
	//SerializedBuffer& operator<<(std::uint16_t v) { writePod(v); return *this; }
	//SerializedBuffer& operator<<(std::int16_t  v) { writePod(v); return *this; }
	//SerializedBuffer& operator<<(std::uint32_t v) { writePod(v); return *this; }
	//SerializedBuffer& operator<<(std::int32_t  v) { writePod(v); return *this; }
	//SerializedBuffer& operator<<(std::uint64_t v) { writePod(v); return *this; }
	//SerializedBuffer& operator<<(std::int64_t  v) { writePod(v); return *this; }
	//SerializedBuffer& operator<<(float         v) { writePod(v); return *this; }
	//SerializedBuffer& operator<<(float         v) { writePod(v); return *this; }
	//SerializedBuffer& operator<<(double        v) { writePod(v); return *this; }



	SerializedBuffer& operator >> (BYTE& byValue);
	SerializedBuffer& operator >> (char& chValue);
	SerializedBuffer& operator >> (short& shValue);
	SerializedBuffer& operator >> (WORD& wValue);
	SerializedBuffer& operator >> (int& iValue);
	SerializedBuffer& operator >> (DWORD& dwValue);
	SerializedBuffer& operator >> (float& fValue);
	SerializedBuffer& operator >> (__int64& iValue);
	SerializedBuffer& operator >> (double& dValue);

	/// @breif 데이타 얻기.
	/// @param (char *)Dest 포인터. (int)Size.
	/// @return (int)복사한 사이즈.
	int		GetData(char* chpDest, int iSize);

	/// @breif 데이타 삽입
	/// @param (char *)Src 포인터. (int)SrcSize.
	/// @return (int)복사한 사이즈.
	int		PutData(char* chpSrc, int iSrcSize);


private:
	int   mBufferSize;
	int   mDataSize;
	char* mBuffer;
};