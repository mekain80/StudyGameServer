#pragma once
#include <Windows.h>

class CPacket
{
public:
	enum ePACKET
	{
		eBUFFER_DEFAULT = 1400 // 패킷의 기본 버퍼 사이즈.
	};

	CPacket();
	CPacket(int iBUfferSize);
	virtual ~CPacket();

	/// 패킷 청소
	void Clear(void);

	/// 버퍼 사이즈 얻기.
	/// Parameters: 없음.
	/// Return: (int)패킷 버퍼 사이즈 얻기.
	int	GetBufferSize(void) { return m_iBufferSize; }

	/// 현재 사용중인 사이즈 얻기.
	/// Parameters: 없음.
	/// Return: (int)사용중인 데이타 사이즈.
	int GetDataSize(void) { return m_iDataSize; }

	/// 버퍼 포인터 얻기.
	/// Parameters: 없음.
	/// Return: (char *)버퍼 포인터.
	char* GetBufferPtr(void) { return m_chpBuffer; }

	/// 버퍼 Pos 이동. (음수이동은 안됨)
	/// GetBufferPtr 함수를 이용하여 외부에서 강제로 버퍼 내용을 수정할 경우 사용. 
	int		MoveWritePos(int iSize);
	int		MoveReadPos(int iSize);

	/* ============================================================================= */
	// 연산자 오버로딩
	/* ============================================================================= */
	CPacket& operator = (CPacket& clSrcPacket);

	CPacket& operator << (unsigned char byValue);
	CPacket& operator << (char chValue);

	CPacket& operator << (short shValue);
	CPacket& operator << (unsigned short wValue);

	CPacket& operator << (int iValue);
	CPacket& operator << (long lValue);
	CPacket& operator << (float fValue);

	CPacket& operator << (__int64 iValue);
	CPacket& operator << (double dValue);

	CPacket& operator >> (BYTE& byValue);
	CPacket& operator >> (char& chValue);

	CPacket& operator >> (short& shValue);
	CPacket& operator >> (WORD& wValue);

	CPacket& operator >> (int& iValue);
	CPacket& operator >> (DWORD& dwValue);
	CPacket& operator >> (float& fValue);

	CPacket& operator >> (__int64& iValue);
	CPacket& operator >> (double& dValue);

	//////////////////////////////////////////////////////////////////////////
	// 데이타 얻기.
	//
	// Parameters: (char *)Dest 포인터. (int)Size.
	// Return: (int)복사한 사이즈.
	//////////////////////////////////////////////////////////////////////////
	int		GetData(char* chpDest, int iSize);

	//////////////////////////////////////////////////////////////////////////
	// 데이타 삽입.
	//
	// Parameters: (char *)Src 포인터. (int)SrcSize.
	// Return: (int)복사한 사이즈.
	//////////////////////////////////////////////////////////////////////////
	int		PutData(char* chpSrc, int iSrcSize);


private:
	int   m_iBufferSize;
	int   m_iDataSize;
	char* m_chpBuffer;
};