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
#include <chrono>
#include <thread>
#include <utility>

#include "support/Shell.h"

namespace tests {

int Shell::every(const double seconds, std::function<void()> what) {
    const int id = _next++;

    // A non-positive period runs once, on the next turn.
    _timers.push_back({.id = id,
                       .due = _now + std::max(seconds, 0.0),
                       .period = seconds > 0.0 ? seconds : 0.0,
                       .what = std::move(what)});

    return id;
}

int Shell::after(const double seconds, std::function<void()> what) {
    const int id = _next++;

    _timers.push_back({.id = id, .due = _now + std::max(seconds, 0.0), .period = 0.0, .what = std::move(what)});

    return id;
}

void Shell::cancel(const int id) {
    std::erase_if(_timers, [id](const Timer &timer) { return timer.id == id; });
}

void Shell::post(std::function<void()> what) {
    const std::scoped_lock hold(_guard);

    _posted.push_back(std::move(what));
}

void Shell::toggle_maximize() const {
    _maximized = !_maximized;
    ++toggled;
}

void Shell::drain() {
    std::deque<std::function<void()>> taken;

    {
        const std::scoped_lock hold(_guard);

        std::swap(taken, _posted);
    }

    for (std::function<void()> &what : taken) {
        if (what) {
            what();
        }
    }
}

bool Shell::runDue() {
    // The nearest first. A callback may add or cancel timers, so the list is
    // searched again after each one rather than walked.
    const auto nearest = std::min_element(
            _timers.begin(), _timers.end(), [](const Timer &one, const Timer &two) { return one.due < two.due; });

    if (nearest == _timers.end() || nearest->due > _now) {
        return false;
    }

    Timer taken = std::move(*nearest);

    _timers.erase(nearest);

    if (taken.period > 0.0) {
        _timers.push_back({.id = taken.id, .due = _now + taken.period, .period = taken.period, .what = taken.what});
    }

    if (taken.what) {
        taken.what();
    }

    return true;
}

void Shell::advance(const double seconds) {
    _now += std::max(seconds, 0.0);

    // A callback that posts or schedules something due now gets that run in the
    // same turn, as the real loop would on its next one. Bounded, since a timer
    // that reschedules itself at zero would otherwise never let go.
    for (int turn = 0; turn < 1000; ++turn) {
        drain();

        if (!runDue()) {
            break;
        }
    }
}

bool Shell::await(const double seconds) {
    const auto until = std::chrono::steady_clock::now() + std::chrono::duration<double>(seconds);

    while (posted() == 0) {
        if (std::chrono::steady_clock::now() >= until) {
            return false;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }

    drain();

    return true;
}

std::size_t Shell::posted() const {
    const std::scoped_lock hold(_guard);

    return _posted.size();
}

bool Shell::holds(const int id) const {
    return std::ranges::any_of(_timers, [id](const Timer &timer) { return timer.id == id; });
}

double Shell::nextDue() const {
    if (_timers.empty()) {
        return -1.0;
    }

    const auto nearest = std::min_element(
            _timers.begin(), _timers.end(), [](const Timer &one, const Timer &two) { return one.due < two.due; });

    return nearest->due - _now;
}

}
