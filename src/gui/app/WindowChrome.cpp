/*
 * This file is part of qZDL
 * Copyright (C) 2026  spacebub
 *
 * qZDL is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <initializer_list>

#include <slint.h>

#include "gui/app/WindowChrome.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <dwmapi.h>
#include <shellapi.h>

namespace {

// Spelled out so an older SDK still builds; an older Windows ignores them.
constexpr DWORD CORNER = 33; // DWMWA_WINDOW_CORNER_PREFERENCE
constexpr DWORD BORDER = 34; // DWMWA_BORDER_COLOR
constexpr DWORD ROUNDED = 2; // DWMWCP_ROUND

constexpr LONG STRIP = 1;

// The row of frame the compositor needs to round, outline and shadow.
constexpr LONG EDGE = 1;

// Event loop turns to wait for the window.
constexpr int PATIENCE = 8;

slint::Window *owner = nullptr;
HWND handle = nullptr;
WNDPROC before = nullptr;

// Kept until there is a window to apply it to.
COLORREF wanted = 0;
bool asked = false;

// Windows keeps the release that ends a drag it ran.
bool dragging = false;

// An auto-hidden taskbar reserves nothing, so its edge is left free.
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

// A frameless window is a popup to Windows and would maximize over the taskbar.
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

// A window sized to the whole screen is kept a pixel short, for the auto-hidden taskbar.
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

LPARAM pointer() {
    POINT at = {0, 0};

    GetCursorPos(&at);
    ScreenToClient(handle, &at);

    return MAKELPARAM(at.x, at.y);
}

// Windows swallows the release that ends its drag, so Slint thinks the bar is still
// held. Replay it, position first so it lands under the pointer.
void endDrag() {
    if (!dragging) {
        return;
    }

    dragging = false;

    const LPARAM at = pointer();

    PostMessageW(handle, WM_MOUSEMOVE, 0, at);
    PostMessageW(handle, WM_LBUTTONUP, 0, at);
}

LRESULT CALLBACK chrome(HWND window, const UINT message, const WPARAM sent, const LPARAM data) {
    switch (message) {
        case WM_GETMINMAXINFO:
            limit(reinterpret_cast<MINMAXINFO *>(data));

            break;

        case WM_WINDOWPOSCHANGING:
            settle(reinterpret_cast<WINDOWPOS *>(data));

            break;

        case WM_EXITSIZEMOVE:
            endDrag();

            break;

        // A release the window sees itself was not taken by a drag.
        case WM_LBUTTONUP:
            dragging = false;

            break;

        case WM_NCDESTROY:
            SetWindowLongPtrW(window, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(before));
            handle = nullptr;

            return CallWindowProcW(before, window, message, sent, data);

        default:
            break;
    }

    const LRESULT answer = CallWindowProcW(before, window, message, sent, data);

    // The backend answers with no frame at all. One row is kept back by shifting the
    // client area down, so nothing in it moves. Maximized, that row is off screen.
    if (message == WM_NCCALCSIZE && sent != 0 && !IsZoomed(window)) {
        auto *frame = reinterpret_cast<NCCALCSIZE_PARAMS *>(data);

        frame->rgrc[0].top += EDGE;
        frame->rgrc[0].bottom += EDGE;
    }

    return answer;
}

void dress() {
    before = reinterpret_cast<WNDPROC>(
        SetWindowLongPtrW(handle, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(chrome)));

    // Re-asks WM_NCCALCSIZE now that the subclass can amend it.
    SetWindowPos(handle, nullptr, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);

    constexpr DWORD rounded = ROUNDED;

    DwmSetWindowAttribute(handle, CORNER, &rounded, sizeof(rounded));

    if (asked) {
        DwmSetWindowAttribute(handle, BORDER, &wanted, sizeof(wanted));
    }
}

void takeOver(const int turns) {
    slint::invoke_from_event_loop([turns] {
        if (owner == nullptr || handle != nullptr) {
            return;
        }

        handle = owner->win32_hwnd();

        if (handle != nullptr) {
            dress();
        } else if (turns > 0) {
            takeOver(turns - 1);
        }
    });
}

}

void WindowChrome::apply(slint::Window &window) {
    owner = &window;

    takeOver(PATIENCE);
}

void WindowChrome::outline(const uint8_t red, const uint8_t green, const uint8_t blue) {
    // COLORREF is 0x00bbggrr.
    wanted = RGB(red, green, blue);
    asked = true;

    if (handle == nullptr) {
        return;
    }

    DwmSetWindowAttribute(handle, BORDER, &wanted, sizeof(wanted));
}

bool WindowChrome::beginMove() {
    if (handle == nullptr || dragging) {
        return false;
    }

    POINT at;

    if (!GetCursorPos(&at)) {
        return false;
    }

    dragging = true;

    // The backend holds the capture from the press.
    ReleaseCapture();

    // HTCAPTION is the one press Windows snaps. Posted, not sent: run from inside
    // the press handler the drag would block the loop's repaints.
    PostMessageW(handle, WM_NCLBUTTONDOWN, HTCAPTION, MAKELPARAM(at.x, at.y));

    return true;
}
