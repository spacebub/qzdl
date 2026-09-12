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
#pragma once

#include <functional>
#include <map>
#include <mutex>
#include <vector>

#include <blend2d/blend2d.h>

#include "gui/draw/Surface.h"
#include "gui/draw/Typeface.h"
#include "gui/toolkit/Root.h"

struct SDL_Cursor;
struct SDL_Window;
union SDL_Event;

// The window and the frame loop: everything between the desktop and the widgets.
//
// Damage decides what is painted and whether anything is painted at all. With no
// tween in flight, no timer due and nothing dirty, the loop blocks in
// SDL_WaitEventTimeout and the process costs nothing.
class Shell {
public:
    Shell();
    ~Shell();

    Shell(const Shell &) = delete;
    Shell &operator=(const Shell &) = delete;
    Shell(Shell &&) = delete;
    Shell &operator=(Shell &&) = delete;

    // False when the platform gives no window, no surface or no font, which the
    // caller turns into a clean exit rather than a crash.
    bool start(int width, int height);

    void run();

    void stop() { _running = false; }

    toolkit::Root &ui() { return *_root; }

    Typeface &type() { return _type; }

    [[nodiscard]] SDL_Window *window() const { return _window; }

    void minimize();
    void toggleMaximize();

    [[nodiscard]] bool maximized() const { return _maximized; }

    static void setOutline(BLRgba32 edge);

    // Where the window manager may take a press and drag the window.
    std::function<bool(double, double)> draggable;

    // The desktop asked for the window to close.
    std::function<void()> closing;

    std::function<void(double, double)> resized;

    // Mouse side buttons.
    std::function<void()> back;
    std::function<void()> forward;

    // Before anything else sees a key, for the shortcuts the whole window owns.
    std::function<bool(const toolkit::Key &)> shortcut;

    // Once per turn of the loop, after the events and before anything is drawn.
    std::function<void()> settle;

    // Called when the desktop's light or dark preference changes.
    std::function<void()> shadeChanged;

    // Runs `what` every `seconds` until cancelled. Keeps the loop awake.
    int every(double seconds, std::function<void()> what);

    // Runs `what` once, `seconds` from now.
    int after(double seconds, std::function<void()> what);

    void cancel(int id);

    // Runs `what` on the interface thread, from any thread, and wakes the loop.
    void post(std::function<void()> what);

    // Puts the pointer the widget under it asks for on the desktop.
    void setCursor(toolkit::Cursor wanted);

    // Where the window is and how big, in logical pixels.
    void geometry(int &x, int &y, int &width, int &height) const;
    void setGeometry(int x, int y, int width, int height);

    [[nodiscard]] static double now();

private:
    struct Alarm {
        int id = 0;
        double due = 0.0;
        double every = 0.0;
        std::function<void()> what;
    };

    void handle(const SDL_Event &event);
    void draw();
    void relayout();

    bool alarms(double at);

    void errands();

    // Milliseconds to block for before the next alarm is due.
    [[nodiscard]] int sleepFor(double at) const;

    SDL_Window *_window = nullptr;

    Surface _surface;
    Typeface _type;

    std::unique_ptr<toolkit::Root> _root;

    std::vector<Alarm> _alarms;
    int _nextAlarm = 1;

    // One of each, made on demand and kept for the life of the window.
    std::map<toolkit::Cursor, SDL_Cursor *> _cursors;
    toolkit::Cursor _cursor = toolkit::Cursor::Default;

    // Work handed over from another thread.
    std::mutex _posted;
    std::vector<std::function<void()>> _errands;
    unsigned _wakeEvent = 0;

    double _width = 0.0;
    double _height = 0.0;

    bool _running = true;
    bool _maximized = false;
    bool _ready = false;

    friend struct ShellHooks;
};
