#pragma once

#include "Session.h"

struct Character
{
    Session* session = nullptr;
    DWORD action = 0;
    BYTE direction = 0;
    short x = 0;
    short y = 0;
    int HP = 0;
};
