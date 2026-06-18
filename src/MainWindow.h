#pragma once

#define NOMINMAX
#include <windows.h>

bool RegisterMainWindowClass(HINSTANCE hInstance);
LRESULT CALLBACK MainWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
