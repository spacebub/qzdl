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

/*
 * Ported from qZDL's src/gui/app/WindowChrome.cpp (GPL-3.0-only, (C) 2026 spacebub).
 *
 * What changed against the original:
 *   - the handle comes straight off SDL, so the retry loop waiting for the window
 *     to exist is gone;
 *   - beginMove(), endDrag() and the dragging flag are gone with it: SDL's hit test
 *     answers WM_NCHITTEST with HTCAPTION, so the press never reaches the client
 *     area and there is no swallowed release to replay.
 * Everything else is the original logic.
 */

#include <initializer_list>

#include <SDL3/SDL.h>

#include "gui/draw/Chrome.h"

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

HWND handle = nullptr;
WNDPROC before = nullptr;
void (*resizing)(bool) = nullptr;

// Kept until there is a window to apply it to.
COLORREF wanted = 0;
bool asked = false;

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

LRESULT CALLBACK chrome(HWND window, const UINT message, const WPARAM sent, const LPARAM data) {
    switch (message) {
        case WM_ENTERSIZEMOVE:
        case WM_EXITSIZEMOVE:
            if (resizing != nullptr) {
                resizing(message == WM_ENTERSIZEMOVE);
            }

            break;

        case WM_GETMINMAXINFO:
            limit(reinterpret_cast<MINMAXINFO *>(data));

            break;

        case WM_WINDOWPOSCHANGING:
            settle(reinterpret_cast<WINDOWPOS *>(data));

            break;

        case WM_NCDESTROY:
            SetWindowLongPtrW(window, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(before));
            handle = nullptr;

            return CallWindowProcW(before, window, message, sent, data);

        default:
            break;
    }

    const LRESULT answer = CallWindowProcW(before, window, message, sent, data);

    // SDL answers with no frame at all. One row is kept back by shifting the client
    // area down, so nothing in it moves. Maximized, that row is off screen.
    if (message == WM_NCCALCSIZE && sent != 0 && !IsZoomed(window)) {
        auto *frame = reinterpret_cast<NCCALCSIZE_PARAMS *>(data);

        frame->rgrc[0].top += EDGE;
        frame->rgrc[0].bottom += EDGE;
    }

    return answer;
}

}

void Chrome::apply(SDL_Window *window) {
    handle = static_cast<HWND>(SDL_GetPointerProperty(SDL_GetWindowProperties(window),
                                                      SDL_PROP_WINDOW_WIN32_HWND_POINTER,
                                                      nullptr));

    if (handle == nullptr) {
        return;
    }

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

void Chrome::outline(const std::uint8_t red, const std::uint8_t green, const std::uint8_t blue) {
    // COLORREF is 0x00bbggrr.
    wanted = RGB(red, green, blue);
    asked = true;

    if (handle == nullptr) {
        return;
    }

    DwmSetWindowAttribute(handle, BORDER, &wanted, sizeof(wanted));
}

void Chrome::whileResizing(void (*told)(bool)) {
    resizing = told;
}
