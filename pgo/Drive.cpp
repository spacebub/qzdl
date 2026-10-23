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

#include <chrono>
#include <cmath>
#include <thread>
#include <utility>

#include "ttk/draw/Theme.h"
#include "ttk/toolkit/Event.h"

#include "Drive.h"
#include "gui/components/TitleBar.h"

namespace pgo {

Drive::Drive(const int width, const int height, const double hz)
    : _canvas(std::make_unique<tests::Canvas>(width, height)), _rig(std::make_unique<tests::Rig>(*_canvas)), _hz(hz) {
}

Drive::~Drive() {
    _rig.reset();
    _canvas.reset();
}

void Drive::point() {
    const ttk::Widget *over = ui().hovered();
    const double x = ui().pointer_x();
    const double y = ui().pointer_y();

    if (over != nullptr && !over->hint.empty()) {
        _rig->tips().point(over->hint, over->box(), x, y, _canvas->now());
    } else {
        _rig->tips().point({}, BLRect{}, x, y, _canvas->now());
    }
}

std::size_t Drive::turn() {
    const double step = 1.0 / _hz;

    _rig->shell().advance(step);

    if (_rig->dirty()) {
        _rig->sync();
    }

    point();

    _rig->toasts().set_messages(_rig->notify().messages());

    const std::size_t painted = _canvas->frameAt(_canvas->now() + step);

    ++tally.frames;
    tally.pixels += painted;

    return painted;
}

std::size_t Drive::turnFor(const double seconds) {
    std::size_t painted = 0;

    for (int at = 0; at < static_cast<int>(std::ceil(seconds * _hz)); ++at) {
        painted += turn();
    }

    return painted;
}

std::size_t Drive::rest(const int most) {
    std::size_t painted = turn();

    for (int at = 1; at < most && (ui().busy() || _rig->dirty()); ++at) {
        painted += turn();
    }

    return painted;
}

std::size_t Drive::idle(const int frames) {
    std::size_t painted = 0;

    for (int at = 0; at < frames; ++at) {
        painted += turn();
    }

    return painted;
}

void Drive::move(const double x, const double y) {
    ui().motion(x, y);
    turn();
}

void Drive::click(const double x, const double y) {
    ui().motion(x, y);
    turn();
    ui().press(ttk::Pointer{.x = x, .y = y});
    turn();
    ui().release(ttk::Pointer{.x = x, .y = y});
    turn();
}

void Drive::click(const ttk::Widget *widget) {
    const BLPoint at = middle(widget->box());

    click(at.x, at.y);
}

void Drive::drag(const double fromX, const double fromY, const double toX, const double toY, const int steps) {
    ui().motion(fromX, fromY);
    turn();
    ui().press(ttk::Pointer{.x = fromX, .y = fromY});
    turn();

    for (int step = 1; step <= steps; ++step) {
        const double share = static_cast<double>(step) / steps;

        ui().motion(fromX + ((toX - fromX) * share), fromY + ((toY - fromY) * share));
        turn();
    }

    ui().release(ttk::Pointer{.x = toX, .y = toY});
    turn();
}

void Drive::wheel(const double steps, const double x, const double y) {
    ui().wheel(steps, x, y);
    turn();
}

void Drive::type(const std::string &text) {
    for (const char each : text) {
        ui().wrote(std::string(1, each));
        turn();
    }
}

void Drive::key(const int code, const bool ctrl, const bool shift) {
    ui().key(ttk::Key{.code = code, .ctrl = ctrl, .shift = shift});
    turn();
}

void Drive::tab(const State::Page page) {
    const components::TitleBar &bar = _rig->bar();
    const double y = ttk::Theme::barHeight / 2.0;

    // The tabs are the runs the window manager may not drag by, left of the buttons.
    std::vector<std::pair<double, double>> spans;
    bool inside = false;

    for (int x = 0; x < static_cast<int>(bar.children().front()->box().x); ++x) {
        const bool held = !bar.draggable(x, y);

        if (held && !inside) {
            spans.emplace_back(x, x);
        }

        if (held) {
            spans.back().second = x + 1.0;
        }

        inside = held;
    }

    const auto index = static_cast<size_t>(page);

    check(index < spans.size(), "the title bar has a tab for every page");

    if (index < spans.size()) {
        click((spans[index].first + spans[index].second) / 2.0, y);
        rest();
    }
}

bool Drive::until(const std::function<bool()> &done, const double seconds) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::duration<double>(seconds);

    while (!done()) {
        if (std::chrono::steady_clock::now() >= deadline) {
            return false;
        }

        turn();

        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }

    return true;
}

void Drive::check(const bool ok, const std::string &what) {
    if (!ok) {
        tally.failed.push_back(what);
    }
}

}
