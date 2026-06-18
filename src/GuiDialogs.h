#pragma once

#include "GuiCommon.h"

std::wstring FormatBytes(unsigned long long bytes);
std::string WideToUTF8(const std::wstring& wstr);
std::wstring EscapeCSV(const std::wstring& field);

std::wstring SaveCSVDialog(HWND hWndParent, const wchar_t* title);
std::wstring OpenProfileDialog(HWND hWndParent);
std::wstring SaveProfileDialog(HWND hWndParent);
std::wstring OpenQueueDialog(HWND hWndParent);
std::wstring SaveQueueDialog(HWND hWndParent);
std::wstring BrowseForFolder(HWND hWndParent, const wchar_t* title);

bool RevealInExplorer(HWND owner, const std::wstring& fullPath);
