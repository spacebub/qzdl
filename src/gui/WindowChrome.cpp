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
#include "gui/WindowChrome.h"

#include <QAbstractNativeEventFilter>
#include <QColor>
#include <QCoreApplication>
#include <QQuickWindow>

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

HWND handleOf(QQuickWindow *window) {
    return reinterpret_cast<HWND>(window->winId());
}

/*
A window covering a screen exactly is one Windows takes for a game or a film:
it holds an auto-hidden taskbar back rather than sliding it out over one, and
Qt reads such a window back as fullscreen from the same test. Keeping a pixel
off that edge takes two messages, a frameless window being maximized two ways.
*/
class Bounds final : public QObject, public QAbstractNativeEventFilter {
public:
    explicit Bounds(QQuickWindow *window)
        : QObject(window), _handle(handleOf(window)) {
        QCoreApplication::instance()->installNativeEventFilter(this);
    }

    ~Bounds() override {
        if (QCoreApplication *application = QCoreApplication::instance()) {
            application->removeNativeEventFilter(this);
        }
    }

    bool nativeEventFilter(const QByteArray &type, void *message, qintptr *) override {
        if (type != "windows_generic_MSG") {
            return false;
        }

        const auto *event = static_cast<const MSG *>(message);

        if (event->hwnd != _handle) {
            return false;
        }

        switch (event->message) {
            // Windows maximizing the window itself, from the keyboard or a
            // drag to the top edge. A popup, which is what a frameless window
            // is to Windows, is given the whole screen unless told otherwise.
            case WM_GETMINMAXINFO:
                limit(reinterpret_cast<MINMAXINFO *>(event->lParam));

                // Qt answers the same message with the interface's minimums.
                return false;

            // The button in the title bar never gets that far: Qt maximizes a
            // frameless window by moving it over the free part of the screen.
            case WM_WINDOWPOSCHANGING:
                settle(reinterpret_cast<WINDOWPOS *>(event->lParam));

                return false;

            default:
                return false;
        }
    }

private:
    void limit(MINMAXINFO *bounds) const {
        MONITORINFO screen = {sizeof(MONITORINFO), {}, {}, 0};

        if (!GetMonitorInfoW(MonitorFromWindow(_handle, MONITOR_DEFAULTTONEAREST), &screen)) {
            return;
        }

        // Asked for from the corner of the screen, not of the desktop.
        bounds->ptMaxPosition.x = screen.rcWork.left - screen.rcMonitor.left;
        bounds->ptMaxPosition.y = screen.rcWork.top - screen.rcMonitor.top;
        bounds->ptMaxSize.x = screen.rcWork.right - screen.rcWork.left;
        bounds->ptMaxSize.y = screen.rcWork.bottom - screen.rcWork.top;
    }

    // A move onto the whole of a screen is put a pixel short of it instead.
    void settle(WINDOWPOS *position) const {
        if (position->flags & SWP_NOSIZE) {
            return;
        }

        RECT going = {position->x, position->y,
                      position->x + position->cx, position->y + position->cy};

        if (position->flags & SWP_NOMOVE) {
            RECT here;

            if (!GetWindowRect(_handle, &here)) {
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

        // The corner was worked out above whether it was asked for or not.
        position->flags &= ~SWP_NOMOVE;
    }

    // An auto-hidden taskbar reserves nothing, so the free part of the screen
    // is all of it. The edge such a bar hides along is the one to leave, and
    // which edge of which screen that is has to be asked for per screen.
    static RECT spare(const RECT &monitor, RECT area) {
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

    HWND _handle;
};

}

void WindowChrome::apply(QQuickWindow *window) {
    const HWND handle = handleOf(window);
    const LONG_PTR style = GetWindowLongPtrW(handle, GWL_STYLE);

    // Qt gives a frameless window a bare popup style, and Windows snaps
    // nothing it does not consider sizeable and maximisable. The frame these
    // ask for is never drawn: Qt answers WM_NCCALCSIZE for one with none.
    SetWindowLongPtrW(handle, GWL_STYLE,
                      style | WS_THICKFRAME | WS_MAXIMIZEBOX | WS_MINIMIZEBOX | WS_SYSMENU);

    // A style change only reaches the window through a frame change.
    SetWindowPos(handle, nullptr, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);

    // Cut by the compositor, so the window keeps its shadow and the radius the
    // rest of the desktop is rounded to. A maximized one is squared off again.
    constexpr DWORD rounded = ROUNDED;

    DwmSetWindowAttribute(handle, CORNER, &rounded, sizeof(rounded));

    new Bounds(window);
}

void WindowChrome::outline(QQuickWindow *window, const QColor &edge) {
    // 0x00bbggrr, which is the way round Qt never hands out.
    const COLORREF colour = RGB(edge.red(), edge.green(), edge.blue());

    DwmSetWindowAttribute(handleOf(window), BORDER, &colour, sizeof(colour));
}
