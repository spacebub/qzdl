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
#include "gui/WindowSnap.h"

#include <QAbstractNativeEventFilter>
#include <QGuiApplication>
#include <QPointer>
#include <QQuickItem>
#include <QQuickWindow>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <windowsx.h>

namespace {

// The button in the title bar that Windows is told about.
constexpr auto MAXIMIZE = "maximize";

/*
Windows offers the snap layouts to whichever button answers WM_NCHITTEST with
HTMAXBUTTON, and from then on keeps every pointer message over that button to
itself, so the hover and the press it no longer sees are handed back here.
*/
class Chrome final : public QObject, public QAbstractNativeEventFilter {
public:
    explicit Chrome(QQuickWindow *window)
        : QObject(window),
          _window(window),
          _handle(reinterpret_cast<HWND>(window->winId())),
          _button(window->findChild<QQuickItem *>(QLatin1StringView(MAXIMIZE))) {
        if (!_button) {
            qWarning("No \"%s\" in the title bar, so there are no snap layouts", MAXIMIZE);
        }

        QGuiApplication::instance()->installNativeEventFilter(this);
    }

    ~Chrome() override {
        if (QCoreApplication *application = QCoreApplication::instance()) {
            application->removeNativeEventFilter(this);
        }
    }

    bool nativeEventFilter(const QByteArray &type, void *message, qintptr *result) override {
        if (type != "windows_generic_MSG") {
            return false;
        }

        const auto *event = static_cast<const MSG *>(message);

        if (event->hwnd != _handle) {
            return false;
        }

        switch (event->message) {
            case WM_NCHITTEST: {
                if (!over(event->lParam)) {
                    light(false);

                    return false;
                }

                light(true);

                *result = HTMAXBUTTON;

                return true;
            }

            // Only sent while the tracking light() asked for is still armed.
            case WM_NCMOUSELEAVE:
                light(false);

                return false;

            case WM_NCLBUTTONDOWN:
            case WM_NCLBUTTONDBLCLK:
                if (event->wParam != HTMAXBUTTON) {
                    return false;
                }

                _pressed = true;
                *result = 0;

                return true;

            case WM_NCLBUTTONUP:
                if (event->wParam != HTMAXBUTTON || !_pressed || !_button) {
                    return false;
                }

                _pressed = false;

                QMetaObject::invokeMethod(_button, "clicked");

                *result = 0;

                return true;

            default:
                return false;
        }
    }

private:
    // The point comes in screen pixels, the button knows itself in the ones the
    // interface is laid out in.
    [[nodiscard]] bool over(LPARAM position) const {
        if (!_button) {
            return false;
        }

        POINT point = {GET_X_LPARAM(position), GET_Y_LPARAM(position)};

        ScreenToClient(_handle, &point);

        const qreal ratio = _window->devicePixelRatio();
        const QRectF area(_button->mapToScene(QPointF(0, 0)), _button->size());

        return area.contains(QPointF(point.x / ratio, point.y / ratio));
    }

    // A pointer that leaves the button through the edge of the window is never
    // hit tested again, so the way back out is asked for at the same time.
    void light(bool on) {
        if (!_button || _lit == on) {
            return;
        }

        _lit = on;

        _button->setProperty("systemHover", on);

        if (!on) {
            // Whatever was pressed on the way out was not released on it.
            _pressed = false;

            return;
        }

        TRACKMOUSEEVENT tracking = {static_cast<DWORD>(sizeof(TRACKMOUSEEVENT)),
                                    TME_LEAVE | TME_NONCLIENT, _handle, 0};

        TrackMouseEvent(&tracking);
    }

    QQuickWindow *_window;
    HWND _handle;
    QPointer<QQuickItem> _button;
    bool _lit{false};
    bool _pressed{false};
};

}

void WindowSnap::enable(QQuickWindow *window) {
    const auto handle = reinterpret_cast<HWND>(window->winId());
    const LONG_PTR style = GetWindowLongPtrW(handle, GWL_STYLE);

    /*
    Qt gives a frameless window a bare popup style, and Windows snaps nothing
    it does not consider sizeable and maximisable. The frame those bits ask for
    is never drawn: Qt answers WM_NCCALCSIZE for a frameless window with none.
    */
    SetWindowLongPtrW(handle, GWL_STYLE,
                      style | WS_THICKFRAME | WS_MAXIMIZEBOX | WS_MINIMIZEBOX | WS_SYSMENU);

    // A style change only reaches the window through a frame change.
    SetWindowPos(handle, nullptr, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);

    // Kept by the window it belongs to.
    new Chrome(window);
}
