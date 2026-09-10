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

#include <atomic>
#include <chrono>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <slint.h>

#include "core/system/Process.h"

// Reads a child's output on its own thread, so a full pipe never blocks the game.
class RunLog {
public:
    struct Line {
        std::string text;

        // Written by ZDL rather than the game.
        bool own{false};
    };

    RunLog();
    ~RunLog();

    RunLog(const RunLog &) = delete;
    RunLog &operator=(const RunLog &) = delete;
    RunLog(RunLog &&) = delete;
    RunLog &operator=(RunLog &&) = delete;

    void watch(Process::Stream output);

    void note(const std::string &text);

    [[nodiscard]] const std::vector<Line> &lines() const { return _lines; }

    // Bumped when lines were dropped, so appending the tail is not enough.
    [[nodiscard]] int generation() const { return _generation; }

    // Nothing is published while inactive.
    [[nodiscard]] bool active() const { return _active; }

    void setActive(bool value);

    [[nodiscard]] bool live() const { return _output != Process::NOTHING; }

    [[nodiscard]] std::string text() const;

    void clear();

    // Called when there is something new to draw.
    std::function<void()> published;

private:
    // Reader thread.
    void read();

    // Interface thread.
    void harvest();

    void release();

    void publish();

    static constexpr size_t LIMIT = 4000;

    // A never-ending line is cut here, so one Text cannot grow without bound.
    static constexpr size_t WIDEST = 1000;

    // Past this the reader drops the oldest rather than blocking the game.
    static constexpr size_t WAITING = LIMIT * 2;

    // Lines gather for BATCH before a redraw; polling oftener than that is pointless.
    static constexpr std::chrono::milliseconds BATCH{60};
    static constexpr std::chrono::milliseconds POLL{120};

    static constexpr std::chrono::milliseconds QUIET{10};

    Process::Stream _output{Process::NOTHING};

    std::thread _reader;
    std::atomic<bool> _quit{false};

    std::mutex _guard;
    std::vector<Line> _arrived;
    bool _closed{true};
    bool _lost{false};

    std::vector<Line> _lines;

    std::vector<Line> _pending;

    // Lines arrived while inactive, so the whole buffer is stale.
    bool _missed{false};

    bool _active{false};
    int _generation{0};

    slint::Timer _poll;
    slint::Timer _batch;
};
