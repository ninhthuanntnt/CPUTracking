#include "TCHAR.h"
#include "pdh.h"
#include <stdio.h>
#include <iostream>
#include <stdlib.h>
#include <curses.h>
#include <gdiplus.h>
#include <windows.h>
#define TOTAL_BYTES_READ    1024
#define OFFSET_BYTES        1024
#define BUFFER 8192
#pragma comment(lib, "pdh.lib")
#pragma comment (lib,"Gdiplus.lib")
using namespace Gdiplus;


BOOL readStringFromRegistry(HKEY hKeyParent, PWCHAR subkey, PWCHAR valueName, PWCHAR* readData)
{
	HKEY hKey;
	DWORD len = TOTAL_BYTES_READ;
	DWORD readDataLen = len;
	PWCHAR readBuffer = (PWCHAR)malloc(sizeof(PWCHAR) * len);
	if (readBuffer == NULL)
		return FALSE;
	//Check if the registry exists
	DWORD Ret = RegOpenKeyEx(
		hKeyParent,
		subkey,
		0,
		KEY_READ,
		&hKey
	);
	if (Ret == ERROR_SUCCESS)
	{
		Ret = RegQueryValueEx(
			hKey,
			valueName,
			NULL,
			NULL,
			(BYTE*)readBuffer,
			&readDataLen
		);
		while (Ret == ERROR_MORE_DATA)
		{
			// Get a buffer that is big enough.
			len += OFFSET_BYTES;
			readBuffer = (PWCHAR)realloc(readBuffer, len);
			readDataLen = len;
			Ret = RegQueryValueEx(
				hKey,
				valueName,
				NULL,
				NULL,
				(BYTE*)readBuffer,
				&readDataLen
			);
		}
		if (Ret != ERROR_SUCCESS)
		{
			RegCloseKey(hKey);
			return false;;
		}
		*readData = readBuffer;
		RegCloseKey(hKey);
		return true;
	}
	else
	{
		return false;
	}
}

BOOL readDwordValueRegistry(HKEY hKeyParent, PWCHAR subkey, PWCHAR valueName, DWORD* readData)
{
	HKEY hKey;
	DWORD Ret;
	//Check if the registry exists
	Ret = RegOpenKeyEx(
		hKeyParent,
		subkey,
		0,
		KEY_READ,
		&hKey
	);
	if (Ret == ERROR_SUCCESS)
	{
		DWORD data;
		DWORD len = sizeof(DWORD);//size of data
		Ret = RegQueryValueEx(
			hKey,
			valueName,
			NULL,
			NULL,
			(LPBYTE)(&data),
			&len
		);
		if (Ret == ERROR_SUCCESS)
		{
			RegCloseKey(hKey);
			(*readData) = data;
			return TRUE;
		}
		RegCloseKey(hKey);
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}

static float CalculateCPULoad(unsigned long long idleTicks, unsigned long long totalTicks)
{
	static unsigned long long _previousTotalTicks = 0;
	static unsigned long long _previousIdleTicks = 0;

	unsigned long long totalTicksSinceLastTime = totalTicks - _previousTotalTicks;
	unsigned long long idleTicksSinceLastTime = idleTicks - _previousIdleTicks;


	float ret = 1.0f - ((totalTicksSinceLastTime > 0) ? ((float)idleTicksSinceLastTime) / totalTicksSinceLastTime : 0);

	_previousTotalTicks = totalTicks;
	_previousIdleTicks = idleTicks;
	return ret;
}

static unsigned long long FileTimeToInt64(const FILETIME& ft)
{
	return (((unsigned long long)(ft.dwHighDateTime)) << 32) | ((unsigned long long)ft.dwLowDateTime);
}

float GetCPULoad()
{
	FILETIME idleTime, kernelTime, userTime;
	return GetSystemTimes(&idleTime, &kernelTime, &userTime) ? CalculateCPULoad(FileTimeToInt64(idleTime), FileTimeToInt64(kernelTime) + FileTimeToInt64(userTime)) : -1.0f;
}

#define SECOND_TIMER 1000

#define O_AXIS_X 700
#define O_AXIS_Y 600
#define X_AXIS_X 1200
#define Y_AXIS_Y 100
#define ADD_PER_GRAPH_POINT 20

#define MAX_COOR 26
const LPCWSTR ClassName = L"CPU Tracking";

LRESULT CALLBACK WindowProcedure(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

// khai bao ham
void AddControls(HWND hwnd);
HWND CreateTextField(LPCWSTR text, int x, int y, int w, int h);
void updateDataThread(HWND hwnd);
void DrawLine(HDC hdc, int x1, int y1, int x2, int y2);
void DrawGraphFromCoor(HDC hdc);
void OnPaint(HDC hdc);
void AddCoor(int y);
// khai bao cac truong UI;
HWND baseSpeedTF;
HWND currentSpeedTF;
HWND cpuInfoTF;
HWND cpuUtilizationTF;
int coordinates[MAX_COOR];

// khai bao cac truong tinh toan
static PDH_HQUERY cpuQuery;
static PDH_HCOUNTER cpuTotal;
static PDH_FMT_COUNTERVALUE counterval;

DWORD cpufrequency;
PWCHAR cpuInfomation = nullptr;

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	// Khoi tao gia tri

	for (int i = 0; i < MAX_COOR; i++)
	{
		coordinates[i] = O_AXIS_Y;
	}

	// Lay cac thong tin tu registry

	readDwordValueRegistry(HKEY_LOCAL_MACHINE, (PWCHAR)L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", (PWCHAR)L"~MHz", &cpufrequency);
	readStringFromRegistry(HKEY_LOCAL_MACHINE, (PWCHAR)L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", (PWCHAR)L"ProcessorNameString", &cpuInfomation);
	// Khoi tao bo query

	PdhOpenQuery(NULL, NULL, &cpuQuery);
	PdhAddCounter(cpuQuery, L"\\processor information(_total)\\% processor performance", NULL, &cpuTotal);
	

	//PdhCollectQueryData(cpuQuery);

	PdhCollectQueryData(cpuQuery);
	PdhGetFormattedCounterValue(cpuTotal, PDH_FMT_DOUBLE, NULL, &counterval);

	// Khoi tao GDI
	GdiplusStartupInput gdiplusStartupInput;
	ULONG_PTR           gdiplusToken;
	GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);
	// Khoi tao WindowClass
	WNDCLASSEX wc;

	wc.cbSize			= sizeof(WNDCLASSEX);                   // Kich thuoc bo nho trong cua window class
	wc.style			= 0;                                    // kieu class
	wc.lpfnWndProc		= WindowProcedure;						// thu tuc cua window
	wc.cbClsExtra		= 0;									// So luong vung nho duoc cap phat cho class nay thuong la 0
	wc.cbWndExtra		= 0;									// So luong vung nho duoc cap phat cho moi window thuoc loai class nay thuong la 0
	wc.hInstance		= hInstance;							// Doi tuong quan ly thong win cua cua so 
	wc.hIcon			= LoadIcon(NULL, IDI_APPLICATION);      // Icon lon cua cua so (Xuat hien khi nhan alt + tab)
	wc.hCursor			= LoadCursor(NULL, IDC_ARROW);          // Kieu con tro su dung trong cua so (Xuat hien khi nhan alt + tab)
	wc.hbrBackground	= CreateSolidBrush(RGB(0, 0, 0));		// Mau nen
	wc.lpszClassName	= ClassName;							// Tieu de cua window class
	wc.lpszMenuName		= 0;
	wc.hIconSm			= LoadIcon(NULL, IDI_APPLICATION);      // Icon nho cua cua so (Phia tren ben trai canh tieu de)

	if (!RegisterClassEx(&wc))
	{
		MessageBox(NULL, L"Window Registration Failed!", L"Error!",
			MB_ICONEXCLAMATION | MB_OK);
		return 0;
	}

	// Khoi tao cua so
	HWND hwnd;
	hwnd = CreateWindowEx(
		WS_EX_OVERLAPPEDWINDOW,                 // kieu mo rong cua window style
		ClassName,                              // ten window class
		L"CPU Tracking",						// title
		WS_OVERLAPPEDWINDOW,                    // kieu window style
		CW_USEDEFAULT, CW_USEDEFAULT,           // toa do X,Y khi cua so mo len
		CW_USEDEFAULT, CW_USEDEFAULT,           // Chieu cao va chieu rong cua cua so
		NULL,                                   // Cua so cha
		NULL,                                   // Phan xu ly menu cua chuong trinh
		hInstance,                              // Instance cua chuong trinh
		NULL);                                  // Con tro dung de gui cac thong tin bo sung

	if (hwnd == NULL)
	{
		MessageBox(NULL, L"Window Creation Failed!", L"Error!",
			MB_ICONEXCLAMATION | MB_OK);
		return 0;
	}
	ShowWindow(hwnd, nCmdShow);
	UpdateWindow(hwnd);

	// Vong lap message
	MSG Msg;

	while (GetMessage(&Msg, NULL, 0, 0) > 0)
	{

		TranslateMessage(&Msg);
		DispatchMessage(&Msg);
	}

	return Msg.wParam;
}

LRESULT CALLBACK WindowProcedure(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{	
	static int i = 0;
	HDC hdcStatic = (HDC)wParam;
	HDC          hdc;
	PAINTSTRUCT  ps;
	switch (msg)
	{
	case WM_CREATE:
		AddControls(hwnd);
		SetTimer(hwnd, SECOND_TIMER, SECOND_TIMER, NULL); // Bat dau timer cho viec cap nhat du lieu len giao dien ung dung
		//other initialisation stuff
		break;
	case WM_CTLCOLORSTATIC:
		SetTextColor(hdcStatic, RGB(0, 240, 0));	// dat mau cho phan chu
		SetBkColor(hdcStatic, RGB(0, 0, 0));			// dat nen cho phan chu
		return (INT_PTR)CreateSolidBrush(RGB(0, 0, 0)); // dat nen cho phan con lai cua window
	case WM_SETTEXT:
	{

		// Lay du lieu tu chuoi lParam co dang ....-.....-.....
		// 1: Current Speed, 2: Cpu Utilization

		WCHAR* pwc;
		WCHAR* ptr;
		pwc = wcstok_s((WCHAR*)lParam, L"-", &ptr);
		int choosen = 1;
		while (pwc != NULL)
		{
			if(choosen==1)
				SetWindowTextW(currentSpeedTF, pwc);
			if (choosen == 2) 
			{
				SetWindowTextW(cpuUtilizationTF, pwc);
				WCHAR* pEnd;
				double cpuUtilization = wcstof((WCHAR*)pwc, &pEnd);
				int y = O_AXIS_Y - (O_AXIS_Y - Y_AXIS_Y) * cpuUtilization / 100.0;
				AddCoor(y);
			}
			pwc = wcstok_s(NULL, L"-", &ptr);
			choosen++;
		}

		break;
	}
		
	case WM_TIMER:
		if (wParam == SECOND_TIMER)
		{
			updateDataThread(hwnd);
		}
		SetTimer(hwnd, SECOND_TIMER, SECOND_TIMER, NULL); // 
		break;
	case WM_PAINT:

		hdc = BeginPaint(hwnd, &ps);
		OnPaint(hdc);
		DrawGraphFromCoor(hdc);
		EndPaint(hwnd, &ps);
		i += 50; 
		break;
	case WM_CLOSE:
		DestroyWindow(hwnd);
		break;
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	default:
		return DefWindowProc(hwnd, msg, wParam, lParam);
	}
	return 0;
}

HWND CreateTextField(HWND hwnd, LPCWSTR text, int x, int y, int w, int h)
{
	return CreateWindowW(L"static", text, WS_VISIBLE | WS_CHILD, x, y, w, h, hwnd, NULL, NULL, NULL);
}

int i = 0;
void AddControls(HWND hwnd)
{
	wchar_t baseSpeed[256];
	swprintf_s(baseSpeed, L"%d Mhz", (int)cpufrequency);
	CreateTextField(hwnd, L"Base speed:", 100, 100, 120, 50);
	baseSpeedTF = CreateTextField(hwnd, baseSpeed, 250, 100, 100, 50);

	// Hien thi thong tin CPU
	CreateTextField(hwnd, (WCHAR*)cpuInfomation, 500, 10, 350, 50);

	CreateTextField(hwnd, L"Current speed:", 100, 150, 120, 50);
	currentSpeedTF = CreateTextField(hwnd, L"0", 250, 150, 100, 50);

	CreateTextField(hwnd, L"Cpu utilization:", 100, 200, 120, 50);
	cpuUtilizationTF = CreateTextField(hwnd, L"0", 250, 200, 100, 50);

	// Tao cac thong so cho do thi
	CreateTextField(hwnd, L"Time(s)",(O_AXIS_X + (X_AXIS_X - O_AXIS_X)/2) , O_AXIS_Y + 20, 100, 50);
	CreateTextField(hwnd, L"0s", X_AXIS_X + 10, O_AXIS_Y + 10, 20, 20);
	CreateTextField(hwnd, L"25s", O_AXIS_X, O_AXIS_Y + 10, 30, 20);
	CreateTextField(hwnd, L"Utilization(%)", O_AXIS_X - 130, (O_AXIS_Y - (O_AXIS_Y - Y_AXIS_Y)/2), 100, 30);
	CreateTextField(hwnd, L"0%", O_AXIS_X - 35, O_AXIS_Y - 20, 30, 20);
	CreateTextField(hwnd, L"100%", O_AXIS_X - 55, Y_AXIS_Y - 20, 40, 20);
}

void updateDataThread(HWND hwnd)
{
	//HWND &hwnd = *(HWND*)(lpParameter);
	PdhCollectQueryData(cpuQuery);
	PdhGetFormattedCounterValue(cpuTotal, PDH_FMT_DOUBLE, NULL, &counterval);
	double curSpeed = cpufrequency * counterval.doubleValue / 100;
	float cpuUtilization = GetCPULoad() * 100;

	wchar_t msg[256];
	swprintf_s(msg, L"%.2lf Mhz-%.2f%%", curSpeed, cpuUtilization);

	SendMessage(hwnd, WM_SETTEXT, 0, (LPARAM)msg);

	//RedrawWindow(hwnd, NULL, NULL, WM_PAINT);
	//SendMessage(hwnd, WM_PAINT, 0, (LPARAM)msg);
	RECT rect;
	rect.left = O_AXIS_X - 10;
	rect.top = Y_AXIS_Y - 10;
	rect.right = X_AXIS_X + 10;
	rect.bottom = O_AXIS_Y + 10;
	InvalidateRect(hwnd, &rect, true);
}

void DrawLine(HDC hdc, int x1, int y1, int x2, int y2) 
{
	Graphics g(hdc);
	Pen	pen(Color(0, 240, 0), 2);
	g.DrawLine(&pen, x1, y1, x2, y2);
}

void DrawGraphFromCoor(HDC hdc)
{
	Graphics g(hdc);
	Pen	pen(Color(0, 240, 0), 1);
	for (int i = 0; i < MAX_COOR - 1; i++)
	{
		DrawLine(hdc, O_AXIS_X + 20*i, coordinates[i], O_AXIS_X + 20 * (i+1), coordinates[i + 1]);
		DrawLine(hdc, O_AXIS_X + 20 * i, O_AXIS_Y - 5, O_AXIS_X + 20 * i, O_AXIS_Y + 5);
	}

	for (int i = 1; i < 5; i++) 
	{
		DrawLine(hdc, O_AXIS_X - 5, O_AXIS_Y - (O_AXIS_Y - Y_AXIS_Y) * i / 4, O_AXIS_X + 5, O_AXIS_Y - (O_AXIS_Y - Y_AXIS_Y) * i / 4);
	}

}

void OnPaint(HDC hdc)
{
	DrawLine(hdc, O_AXIS_X, O_AXIS_Y, X_AXIS_X, O_AXIS_Y);
	DrawLine(hdc, O_AXIS_X, O_AXIS_Y, O_AXIS_X, Y_AXIS_Y - 10);
}

void AddCoor(int y)
{
	for (int i = 0; i < MAX_COOR - 1; i++)
	{
		coordinates[i] = coordinates[i + 1];
	}
	coordinates[MAX_COOR - 1] = y;
}