#include <stdint.h>
#include <string.h>

#include <Windows.h>
#include <mmsystem.h>
#pragma comment(lib, "winmm.lib")

#include "resource.h"

static constexpr auto APP_NAME = TEXT("JoyCursor");
static constexpr UINT WM_NOTIFY_ICON = WM_USER + 1;

static HICON appIcon;
static HWND appWindow;
static TCHAR aboutString[256];
static TCHAR menuAboutString[16];
static TCHAR menuExitString[16];

static HWND desktopWindow1;  //GetDesktopWindow
static HWND desktopWindow2;  //Progman

static void getDesktopWindows()
{
	desktopWindow1 = GetDesktopWindow();
	desktopWindow2 = FindWindow(TEXT("Progman"), TEXT("Program Manager"));
}

static void loadResources(HINSTANCE instance)
{
	appIcon = LoadIcon(instance, MAKEINTRESOURCE(IDI_ICON));
	LoadString(instance, IDS_STRING_ABOUT,         aboutString,        sizeof(aboutString)        / sizeof(TCHAR));
	LoadString(instance, IDS_STRING_MENU_ABOUT,    menuAboutString,    sizeof(menuAboutString)    / sizeof(TCHAR));
	LoadString(instance, IDS_STRING_MENU_EXIT,     menuExitString,     sizeof(menuExitString)     / sizeof(TCHAR));
}

static void enableControl();
static void enableGameCheck();
static void disableGameCheck();
static void disableControl();
static LRESULT CALLBACK wndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);


int WINAPI WinMain(HINSTANCE instance, HINSTANCE prev, LPSTR argv, int argc)
{
	loadResources(instance);
	getDesktopWindows();

	WNDCLASSEX wndClass = { 0 };
	wndClass.cbSize = sizeof(WNDCLASSEX);
	wndClass.hInstance = instance;
	wndClass.lpszClassName = APP_NAME;
	wndClass.hIcon = appIcon;
	wndClass.lpfnWndProc = wndProc;

	if (!RegisterClassEx(&wndClass)) {
		MessageBox(appWindow, TEXT("JoyCursor: RegisterClassEx failed!"), APP_NAME, MB_ICONERROR);
		return 1;
	}

	appWindow = CreateWindowEx(WS_EX_TOOLWINDOW, APP_NAME, APP_NAME, WS_POPUP, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, NULL, NULL, instance, NULL);
	if (appWindow == NULL || appWindow == INVALID_HANDLE_VALUE) {
		MessageBox(appWindow, TEXT("JoyCursor: CreateWindowEx failed!"), APP_NAME, MB_ICONERROR);
		return 1;
	}

	NOTIFYICONDATA nid = { 0 };
	nid.cbSize = sizeof(NOTIFYICONDATA);
	nid.hWnd = appWindow;
	lstrcpy(nid.szTip, APP_NAME);
	nid.hIcon = appIcon;
	nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
	nid.uCallbackMessage = WM_NOTIFY_ICON;

	Shell_NotifyIcon(NIM_ADD, &nid);

	enableControl();
	enableGameCheck();

	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	disableGameCheck();
	disableControl();
	Shell_NotifyIcon(NIM_DELETE, &nid);
	DestroyWindow(appWindow);

	return msg.wParam;
}


static constexpr UINT JOYSTICK_TIMER = 1;
static constexpr int  JOYSTICK_PERIOD_MS = 1000 / 60;

static constexpr UINT GAME_CHECK_TIMER = 2;
static constexpr int GAME_CHECK_PERIOD_MS = 500;

enum MenuItem
{
	MENU_ABOUT,
	MENU_EXIT,
};

static void enableControl()
{
	SetTimer(appWindow, JOYSTICK_TIMER, JOYSTICK_PERIOD_MS, NULL);
}

static void disableControl()
{
	KillTimer(appWindow, JOYSTICK_TIMER);
}

static void enableGameCheck()
{
	SetTimer(appWindow, GAME_CHECK_TIMER, GAME_CHECK_PERIOD_MS, NULL);
}

static void disableGameCheck()
{
	KillTimer(appWindow, GAME_CHECK_TIMER);
}

static void showAbout()
{
	MessageBox(appWindow, aboutString, APP_NAME, MB_ICONINFORMATION);
}

static void leftButtonDown(int x, int y)
{
	mouse_event(MOUSEEVENTF_LEFTDOWN, x, y, 0, 0);
}

static void leftButtonUp(int x, int y)
{
	mouse_event(MOUSEEVENTF_LEFTUP, x, y, 0, 0);
}

static void rightButtonDown(int x, int y)
{
	mouse_event(MOUSEEVENTF_RIGHTDOWN, x, y, 0, 0);
}

static void rightButtonUp(int x, int y)
{
	mouse_event(MOUSEEVENTF_RIGHTUP, x, y, 0, 0);
}

static void moveCursor(int x, int y)
{
	RECT rect;
	GetWindowRect(desktopWindow1, &rect);
	LONG width = rect.right - rect.left;
	LONG height = rect.bottom - rect.top;
	LONG absX = x * 65535 / width;
	LONG absY = y * 65535 / height;
	mouse_event(MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE, absX, absY, 0, 0);
}

static void moveCursorByXYMove(POINT& pos, int xPos, int yPos, int thold)
{
	int64_t xDiff = (int64_t)xPos - 32767;
	int64_t yDiff = (int64_t)yPos - 32767;

	bool cursorMoved = false;
	if (xDiff > thold) {
		pos.x++;
		cursorMoved = true;
	}
	if (xDiff < -thold) {
		pos.x--;
		cursorMoved = true;
	}
	if (yDiff > thold) {
		pos.y++;
		cursorMoved = true;
	}
	if (yDiff < -thold) {
		pos.y--;
		cursorMoved = true;
	}

	if (cursorMoved) {
		moveCursor(pos.x, pos.y);
	}
}

//GAME_CHECK_TIMER timeout callback
static void gameCheckTimer()
{
	HWND wnd = GetForegroundWindow();

	bool isDesktop = wnd == desktopWindow1 || wnd == desktopWindow2;
	if (!isDesktop) {
		char buf[16];
		UINT len = RealGetWindowClassA(wnd, buf, 15);
		buf[len] = 0;
		isDesktop = strcmp(buf, "WorkerW") == 0;
	}

	if (!isDesktop) {
		RECT rect;
		if (GetWindowRect(wnd, &rect)) {
			RECT desktop;
			GetWindowRect(desktopWindow1, &desktop);  //get desktop size again
			if (rect.left   == desktop.left  &&
				rect.right  == desktop.right &&
				rect.top    == desktop.top   &&
				rect.bottom == desktop.bottom){  //foreground window not desktop and fullscreen, it's GAME :)
				moveCursor(desktop.right, desktop.bottom);
				disableControl();
				return;
			}
		}
	}
	enableControl();
}

//JOYSTICK_TIMER timeout callback
static void controlTimer()
{
	JOYINFOEX joyInfoEx = { 0 };
	joyInfoEx.dwSize = sizeof(JOYINFOEX);
	joyInfoEx.dwFlags = JOY_RETURNALL;
	if (joyGetPosEx(JOYSTICKID1, &joyInfoEx) == JOYERR_NOERROR)
	{
		//cursur move
		POINT pos;
		GetCursorPos(&pos);

		for (int i = 10000; i < 20000; i += 2000)
			moveCursorByXYMove(pos, joyInfoEx.dwXpos, joyInfoEx.dwYpos, i);

		for (int i = 20000; i < 30000; i += 1000)
			moveCursorByXYMove(pos, joyInfoEx.dwXpos, joyInfoEx.dwYpos, i);

		for (int i = 30000; i < 32000; i += 500)
			moveCursorByXYMove(pos, joyInfoEx.dwXpos, joyInfoEx.dwYpos, i);

		//mouse button
		static bool aButton = false;
		static bool bButton = false;

		if ((joyInfoEx.dwButtons & JOY_BUTTON1) != 0) {
			if (!aButton) {
				aButton = true;
				leftButtonDown(pos.x, pos.y);
			}
		}
		else {
			if (aButton) {
				aButton = false;
				leftButtonUp(pos.x, pos.y);
			}
		}

		if ((joyInfoEx.dwButtons & JOY_BUTTON2) != 0) {
			if (!bButton) {
				bButton = true;
				rightButtonDown(pos.x, pos.y);
			}
		}
		else {
			if (bButton) {
				bButton = false;
				rightButtonUp(pos.x, pos.y);
			}
		}
	}
}


LRESULT wndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg){
	case WM_NOTIFY_ICON:
		if (lParam == WM_LBUTTONDBLCLK) {
			showAbout();
		}
		else if (lParam == WM_RBUTTONUP) {
			HMENU menu = CreatePopupMenu();
			InsertMenu(menu, 0, MF_STRING, MENU_ABOUT, menuAboutString);
			InsertMenu(menu, 1, MF_STRING, MENU_EXIT,  menuExitString);

			POINT pt;
			GetCursorPos(&pt);
			TrackPopupMenu(menu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, NULL);
			DestroyMenu(menu);
		}
		break;

	case WM_TIMER:
		if (wParam == JOYSTICK_TIMER) {
			controlTimer();
		}
		else if (wParam == GAME_CHECK_TIMER) {
			gameCheckTimer();
		}
		break;

	case WM_COMMAND:
		if (LOWORD(wParam) == MENU_ABOUT) {
			showAbout();
		}
		else if (LOWORD(wParam == MENU_EXIT)) {
			PostQuitMessage(0);
		}
		break;
	}

	return DefWindowProc(hwnd, msg, wParam, lParam);
}
