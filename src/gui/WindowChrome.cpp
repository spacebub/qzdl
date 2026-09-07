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

#include "slint/WindowChrome.h"

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

// The row of frame the compositor is left, without which it rounds, outlines
// and shadows nothing. The same row the backend keeps for a window it is asked
// to shadow itself, which Slint never asks of it.
constexpr LONG EDGE = 1;

// Turns of the event loop the window is waited for before it is given up on.
constexpr int PATIENCE = 8;

slint::Window *owner = nullptr;
HWND handle = nullptr;
WNDPROC before = nullptr;

// Asked for before there was a window to ask it of, so kept until there is.
COLORREF wanted = 0;
bool asked = false;

// A drag handed to Windows, whose release Windows keeps.
bool dragging = false;

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

// Where the pointer is, in the window's own coordinates, packed the way a mouse
// message carries it.
LPARAM pointer() {
    POINT at = {0, 0};

    GetCursorPos(&at);
    ScreenToClient(handle, &at);

    return MAKELPARAM(at.x, at.y);
}

// Windows keeps the release that ends a drag it ran, so the backend never learns
// the button went up. To Slint the bar is then still held: whatever is pressed
// keeps the pointer until it is released, so every later press anywhere in the
// window goes to the bar instead, and starts a drag of its own. The backend
// replays the release after the drags it starts itself, and this replays it
// after ours, the position first so the release lands where the pointer is
// rather than at the origin the backend's own drag hack last reported.
void endDrag() {
    if (!dragging) {
        return;
    }

    dragging = false;

    const LPARAM at = pointer();

    PostMessageW(handle, WM_MOUSEMOVE, 0, at);
    PostMessageW(handle, WM_LBUTTONUP, 0, at);
}

// Answered ahead of the backend, which is what the subclass is for: the pixel
// that keeps an auto-hidden taskbar sliding out over a window sized to the whole
// screen, the end of a drag, and the row of frame the compositor is left.
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

        case WM_EXITSIZEMOVE:
            endDrag();

            break;

        // A release the window sees for itself was never taken by a drag.
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

    // The backend answers this with the client area over the whole window: no
    // frame, so nothing for the compositor to round off, outline or shadow. One
    // row is kept back, enough for all three and too little to see, by moving
    // the client area down rather than shortening it, so nothing in it shifts.
    // A maximized window has that row off the screen and is left as answered.
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

    // The backend already gave the window every style a snap needs and keeps
    // them, so none are asked for here. The frame is settled in an answer to
    // WM_NCCALCSIZE, which is asked again now that the subclass is there to
    // amend it.
    SetWindowPos(handle, nullptr, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);

    // Cut by the compositor, so the window keeps its shadow and the desktop's radius.
    constexpr DWORD rounded = ROUNDED;

    DwmSetWindowAttribute(handle, CORNER, &rounded, sizeof(rounded));

    if (asked) {
        DwmSetWindowAttribute(handle, BORDER, &wanted, sizeof(wanted));
    }
}

// The window is asked for from inside the loop, where it exists, and asked for
// again on a later turn if it does not yet.
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
    // 0x00bbggrr, the way round nobody hands it out.
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

    // The backend took the pointer on the press, and Windows will not take it
    // from it.
    ReleaseCapture();

    // What is said is that the caption was pressed, that being the one press
    // Windows docks against an edge; a plain system move is the keyboard's, and
    // offers none. It is posted rather than sent: this is reached from the
    // press, inside the loop's own handler, and the drag Windows runs would run
    // in there too, with the loop unable to answer the repaints the drag causes.
    // From the queue it runs with nothing beneath it, which is also how the
    // backend starts the drags of its own.
    PostMessageW(handle, WM_NCLBUTTONDOWN, HTCAPTION, MAKELPARAM(at.x, at.y));

    return true;
}
