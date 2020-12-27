#include "TCHAR.h"
#include "pdh.h"
#define MAX_COOR 20
#include <stdio.h>
#include <gdiplus.h>
#pragma comment (lib,"Gdiplus.lib")

using namespace Gdiplus;

void DrawLine(HDC hdc, int x1, int y1, int x2, int y2)
{
	Graphics g(hdc);
	Pen	pen(Color(255, 255, 255), 2);
	g.DrawLine(&pen, x1, y1, x2, y2);
}

void DrawGraphFromCoor(HDC hdc, int coordinates[MAX_COOR][2])
{
	Graphics g(hdc);
	Pen	pen(Color(255, 255, 255), 1);
	for (int i = 0; i < MAX_COOR - 1; i++)
	{
		DrawLine(hdc, coordinates[i][0], coordinates[i][1], coordinates[i + 1][0], coordinates[i + 1][1]);
	}
}