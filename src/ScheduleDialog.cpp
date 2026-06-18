#include "ScheduleDialog.h"

#include <dwmapi.h>

static LRESULT CALLBACK ScheduleWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    ScheduleDialogState* state = reinterpret_cast<ScheduleDialogState*>(GetWindowLongPtrW(hWnd, GWLP_USERDATA));

    switch (message) {
        case WM_CREATE: {
            state = reinterpret_cast<ScheduleDialogState*>(
                reinterpret_cast<LPCREATESTRUCTW>(lParam)->lpCreateParams);
            SetWindowLongPtrW(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));

            CreateWindowExW(0, L"STATIC", L"Profile:", WS_CHILD | WS_VISIBLE, 15, 15, 60, 20, hWnd, NULL, NULL, NULL);
            CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", state->profilePath.c_str(),
                            WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | ES_READONLY,
                            80, 12, 250, 24, hWnd, NULL, NULL, NULL);

            CreateWindowExW(0, L"STATIC", L"Run:", WS_CHILD | WS_VISIBLE, 15, 50, 40, 20, hWnd, NULL, NULL, NULL);
            HWND hwndType = CreateWindowExW(0, L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST,
                                              60, 46, 120, 120, hWnd, (HMENU)ID_SCHEDULE_TYPE_COMBO, NULL, NULL);
            SendMessageW(hwndType, CB_ADDSTRING, 0, (LPARAM)L"Daily");
            SendMessageW(hwndType, CB_ADDSTRING, 0, (LPARAM)L"Weekly");
            SendMessageW(hwndType, CB_SETCURSEL, 0, 0);

            CreateWindowExW(0, L"STATIC", L"Day:", WS_CHILD | WS_VISIBLE, 190, 50, 35, 20, hWnd, NULL, NULL, NULL);
            HWND hwndDay = CreateWindowExW(0, L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST,
                                           225, 46, 100, 160, hWnd, (HMENU)ID_SCHEDULE_DAY_COMBO, NULL, NULL);
            const wchar_t* days[] = { L"MON", L"TUE", L"WED", L"THU", L"FRI", L"SAT", L"SUN" };
            for (const wchar_t* day : days) {
                SendMessageW(hwndDay, CB_ADDSTRING, 0, (LPARAM)day);
            }
            SendMessageW(hwndDay, CB_SETCURSEL, 0, 0);

            CreateWindowExW(0, L"STATIC", L"Time (HH:MM):", WS_CHILD | WS_VISIBLE, 15, 85, 100, 20, hWnd, NULL, NULL, NULL);
            CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"02:00",
                            WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                            120, 82, 80, 24, hWnd, (HMENU)ID_SCHEDULE_TIME_EDIT, NULL, NULL);

            CreateWindowExW(0, L"BUTTON", L"Create Task", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
                            70, 125, 100, 30, hWnd, (HMENU)ID_SCHEDULE_OK, NULL, NULL);
            CreateWindowExW(0, L"BUTTON", L"Cancel", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                            185, 125, 100, 30, hWnd, (HMENU)ID_SCHEDULE_CANCEL, NULL, NULL);
            break;
        }
        case WM_COMMAND: {
            if (!state) {
                break;
            }
            int wmId = LOWORD(wParam);
            if (wmId == ID_SCHEDULE_OK) {
                HWND hwndType = GetDlgItem(hWnd, ID_SCHEDULE_TYPE_COMBO);
                HWND hwndDay = GetDlgItem(hWnd, ID_SCHEDULE_DAY_COMBO);
                HWND hwndTime = GetDlgItem(hWnd, ID_SCHEDULE_TIME_EDIT);
                state->weekly = (SendMessageW(hwndType, CB_GETCURSEL, 0, 0) == 1);

                wchar_t dayBuf[8] = L"MON";
                SendMessageW(hwndDay, CB_GETLBTEXT, SendMessageW(hwndDay, CB_GETCURSEL, 0, 0), (LPARAM)dayBuf);
                state->dayName = dayBuf;

                wchar_t timeBuf[16] = L"02:00";
                GetWindowTextW(hwndTime, timeBuf, 16);
                std::wstring timeText = timeBuf;
                size_t colon = timeText.find(L':');
                if (colon != std::wstring::npos) {
                    state->hour = _wtoi(timeText.substr(0, colon).c_str());
                    state->minute = _wtoi(timeText.substr(colon + 1).c_str());
                }

                state->confirmed = true;
                DestroyWindow(hWnd);
            } else if (wmId == ID_SCHEDULE_CANCEL) {
                DestroyWindow(hWnd);
            }
            break;
        }
        case WM_CLOSE:
            DestroyWindow(hWnd);
            break;
        case WM_DESTROY:
            break;
        default:
            return DefWindowProcW(hWnd, message, wParam, lParam);
    }
    return 0;
}

bool RunScheduleDialog(HWND parent, ScheduleDialogState& state) {
    static bool classRegistered = false;
    if (!classRegistered) {
        WNDCLASSEXW wc = { sizeof(WNDCLASSEXW) };
        wc.lpfnWndProc = ScheduleWndProc;
        wc.hInstance = GetModuleHandleW(NULL);
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        wc.hbrBackground = g_hbrBackground ? g_hbrBackground : reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
        wc.lpszClassName = L"ChronoSyncScheduleWindow";
        RegisterClassExW(&wc);
        classRegistered = true;
    }

    EnableWindow(parent, FALSE);
    HWND hwndDlg = CreateWindowExW(
        WS_EX_DLGMODALFRAME,
        L"ChronoSyncScheduleWindow",
        L"Schedule Sync",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
        CW_USEDEFAULT, CW_USEDEFAULT, 360, 220,
        parent, NULL, GetModuleHandleW(NULL), &state);

    if (!hwndDlg) {
        EnableWindow(parent, TRUE);
        return false;
    }

    BOOL useDarkMode = TRUE;
    DwmSetWindowAttribute(hwndDlg, 19, &useDarkMode, sizeof(useDarkMode));
    DwmSetWindowAttribute(hwndDlg, 20, &useDarkMode, sizeof(useDarkMode));
    ShowWindow(hwndDlg, SW_SHOW);

    MSG msg;
    while (IsWindow(hwndDlg) && GetMessageW(&msg, NULL, 0, 0)) {
        if (!IsDialogMessageW(hwndDlg, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    EnableWindow(parent, TRUE);
    SetForegroundWindow(parent);
    return state.confirmed;
}
