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

#include <algorithm>
#include <memory>
#include <mutex>

#include <SDL3/SDL.h>

#include "gui/app/Shell.h"
#include "gui/draw/Chrome.h"
#include "gui/draw/Theme.h"

// SDL's callbacks want plain function pointers with its own signatures, and both
// reach into the Shell they were handed.
struct ShellHooks {
    static SDL_HitTestResult SDLCALL hitTest(SDL_Window *window, const SDL_Point *at, void *held);
    static bool SDLCALL watch(void *held, SDL_Event *event);
};

namespace {

// How close to an edge a press starts a resize.
constexpr double EDGE = 6.0;

toolkit::Click buttonOf(const Uint8 which) {
    switch (which) {
        case SDL_BUTTON_MIDDLE:
            return toolkit::Click::Middle;

        case SDL_BUTTON_RIGHT:
            return toolkit::Click::Right;

        case SDL_BUTTON_X1:
            return toolkit::Click::Back;

        case SDL_BUTTON_X2:
            return toolkit::Click::Forward;

        default:
            return toolkit::Click::Left;
    }
}

}

Shell::Shell() = default;

Shell::~Shell() {
    for (const auto &[wanted, made] : _cursors) {
        SDL_DestroyCursor(made);
    }

    if (_window != nullptr) {
        _surface.detach();
        SDL_DestroyWindow(_window);
    }

    SDL_Quit();
}

double Shell::now() {
    return static_cast<double>(SDL_GetTicksNS()) / 1000000000.0;
}

bool Shell::start(const int width, const int height) {
    SDL_SetHint(SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH, "1");

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        return false;
    }

    // Only the Windows and X11 drivers have a framebuffer of their own; pinned to
    // it the window surface is plain memory and no graphics device is opened.
    if (const char *driver = SDL_GetCurrentVideoDriver();
        driver != nullptr
        && (SDL_strcmp(driver, "windows") == 0 || SDL_strcmp(driver, "x11") == 0)) {
        SDL_SetHint(SDL_HINT_FRAMEBUFFER_ACCELERATION, "0");
    }

    _window = SDL_CreateWindow("ZDL4", width, height,
                               SDL_WINDOW_RESIZABLE | SDL_WINDOW_BORDERLESS
                                   | SDL_WINDOW_HIDDEN);

    if (_window == nullptr) {
        return false;
    }

    SDL_SetWindowMinimumSize(_window, 720, 520);

    if (!_surface.attach(_window) || !_type.load()) {
        return false;
    }

    Theme::setSystemDark(SDL_GetSystemTheme() != SDL_SYSTEM_THEME_LIGHT);

    _root = std::make_unique<toolkit::Root>(_type);

    _root->composing = [this](const bool wanted) {
        if (wanted) {
            SDL_StartTextInput(_window);
        } else {
            SDL_StopTextInput(_window);
        }
    };

    Chrome::apply(_window);

    SDL_SetWindowHitTest(_window, ShellHooks::hitTest, this);

    _wakeEvent = SDL_RegisterEvents(1);
    _ready = true;

    return true;
}

SDL_HitTestResult SDLCALL ShellHooks::hitTest(SDL_Window *window, const SDL_Point *at,
                                             void *held) {
    auto *shell = static_cast<Shell *>(held);

    int width = 0;
    int height = 0;

    SDL_GetWindowSize(window, &width, &height);

    if (!shell->_maximized) {
        const bool left = at->x < EDGE;
        const bool right = at->x >= width - EDGE;
        const bool top = at->y < EDGE;
        const bool bottom = at->y >= height - EDGE;

        if (top && left) { return SDL_HITTEST_RESIZE_TOPLEFT;
}
        if (top && right) { return SDL_HITTEST_RESIZE_TOPRIGHT;
}
        if (bottom && left) { return SDL_HITTEST_RESIZE_BOTTOMLEFT;
}
        if (bottom && right) { return SDL_HITTEST_RESIZE_BOTTOMRIGHT;
}
        if (left) { return SDL_HITTEST_RESIZE_LEFT;
}
        if (right) { return SDL_HITTEST_RESIZE_RIGHT;
}
        if (top) { return SDL_HITTEST_RESIZE_TOP;
}
        if (bottom) { return SDL_HITTEST_RESIZE_BOTTOM;
}
    }

    if (shell->_root != nullptr && shell->_root->grabbed() != nullptr) {
        return SDL_HITTEST_NORMAL;
    }

    if (shell->draggable && shell->draggable(at->x, at->y)) {
        return SDL_HITTEST_DRAGGABLE;
    }

    return SDL_HITTEST_NORMAL;
}

void Shell::setOutline(const BLRgba32 edge) {
    Chrome::outline(static_cast<std::uint8_t>(edge.r()), static_cast<std::uint8_t>(edge.g()),
                    static_cast<std::uint8_t>(edge.b()));
}

void Shell::minimize() {
    SDL_MinimizeWindow(_window);
}

void Shell::toggleMaximize() {
    if (_maximized) {
        SDL_RestoreWindow(_window);
    } else {
        SDL_MaximizeWindow(_window);
    }
}

int Shell::every(const double seconds, std::function<void()> what) {
    const int id = _nextAlarm++;

    _alarms.push_back(Alarm{
        .id = id,
        .due = now() + seconds,
        .every = seconds,
        .what = std::move(what),
    });

    return id;
}

int Shell::after(const double seconds, std::function<void()> what) {
    const int id = _nextAlarm++;

    _alarms.push_back(Alarm{
        .id = id,
        .due = now() + seconds,
        .every = 0.0,
        .what = std::move(what),
    });

    return id;
}

void Shell::cancel(const int id) {
    std::erase_if(_alarms, [id](const Alarm &alarm) { return alarm.id == id; });
}

void Shell::setCursor(const toolkit::Cursor wanted) {
    if (wanted == _cursor) {
        return;
    }

    _cursor = wanted;

    const auto held = _cursors.find(wanted);

    if (held != _cursors.end()) {
        SDL_SetCursor(held->second);

        return;
    }

    SDL_SystemCursor system = SDL_SYSTEM_CURSOR_DEFAULT;

    switch (wanted) {
        case toolkit::Cursor::Pointer:
            system = SDL_SYSTEM_CURSOR_POINTER;

            break;

        case toolkit::Cursor::Text:
            system = SDL_SYSTEM_CURSOR_TEXT;

            break;

        case toolkit::Cursor::Resize:
            system = SDL_SYSTEM_CURSOR_NS_RESIZE;

            break;

        case toolkit::Cursor::Grab:
        case toolkit::Cursor::Grabbing:
            system = SDL_SYSTEM_CURSOR_MOVE;

            break;

        default:
            break;
    }

    SDL_Cursor *made = SDL_CreateSystemCursor(system);

    _cursors[wanted] = made;

    if (made != nullptr) {
        SDL_SetCursor(made);
    }
}

void Shell::post(std::function<void()> what) {
    {
        const std::scoped_lock held(_posted);

        _errands.push_back(std::move(what));
    }

    // A loop asleep in SDL_WaitEventTimeout has to be told, and only an event does
    // that from another thread.
    SDL_Event wake{};

    wake.type = _wakeEvent;

    SDL_PushEvent(&wake);
}

void Shell::errands() {
    std::vector<std::function<void()>> due;

    {
        const std::scoped_lock held(_posted);

        due.swap(_errands);
    }

    for (const std::function<void()> &what : due) {
        what();
    }
}

bool Shell::alarms(const double at) {
    bool ran = false;

    // Copied: an alarm may add or cancel one while it runs.
    const std::vector<Alarm> due = _alarms;

    for (const Alarm &alarm : due) {
        if (alarm.due > at) {
            continue;
        }

        const auto held = std::ranges::find_if(_alarms, [&alarm](const Alarm &kept) {
            return kept.id == alarm.id;
        });

        if (held == _alarms.end()) {
            continue;
        }

        if (alarm.every > 0.0) {
            held->due = at + alarm.every;
        } else {
            _alarms.erase(held);
        }

        alarm.what();

        ran = true;
    }

    return ran;
}

int Shell::sleepFor(const double at) const {
    double soonest = -1.0;

    for (const Alarm &alarm : _alarms) {
        if (soonest < 0.0 || alarm.due < soonest) {
            soonest = alarm.due;
        }
    }

    if (soonest < 0.0) {
        return 1000;
    }

    return std::clamp(static_cast<int>((soonest - at) * 1000.0), 0, 1000);
}

void Shell::relayout() {
    if (!_surface.sync(_window)) {
        return;
    }

    const double width = _surface.width();
    const double height = _surface.height();

    if (width == _width && height == _height) {
        return;
    }

    _width = width;
    _height = height;

    _root->resize(width, height);

    if (resized) {
        resized(width, height);
    }
}

void Shell::draw() {
    relayout();

    if (!_surface.ready()) {
        return;
    }

    _root->settle();

    for (const BLRect &region : _root->take()) {
        _surface.damage(region);
    }

    for (const toolkit::Root::Shift &moved : _root->takeShifts()) {
        _surface.shift(moved.region, moved.dy);
    }

    if (!_surface.dirty()) {
        return;
    }

    BLContext &context = _surface.context();

    for (const BLRectI &region : _surface.regions()) {
        context.save();
        context.clip_to_rect(region);
        context.fill_rect(BLRect{static_cast<double>(region.x), static_cast<double>(region.y),
                                 static_cast<double>(region.w), static_cast<double>(region.h)},
                          Theme::of().background);

        _root->paint(context, region);

        context.restore();
    }

    _surface.present(_window);
}

bool SDLCALL ShellHooks::watch(void *held, SDL_Event *event) {
    auto *shell = static_cast<Shell *>(held);
    SDL_Window *window = shell->_window;

    if (!shell->_ready) {
        return true;
    }

    // A desktop that runs a resize inside a modal loop of its own never comes back
    // to ours until the drag is over. A watch is called as the event is pushed,
    // which happens inside that loop, so the frame is drawn from there.
    switch (event->type) {
        case SDL_EVENT_WINDOW_RESIZED: {
            shell->relayout();

            // A resize that lands on the same size still gets SDL's pixels back
            // as a fresh block, so the frame is drawn whole either way.
            shell->_surface.damageAll();
            shell->_root->damageAll();

            // SDL throws the window surface away in the handler that runs *after*
            // this event is pushed, so right here it is still the old one. Drawing
            // against it would lay the page out at the size it used to be; the
            // live-resize expose that follows draws the frame instead.
            int wide = 0;
            int tall = 0;

            SDL_GetWindowSizeInPixels(window, &wide, &tall);

            if (wide != shell->_surface.width() || tall != shell->_surface.height()) {
                break;
            }

            shell->_root->setNow(Shell::now());
            shell->_root->advance(Shell::now());
            shell->draw();

            break;
        }

        case SDL_EVENT_WINDOW_EXPOSED:
            shell->_surface.damageAll();
            shell->draw();

            break;

        default:
            break;
    }

    return true;
}

void Shell::handle(const SDL_Event &event) {
    // A tween this event starts is clocked from now, not from the frame the loop
    // went to sleep after; otherwise the first step after an idle spell jumps.
    _root->setNow(now());

    switch (event.type) {
        case SDL_EVENT_QUIT:
        case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
            if (closing) {
                closing();
            }

            _running = false;

            break;

        // Anything that can make SDL throw the window's pixels away and allocate
        // them again. Off the direct path only the damaged rectangles are handed
        // over, so a fresh block has to be filled whole.
        case SDL_EVENT_WINDOW_RESIZED:
        case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
        case SDL_EVENT_WINDOW_SHOWN:
        case SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED:
        case SDL_EVENT_WINDOW_ENTER_FULLSCREEN:
        case SDL_EVENT_WINDOW_LEAVE_FULLSCREEN:
            relayout();

            _surface.damageAll();
            _root->damageAll();

            break;

        case SDL_EVENT_WINDOW_MAXIMIZED:
        case SDL_EVENT_WINDOW_RESTORED:
            _maximized = event.type == SDL_EVENT_WINDOW_MAXIMIZED;

            _surface.damageAll();
            _root->damageAll();

            if (shadeChanged) {
                shadeChanged();
            }

            break;

        case SDL_EVENT_SYSTEM_THEME_CHANGED:
            Theme::setSystemDark(SDL_GetSystemTheme() != SDL_SYSTEM_THEME_LIGHT);

            if (shadeChanged) {
                shadeChanged();
            }

            break;

        case SDL_EVENT_MOUSE_MOTION:
            _root->motion(event.motion.x, event.motion.y);

            break;

        case SDL_EVENT_MOUSE_BUTTON_DOWN: {
            const toolkit::Click which = buttonOf(event.button.button);

            if (which == toolkit::Click::Back || which == toolkit::Click::Forward) {
                break;
            }

            const SDL_Keymod mods = SDL_GetModState();

            _root->press(toolkit::Pointer{
                .x = event.button.x,
                .y = event.button.y,
                .button = which,
                .ctrl = (mods & SDL_KMOD_CTRL) != 0,
                .shift = (mods & SDL_KMOD_SHIFT) != 0,
            });

            break;
        }

        case SDL_EVENT_MOUSE_BUTTON_UP: {
            const toolkit::Click which = buttonOf(event.button.button);

            if (which == toolkit::Click::Back) {
                if (back) {
                    back();
                }

                break;
            }

            if (which == toolkit::Click::Forward) {
                if (forward) {
                    forward();
                }

                break;
            }

            _root->release(toolkit::Pointer{
                .x = event.button.x,
                .y = event.button.y,
                .button = which,
            });

            break;
        }

        case SDL_EVENT_MOUSE_WHEEL:
            _root->wheel(event.wheel.y, event.wheel.mouse_x, event.wheel.mouse_y);

            break;

        case SDL_EVENT_WINDOW_MOUSE_LEAVE:
            _root->leave();

            break;

        case SDL_EVENT_KEY_DOWN: {
            const toolkit::Key pressed{
                .code = static_cast<int>(event.key.key),
                .text = {},
                .ctrl = (event.key.mod & SDL_KMOD_CTRL) != 0,
                .shift = (event.key.mod & SDL_KMOD_SHIFT) != 0,
                .alt = (event.key.mod & SDL_KMOD_ALT) != 0,
            };

            if (_root->key(pressed)) {
                break;
            }

            if (shortcut && shortcut(pressed)) {
                break;
            }

            break;
        }

        case SDL_EVENT_TEXT_INPUT:
            _root->wrote(event.text.text);

            break;

        default:
            break;
    }
}

void Shell::run() {
    SDL_AddEventWatch(ShellHooks::watch, this);

    _root->setNow(now());

    // The page holds nothing until settle fills it, and the first frame is drawn
    // here.
    if (settle) {
        settle();
    }

    _root->damageAll();

    draw();

    SDL_ShowWindow(_window);

    // The frame above went into a hidden window, where the blit goes nowhere. This
    // is the one the desktop actually shows.
    _surface.damageAll();
    draw();

    // SDL_GetCurrentDisplayMode enumerates the display's whole mode list, half a
    // second of it on a high-refresh monitor. The desktop mode reads the same rate.
    const SDL_DisplayMode *mode = SDL_GetDesktopDisplayMode(SDL_GetDisplayForWindow(_window));
    const double refresh = mode != nullptr && mode->refresh_rate > 1.0F ? mode->refresh_rate
                                                                       : 60.0;
    const auto frame = static_cast<Uint64>(1000000000.0 / refresh);

    Uint64 painted = SDL_GetTicksNS();

    while (_running) {
        SDL_Event event;

        // Nothing in flight: block until the desktop or an alarm has something to
        // say. This is where the idle cost goes to nothing at all.
        if (!_root->busy() && !_root->dirty()) {
            if (SDL_WaitEventTimeout(&event, sleepFor(now()))) {
                handle(event);
            }
        } else if (const Uint64 since = SDL_GetTicksNS() - painted; since < frame) {
            SDL_DelayNS(frame - since);
        }

        while (SDL_PollEvent(&event)) {
            handle(event);
        }

        const double at = now();

        _root->setNow(at);

        errands();
        alarms(at);

        if (settle) {
            settle();
        }

        setCursor(_root->cursor());

        _root->advance(at);

        draw();

        painted = SDL_GetTicksNS();
    }
}

void Shell::geometry(int &x, int &y, int &width, int &height) const {
    SDL_GetWindowPosition(_window, &x, &y);
    SDL_GetWindowSize(_window, &width, &height);
}

void Shell::setGeometry(const int x, const int y, const int width, const int height) {
    if (width > 0 && height > 0) {
        SDL_SetWindowSize(_window, width, height);
    }

    if (x >= 0 && y >= 0) {
        SDL_SetWindowPosition(_window, x, y);
    }
}
