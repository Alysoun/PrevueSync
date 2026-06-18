#pragma once

#include "GuiCommon.h"
#include <string>

struct ScheduleDialogState {
    std::wstring profilePath;
    bool weekly = false;
    std::wstring dayName = L"MON";
    int hour = 2;
    int minute = 0;
    bool confirmed = false;
};

bool RunScheduleDialog(HWND parent, ScheduleDialogState& state);
