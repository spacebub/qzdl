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

// The frame loop's timers and its way back from a worker thread, without the window
// around them. A service wants to be run again later; it has nothing to draw, and
// depending on the window is what made the folder graph cyclic.
class Clock {
public:
    Clock() = default;
    virtual ~Clock() = default;

    Clock(const Clock &) = delete;
    Clock &operator=(const Clock &) = delete;
    Clock(Clock &&) = delete;
    Clock &operator=(Clock &&) = delete;

    // Runs `what` every `seconds` until cancelled. Keeps the loop awake.
    virtual int every(double seconds, std::function<void()> what) = 0;

    // Runs `what` once, `seconds` from now.
    virtual int after(double seconds, std::function<void()> what) = 0;

    virtual void cancel(int id) = 0;

    // Runs `what` on the interface thread, from any thread, and wakes the loop.
    virtual void post(std::function<void()> what) = 0;
};
