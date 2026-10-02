// ****************************************************************************
// 
// RBTray
// Copyright (C) 1998-2010  Nikolay Redko, J.D. Purcell
// 
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
// 
// ****************************************************************************

#include <windows.h>
#include <stdlib.h>
#include "rbtray.h"
#include "resource.h"

#define MAXTRAYITEMS 64

static UINT WM_TASKBAR_CREATED;

static HINSTANCE _hInstance;
static HMODULE _hLib;
static HWND _hwndHook;
static HWND _hwndItems[MAXTRAYITEMS];
static HWND _hwndForMenu;

int FindInTray(HWND hwnd) {
	for (int i = 0; i < MAXTRAYITEMS; i++) {
		if (_hwndItems[i] == hwnd) return i;
	}
	return -1;
}

HICON GetWindowIcon(HWND hwnd) {
	HICON icon;
	if (icon = (HICON)SendMessage(hwnd, WM_GETICON, ICON_SMALL, 0)) return icon;
	if (icon = (HICON)SendMessage(hwnd, WM_GETICON, ICON_BIG, 0)) return icon;
	if (icon = (HICON)GetClassLongPtr(hwnd, GCLP_HICONSM)) return icon;
	if (icon = (HICON)GetClassLongPtr(hwnd, GCLP_HICON)) return icon;
	return LoadIcon(NULL, IDI_WINLOGO);
}

static void AddToTray(int i) {
	NOTIFYICONDATA nid;
	ZeroMemory(&nid, sizeof(nid));
	nid.cbSize           = NOTIFYICONDATA_V2_SIZE;
	nid.hWnd             = _hwndHook;
	nid.uID              = (UINT)i;
	nid.uFlags           = NIF_MESSAGE | NIF_ICON | NIF_TIP;
	nid.uCallbackMessage = WM_TRAYCMD;
	nid.hIcon            = GetWindowIcon(_hwndItems[i]);
	GetWindowText(_hwndItems[i], nid.szTip, sizeof(nid.szTip) / sizeof(nid.szTip[0]));
	nid.uVersion         = NOTIFYICON_VERSION;
	Shell_NotifyIcon(NIM_ADD, &nid);
	Shell_NotifyIcon(NIM_SETVERSION, &nid);
}

static void AddWindowToTray(HWND hwnd) {
	int i = FindInTray(NULL);
	if (i == -1) return;
	_hwndItems[i] = hwnd;
	AddToTray(i);
}

static void MinimizeWindowToTray(HWND hwnd) {
	// Don't minimize MDI child windows
	if ((UINT)GetWindowLongPtr(hwnd, GWL_EXSTYLE) & WS_EX_MDICHILD) return;

	// If hwnd is a child window, find parent window (e.g. minimize button in
	// Office 2007 (ribbon interface) is in a child window)
	if ((UINT)GetWindowLongPtr(hwnd, GWL_STYLE) & WS_CHILD) {
		hwnd = GetAncestor(hwnd, GA_ROOT);
	}

	// Add icon to tray if it's not already there
	if (FindInTray(hwnd) == -1) {
		AddWindowToTray(hwnd);
	}

	// Hide window
	ShowWindow(hwnd, SW_HIDE);
}

static void RemoveFromTray(int i) {
	NOTIFYICONDATA nid;
	ZeroMemory(&nid, sizeof(nid));
	nid.cbSize = NOTIFYICONDATA_V2_SIZE;
	nid.hWnd   = _hwndHook;
	nid.uID    = (UINT)i;
	Shell_NotifyIcon(NIM_DELETE, &nid);
}

static void RemoveWindowFromTray(HWND hwnd) {
	int i = FindInTray(hwnd);
	if (i == -1) return;
	RemoveFromTray(i);
	_hwndItems[i] = 0;
}

static void RestoreWindowFromTray(HWND hwnd) {
	ShowWindow(hwnd, SW_SHOW);
	SetForegroundWindow(hwnd);
	RemoveWindowFromTray(hwnd);
}

static void CloseWindowFromTray(HWND hwnd) {
	// Use PostMessage to avoid blocking if the program brings up a dialog on exit.
	// Also, Explorer windows ignore WM_CLOSE messages from SendMessage.
	PostMessage(hwnd, WM_CLOSE, 0, 0);

	Sleep(50);
	if (IsWindow(hwnd)) Sleep(50);

	if (!IsWindow(hwnd)) {
		// Closed successfully
		RemoveWindowFromTray(hwnd);
	}
}

void RefreshWindowInTray(HWND hwnd) {
	int i = FindInTray(hwnd);
	if (i == -1) return;
	if (!IsWindow(hwnd) || IsWindowVisible(hwnd)) {
		RemoveWindowFromTray(hwnd);
	}
	else {
		NOTIFYICONDATA nid;
		ZeroMemory(&nid, sizeof(nid));
		nid.cbSize = NOTIFYICONDATA_V2_SIZE;
		nid.hWnd   = _hwndHook;
		nid.uID    = (UINT)i;
		nid.uFlags = NIF_TIP;
		GetWindowText(hwnd, nid.szTip, sizeof(nid.szTip) / sizeof(nid.szTip[0]));
		Shell_NotifyIcon(NIM_MODIFY, &nid);
	}
}

void ExecuteMenu() {
	HMENU hMenu;
	POINT point;

	hMenu = CreatePopupMenu();
	if (!hMenu) {
		MessageBox(NULL, L"Error creating menu.", L"RBTray", MB_OK | MB_ICONERROR);
		return;
	}
	AppendMenu(hMenu, MF_STRING, IDM_ABOUT,   L"About RBTray");
	AppendMenu(hMenu, MF_STRING, IDM_EXIT,    L"Exit RBTray");
	AppendMenu(hMenu, MF_SEPARATOR, 0, NULL); //--------------
	AppendMenu(hMenu, MF_STRING, IDM_CLOSE,   L"Close Window");
	AppendMenu(hMenu, MF_STRING, IDM_RESTORE, L"Restore Window");

	GetCursorPos(&point);
	SetForegroundWindow(_hwndHook);

	TrackPopupMenu(hMenu, TPM_LEFTBUTTON | TPM_RIGHTBUTTON | TPM_RIGHTALIGN | TPM_BOTTOMALIGN, point.x, point.y, 0, _hwndHook, NULL);

	PostMessage(_hwndHook, WM_USER, 0, 0);
	DestroyMenu(hMenu);
}

BOOL CALLBACK AboutDlgProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam) {
	switch (Msg) {
		case WM_CLOSE:
			PostMessage(hWnd, WM_COMMAND, IDCANCEL, 0);
			break;
		case WM_COMMAND:
			switch (LOWORD(wParam)) {
				case IDOK:
					EndDialog(hWnd, TRUE);
					break;
				case IDCANCEL:
					EndDialog(hWnd, FALSE);
					break;
			}
			break;
		default:
			return FALSE;
	}
	return TRUE;
}

LRESULT CALLBACK HookWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	switch (msg) {
		case WM_COMMAND:
			switch (LOWORD(wParam)) {
				case IDM_RESTORE:
					RestoreWindowFromTray(_hwndForMenu);
					break;
				case IDM_CLOSE:
					CloseWindowFromTray(_hwndForMenu);
					break;
				case IDM_ABOUT:
					DialogBox(_hInstance, MAKEINTRESOURCE(IDD_ABOUT), _hwndHook, (DLGPROC)AboutDlgProc);
					break;
				case IDM_EXIT:
					SendMessage(_hwndHook, WM_DESTROY, 0, 0);
					break;
			}
			break;
		case WM_ADDTRAY:
			MinimizeWindowToTray((HWND)lParam);
			break;
		case WM_REMTRAY:
			RestoreWindowFromTray((HWND)lParam);
			break;
		case WM_REFRTRAY:
			RefreshWindowInTray((HWND)lParam);
			break;
		case WM_TRAYCMD:
			switch ((UINT)lParam) {
				case NIN_SELECT:
					RestoreWindowFromTray(_hwndItems[wParam]);
					break;
				case WM_CONTEXTMENU:
					_hwndForMenu = _hwndItems[wParam];
					ExecuteMenu();
					break;
				case WM_MOUSEMOVE:
					RefreshWindowInTray(_hwndItems[wParam]);
					break;
			}
			break;
		case WM_DESTROY:
			for (int i = 0; i < MAXTRAYITEMS; i++) {
				if (_hwndItems[i]) {
					RestoreWindowFromTray(_hwndItems[i]);
				}
			}
			UnRegisterHook();
			FreeLibrary(_hLib);
			PostQuitMessage(0);
			break;
	}

	if (msg == WM_TASKBAR_CREATED) {
		for (int i = 0; i < MAXTRAYITEMS; i++) {
			if (_hwndItems[i]) {
				AddToTray(i);
			}
		}
	}

	return DefWindowProc(hwnd, msg, wParam, lParam);
}

// AttachConsole exists only on Windows XP+, so resolve it at runtime: a
// static import would stop the loader on Windows 2000. SDKs targeting
// 2000 don't declare ATTACH_PARENT_PROCESS either, so fall back locally.
#ifndef ATTACH_PARENT_PROCESS
#define ATTACH_PARENT_PROCESS ((DWORD)-1)
#endif
typedef BOOL (WINAPI *AttachConsoleFunc)(DWORD dwProcessId);

static HWND GetConsoleHwndForTray() {
	HWND hwnd = GetConsoleWindow();
	AttachConsoleFunc pAttachConsole;
	if (hwnd) return hwnd;
	// RBTray is a Windows-subsystem exe, so it has no console of its own.
	// Attach to the parent console (the cmd window that launched us) to find it.
	pAttachConsole = (AttachConsoleFunc)GetProcAddress(GetModuleHandle(L"kernel32.dll"), "AttachConsole");
	if (pAttachConsole && pAttachConsole(ATTACH_PARENT_PROCESS)) {
		hwnd = GetConsoleWindow();
		FreeConsole();
	}
	return hwnd;
}

static BOOL ParseHwndArg(const char *s, HWND *out) {
	char buf[32];
	int len = 0;
	while (*s == ' ' || *s == '\t') s++;
	if (*s == '=' || *s == ':') {
		s++;
		while (*s == ' ' || *s == '\t') s++;
	}
	if (*s == '"') s++; // allow quoted value
	while (*s && *s != ' ' && *s != '\t' && *s != '"' && len < (int)sizeof(buf) - 1) {
		buf[len++] = *s++;
	}
	buf[len] = '\0';
	if (len == 0) return FALSE;
	char *end = NULL;
	int base = 10;
	if (buf[0] == '0' && (buf[1] == 'x' || buf[1] == 'X')) base = 16; // 0x... = hex, else decimal
	unsigned __int64 val = _strtoui64(buf, &end, base);
	if (end == buf || *end != '\0' || val == 0) return FALSE;
#ifndef _WIN64
	if (val > 0xFFFFFFFF) return FALSE;
#endif
	*out = (HWND)(UINT_PTR)val;
	return TRUE;
}

static BOOL IsCmdBoundary(char c) {
	return c == '\0' || c == ' ' || c == '\t' || c == '"';
}

// Collect --hwnd <val> (repeatable, "--hwnd=val" also ok) and --console targets.
// Returns count in out[], 0 if none present, -1 on error (message already shown).
static int CollectTrayTargets(LPSTR szCmdLine, HWND *out, int maxOut) {
	int n = 0;
	const char *p = szCmdLine;
	while ((p = strstr(p, "--hwnd")) != NULL) {
		if (p != szCmdLine && !IsCmdBoundary(p[-1])) { p += 6; continue; }
		HWND hwnd = NULL;
		if (!ParseHwndArg(p + 6, &hwnd) || !IsWindow(hwnd)) {
			MessageBox(NULL, L"Invalid window handle for --hwnd. Use decimal or 0x-hex of a valid window.", L"RBTray", MB_OK | MB_ICONERROR);
			return -1;
		}
		BOOL dup = FALSE;
		for (int i = 0; i < n; i++) if (out[i] == hwnd) { dup = TRUE; break; }
		if (!dup && n < maxOut) out[n++] = hwnd;
		p += 6;
	}
	p = szCmdLine;
	while ((p = strstr(p, "--console")) != NULL) {
		if (p != szCmdLine && !IsCmdBoundary(p[-1])) { p += 9; continue; }
		HWND hwnd = GetConsoleHwndForTray();
		if (!hwnd || !IsWindow(hwnd)) {
			MessageBox(NULL, L"No console window found for --console.", L"RBTray", MB_OK | MB_ICONERROR);
			return -1;
		}
		BOOL dup = FALSE;
		for (int i = 0; i < n; i++) if (out[i] == hwnd) { dup = TRUE; break; }
		if (!dup && n < maxOut) out[n++] = hwnd;
		p += 9;
		break; // one console window only; extra --console flags add nothing
	}
	return n;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR szCmdLine, int iCmdShow) {
	WNDCLASS wc;
	MSG msg;
	HWND pendingTargets[MAXTRAYITEMS];
	int pendingCount = 0;

	_hInstance = hInstance;
	_hwndHook = FindWindow(NAME, NAME);
	if (_hwndHook) {
		if (strstr(szCmdLine, "--exit")) {
			SendMessage(_hwndHook, WM_CLOSE, 0, 0);
			return 0;
		}
		int n = CollectTrayTargets(szCmdLine, pendingTargets, MAXTRAYITEMS);
		if (n < 0) return 0; // error already shown
		if (n == 0) {
			MessageBox(NULL, L"RBTray is already running.", L"RBTray", MB_OK | MB_ICONINFORMATION);
			return 0;
		}
		for (int i = 0; i < n; i++) {
			if (IsWindow(pendingTargets[i])) {
				PostMessage(_hwndHook, WM_ADDTRAY, 0, (LPARAM)pendingTargets[i]);
			}
		}
		return 0;
	}
	if (strstr(szCmdLine, "--exit")) {
		return 0; // nothing running, nothing to exit; don't start daemon just to quit it
	}
	{
		int n = CollectTrayTargets(szCmdLine, pendingTargets, MAXTRAYITEMS);
		if (n < 0) n = 0; // error already shown; still start daemon, tray nothing
		pendingCount = n;
	}
	if (!(_hLib = LoadLibrary(L"RBHook.dll"))) {
		MessageBox(NULL, L"Error loading RBHook.dll.", L"RBTray", MB_OK | MB_ICONERROR);
		return 0;
	}
	if (!RegisterHook(_hLib)) {
		MessageBox(NULL, L"Error setting hook procedure.", L"RBTray", MB_OK | MB_ICONERROR);
		return 0;
	}
	wc.style         = 0;
	wc.lpfnWndProc   = HookWndProc;
	wc.cbClsExtra    = 0;
	wc.cbWndExtra    = 0;
	wc.hInstance     = hInstance;
	wc.hIcon         = NULL;
	wc.hCursor       = NULL;
	wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wc.lpszMenuName  = NULL;
	wc.lpszClassName = NAME;
	if (!RegisterClass(&wc)) {
		MessageBox(NULL, L"Error creating window class", L"RBTray", MB_OK | MB_ICONERROR);
		return 0;
	}
	if (!(_hwndHook = CreateWindow(NAME, NAME, WS_OVERLAPPED, 0, 0, 0, 0, (HWND)NULL, (HMENU)NULL, (HINSTANCE)hInstance, (LPVOID)NULL))) {
		MessageBox(NULL, L"Error creating window", L"RBTray", MB_OK | MB_ICONERROR);
		return 0;
	}
	for (int i = 0; i < MAXTRAYITEMS; i++) {
		_hwndItems[i] = NULL;
	}
	for (int i = 0; i < pendingCount; i++) {
		if (IsWindow(pendingTargets[i])) {
			MinimizeWindowToTray(pendingTargets[i]);
		}
	}
	WM_TASKBAR_CREATED = RegisterWindowMessage(L"TaskbarCreated");

	while (IsWindow(_hwndHook) && GetMessage(&msg, _hwndHook, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	return 0;
}
