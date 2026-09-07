/*
 * This file is part of qZDL
 * Copyright (C) 2026  spacebub
 *
 * qZDL is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "slint-gui/WindowChrome.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <dwmapi.h>
#include <shellapi.h>

namespace {

// Written out rather than taken from dwmapi.h so an older SDK still builds.
// An older Windows turns both down and keeps its square corner and border.
constexpr DWORD CORNER = 33; // DWMWA_WINDOW_CORNER_PREFERENCE
constexpr DWORD BORDER = 34; // DWMWA_BORDER_COLOR
constexpr DWORD ROUNDED = 2; // DWMWCP_ROUND

constexpr LONG STRIP = 1;

HWND handle = nullptr;
WNDPROC before = nullptr;

// Slint hands no window handle to C++, so this thread's one top level window
// is found instead.
BOOL CALLBACK pick(const HWND window, LPARAM into) {
    if (GetWindow(window, GW_OWNER) == nullptr && IsWindowVisible(window)) {
        *reinterpret_cast<HWND *>(into) = window;

        return FALSE;
    }

    return TRUE;
}

HWND findWindow() {
    HWND found = nullptr;

    EnumThreadWindows(GetCurrentThreadId(), pick, reinterpret_cast<LPARAM>(&found));

    return found;
}

// An auto-hidden taskbar reserves nothing, so the edge it hides along has to be
// asked for per screen and left free.
RECT spare(const RECT &monitor, RECT area) {
    APPBARDATA taskbar = {sizeof(APPBARDATA), nullptr, 0, 0, {}, 0};

    if (!(SHAppBarMessage(ABM_GETSTATE, &taskbar) & ABS_AUTOHIDE)) {
        return area;
    }

    for (const UINT edge : {ABE_LEFT, ABE_TOP, ABE_RIGHT, ABE_BOTTOM}) {
        APPBARDATA along = {sizeof(APPBARDATA), nullptr, 0, edge, monitor, 0};

        if (!SHAppBarMessage(ABM_GETAUTOHIDEBAREX, &along)) {
            continue;
        }

        switch (edge) {
            case ABE_LEFT:   area.left += STRIP;   break;
            case ABE_TOP:    area.top += STRIP;    break;
            case ABE_RIGHT:  area.right -= STRIP;  break;
            default:         area.bottom -= STRIP; break;
        }
    }

    return area;
}

// Windows maximizing the window itself. A popup, which is what a frameless window
// is to it, gets the whole screen unless told otherwise.
void limit(MINMAXINFO *bounds) {
    MONITORINFO screen = {sizeof(MONITORINFO), {}, {}, 0};

    if (!GetMonitorInfoW(MonitorFromWindow(handle, MONITOR_DEFAULTTONEAREST), &screen)) {
        return;
    }

    // From the corner of the screen, not of the desktop.
    bounds->ptMaxPosition.x = screen.rcWork.left - screen.rcMonitor.left;
    bounds->ptMaxPosition.y = screen.rcWork.top - screen.rcMonitor.top;
    bounds->ptMaxSize.x = screen.rcWork.right - screen.rcWork.left;
    bounds->ptMaxSize.y = screen.rcWork.bottom - screen.rcWork.top;
}

// A move onto the whole of a screen is put a pixel short of it instead.
void settle(WINDOWPOS *position) {
    if (position->flags & SWP_NOSIZE) {
        return;
    }

    RECT going = {position->x, position->y,
                  position->x + position->cx, position->y + position->cy};

    if (position->flags & SWP_NOMOVE) {
        RECT here;

        if (!GetWindowRect(handle, &here)) {
            return;
        }

        going = {here.left, here.top, here.left + position->cx, here.top + position->cy};
    }

    MONITORINFO screen = {sizeof(MONITORINFO), {}, {}, 0};

    if (!GetMonitorInfoW(MonitorFromRect(&going, MONITOR_DEFAULTTONEAREST), &screen)) {
        return;
    }

    const RECT &monitor = screen.rcMonitor;

    if (going.left > monitor.left || going.top > monitor.top
        || going.right < monitor.right || going.bottom < monitor.bottom) {
        return;
    }

    const RECT room = spare(monitor, going);

    if (room.left == going.left && room.top == going.top
        && room.right == going.right && room.bottom == going.bottom) {
        return;
    }

    position->x = room.left;
    position->y = room.top;
    position->cx = room.right - room.left;
    position->cy = room.bottom - room.top;

    position->flags &= ~SWP_NOMOVE;
}

// A window covering a screen exactly is taken for a game, and an auto-hidden
// taskbar will not slide out over it. Keeping a pixel off that edge takes both
// messages below, answered before the window sees them: hence the subclass.
LRESULT CALLBACK chrome(HWND window, const UINT message, const WPARAM sent, const LPARAM data) {
    switch (message) {
        case WM_GETMINMAXINFO:
            limit(reinterpret_cast<MINMAXINFO *>(data));

            break;

        // The title bar's own button never gets this far: it maximizes by moving
        // the window over the free part of the screen.
        case WM_WINDOWPOSCHANGING:
            settle(reinterpret_cast<WINDOWPOS *>(data));

            break;

        default:
            break;
    }

    return CallWindowProcW(before, window, message, sent, data);
}

}

void WindowChrome::apply() {
    handle = findWindow();

    if (handle == nullptr) {
        return;
    }

    const LONG_PTR style = GetWindowLongPtrW(handle, GWL_STYLE);

    // Windows snaps nothing it does not consider sizeable and maximisable. The frame
    // those bits ask for is never drawn: WM_NCCALCSIZE is answered with none.
    SetWindowLongPtrW(handle, GWL_STYLE,
                      style | WS_THICKFRAME | WS_MAXIMIZEBOX | WS_MINIMIZEBOX | WS_SYSMENU);

    // A style change only reaches the window through a frame change.
    SetWindowPos(handle, nullptr, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);

    // Cut by the compositor, so the window keeps its shadow and the desktop's radius.
    constexpr DWORD rounded = ROUNDED;

    DwmSetWindowAttribute(handle, CORNER, &rounded, sizeof(rounded));

    before = reinterpret_cast<WNDPROC>(
        SetWindowLongPtrW(handle, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(chrome)));
}

void WindowChrome::outline(const uint8_t red, const uint8_t green, const uint8_t blue) {
    if (handle == nullptr) {
        return;
    }

    // 0x00bbggrr, the way round nobody hands it out.
    const COLORREF colour = RGB(red, green, blue);

    DwmSetWindowAttribute(handle, BORDER, &colour, sizeof(colour));
}

bool WindowChrome::beginMove() {
    if (handle == nullptr) {
        return false;
    }

    // Handed to the window manager, which is what makes a drag snap.
    ReleaseCapture();
    SendMessageW(handle, WM_NCLBUTTONDOWN, HTCAPTION, 0);

    return true;
}
