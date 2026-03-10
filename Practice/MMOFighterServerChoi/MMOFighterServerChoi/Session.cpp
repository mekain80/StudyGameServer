#include "stdafx.h"

#include <unordered_map>
#include "Session.h"

std::unordered_map<SOCKET, Session*> gSessionMap;
