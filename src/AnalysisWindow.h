#pragma once

#include "GuiCommon.h"

bool RegisterAnalysisWindowClass(HINSTANCE hInstance);
void ShowAnalysisWindow(HWND parent, const std::wstring& report);
