#include "stdafx.h"

#include <conio.h>

#include "ServerControl.h"
#include "Network.h"

void ServerControl() noexcept
{
    // 키보드 컨트롤 잠금,풀림 변수
    static bool bControlMode = false;
    //------------------------------------------------------
    // L : 컨트롤 Lock / U : 컨트롤 Unlock / Q : 서버 종료
    //------------------------------------------------------
    if (_kbhit())
    {
        WCHAR ControlKey = _getwch();
        // 키보드 제어 허용
        if (L'u' == ControlKey || L'U' == ControlKey)
        {
            bControlMode = true;
            // 관련 키 도움말 출력.
            wprintf(L"Control Mode : Press Q - Quit \n");
            wprintf(L"Control Mode : Press L - Key Lock \n");
        }
        // 키보드 제어 잠금
        if ((L'l' == ControlKey || L'L' == ControlKey) && bControlMode)
        {
            wprintf(L"Control Lock..! Press U - Control Unlock\n");
            bControlMode = false;
        }
        if ((L'q' == ControlKey || L'Q' == ControlKey) && bControlMode)
        {
            gShutdown = true;
        }
    }
}
