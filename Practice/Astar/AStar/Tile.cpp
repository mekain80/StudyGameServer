#include "Tile.h"
#include <stdio.h>

char gTile[GRID_HEIGHT][GRID_WIDTH];
Info gTileInfo[GRID_HEIGHT][GRID_WIDTH];

HBRUSH gOpenBrush;
HBRUSH gCloseBrush;
HBRUSH gRouteBrush;
HBRUSH gStartBrush;
HBRUSH gEndBrush;

static void FillTile(HDC hdc, int x, int y, HBRUSH brush)
{
    RECT rc;
    rc.left = x;
    rc.top = y;
    rc.right = x + GRID_SIZE;
    rc.bottom = y + GRID_SIZE;
    FillRect(hdc, &rc, brush);
}

void RenderOpen(HDC hdc)
{
    for (int i = 0; i < GRID_WIDTH; i++)
    {
        for (int j = 0; j < GRID_HEIGHT; j++)
        {
            if (gTile[j][i] == (char)Mode::OPENLIST)
            {
                const int X = i * GRID_SIZE;
                const int Y = j * GRID_SIZE;
                FillTile(hdc, X, Y, gOpenBrush);
            }
        }
    }
}

void RenderClose(HDC hdc)
{
    for (int i = 0; i < GRID_WIDTH; i++)
    {
        for (int j = 0; j < GRID_HEIGHT; j++)
        {
            if (gTile[j][i] == (char)Mode::CLOSELIST)
            {
                const int X = i * GRID_SIZE;
                const int Y = j * GRID_SIZE;
                FillTile(hdc, X, Y, gCloseBrush);
            }
        }
    }
}

void RenderRoute(HDC hdc)
{
    for (int i = 0; i < GRID_WIDTH; i++)
    {
        for (int j = 0; j < GRID_HEIGHT; j++)
        {
            if (gTile[j][i] == (char)Mode::ROUTE)
            {
                const int X = i * GRID_SIZE;
                const int Y = j * GRID_SIZE;
                FillTile(hdc, X, Y, gRouteBrush);
            }
        }
    }
}

void RenderStart(HDC hdc)
{
    for (int i = 0; i < GRID_WIDTH; i++)
    {
        for (int j = 0; j < GRID_HEIGHT; j++)
        {
            if (gTile[j][i] == (char)Mode::START)
            {
                const int X = i * GRID_SIZE;
                const int Y = j * GRID_SIZE;
                FillTile(hdc, X, Y, gStartBrush);
            }
        }
    }
}

void RenderEnd(HDC hdc)
{
    for (int i = 0; i < GRID_WIDTH; i++)
    {
        for (int j = 0; j < GRID_HEIGHT; j++)
        {
            if (gTile[j][i] == (char)Mode::END)
            {
                const int X = i * GRID_SIZE;
                const int Y = j * GRID_SIZE;
                FillTile(hdc, X, Y, gEndBrush);
            }
        }
    }
}

void RenderText(HDC hdc)
{
    int X = 0;
    int Y = 0;

    HFONT hFont = CreateFont(
        10, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, ANSI_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
        DEFAULT_PITCH | FF_SWISS, L"Arial");

    // 폰트 선택/원복
    HFONT oldFont = (HFONT)SelectObject(hdc, hFont);

    SetBkMode(hdc, TRANSPARENT);

    WCHAR G[10] = { 0 };
    WCHAR H[10] = { 0 };

    for (int i = 0; i < GRID_HEIGHT; i++)
    {
        for (int j = 0; j < GRID_WIDTH; j++)
        {
            if (gTileInfo[i][j].mG != 0)
            {
                X = j * GRID_SIZE;
                Y = i * GRID_SIZE;

                swprintf_s(G, L"%.1f", gTileInfo[i][j].mG);
                TextOutW(hdc, X, Y, G, (int)wcslen(G));       // 길이: G

                swprintf_s(H, L"%.1f", gTileInfo[i][j].mH);
                TextOutW(hdc, X, Y + 7, H, (int)wcslen(H));   // 길이: H
            }
        }
    }

    // 원복 + 해제 (누수 방지)
    SelectObject(hdc, oldFont);
    DeleteObject(hFont);
}
