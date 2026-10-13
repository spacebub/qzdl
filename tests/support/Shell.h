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

#include <cstddef>
#include <deque>
#include <functional>
#include <mutex>
#include <vector>

#include "ttk/util/Clock.h"
#include "ttk/util/Window.h"

namespace tests {

// The frame loop's clock and the window's controls with no window behind them.
// A timer fires when a test advances the clock past it, and a function posted
// from any thread runs when a test drains the queue, so every service that
// schedules work can be driven one step at a time.
class Shell final : public ttk::Clock, public ttk::Window {
public:
    int every(double seconds, std::function<void()> what) override;
    int after(double seconds, std::function<void()> what) override;
    void cancel(int id) override;
    void post(std::function<void()> what) override;

    void minimize() const override { ++minimized; }
    void toggle_maximize() const override;
    [[nodiscard]] bool maximized() const override { return _maximized; }
    void stop() override { stopped = true; }

    [[nodiscard]] double now() const { return _now; }

    // Moves the clock by `seconds`, then runs what was posted and every timer due,
    // in due order, until nothing more is due. Zero runs only what is due already.
    // A repeating timer runs once however far the clock jumped and is next due its
    // period after that run, as the real loop does when a turn is late, so step the
    // clock to see every beat.
    void advance(double seconds);

    // Runs what was posted, and nothing else.
    void drain();

    // Blocks up to `seconds` of wall time for a post from another thread, then runs
    // what was posted. False when nothing arrived.
    bool await(double seconds = 2.0);

    [[nodiscard]] std::size_t pending() const { return _timers.size(); }
    [[nodiscard]] std::size_t posted() const;
    [[nodiscard]] bool holds(int id) const;

    // Seconds until the nearest timer, or a negative number for none.
    [[nodiscard]] double nextDue() const;

    mutable int minimized = 0;
    mutable int toggled = 0;
    bool stopped = false;

private:
    struct Timer {
        int id;
        double due;

        // Zero for one that runs once.
        double period;

        std::function<void()> what;
    };

    bool runDue();

    std::vector<Timer> _timers;

    mutable std::mutex _guard;
    std::deque<std::function<void()>> _posted;

    double _now = 0.0;
    int _next = 1;

    mutable bool _maximized = false;
};

}
