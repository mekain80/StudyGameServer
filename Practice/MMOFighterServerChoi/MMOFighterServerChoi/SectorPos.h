#pragma once

struct SectorPos
{
    short x = -1;
    short y = -1;
};

struct SectorAround
{
	int count = 0;
	SectorPos around[9];
};