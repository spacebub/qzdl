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

#include "core/Process.h"

// What one game printed, read on a thread of its own: a child whose output nobody
// takes stops when the pipe fills, and drawing must never decide whether it runs.
class RunLog {
public:
    struct Line {
        std::string text;

        // A line ZDL wrote about the run rather than one the game did.
        bool own{false};
    };

    RunLog();
    ~RunLog();

    RunLog(const RunLog &) = delete;
    RunLog &operator=(const RunLog &) = delete;
    RunLog(RunLog &&) = delete;
    RunLog &operator=(RunLog &&) = delete;

    void watch(Process::Stream output);

    // One of ZDL's own lines, in its place among the game's.
    void note(const std::string &text);

    [[nodiscard]] const std::vector<Line> &lines() const { return _lines; }

    // Bumped when the buffer lost lines rather than gained them, which says that
    // appending the tail is not enough.
    [[nodiscard]] int generation() const { return _generation; }

    // Whether anything is showing it; nothing is published while nothing is.
    [[nodiscard]] bool active() const { return _active; }

    void setActive(bool value);

    // True while the pipe is still open.
    [[nodiscard]] bool live() const { return _output != Process::NOTHING; }

    // All of it as one string, for the clipboard.
    [[nodiscard]] std::string text() const;

    void clear();

    // Told whenever there is something new to draw.
    std::function<void()> published;

private:
    // On the reader's thread: the pipe, emptied while it is open.
    void read();

    // On the interface's thread: what the reader has gathered since last time.
    void harvest();

    void release();

    void publish();

    // As far back as the console can be scrolled.
    static constexpr size_t LIMIT = 4000;

    // A game that never ends a line would otherwise be one enormous Text, laid
    // out again on every pass.
    static constexpr size_t WIDEST = 1000;

    // Held for an interface that stopped taking them. Past this the oldest go:
    // losing lines beats blocking the game on a full pipe.
    static constexpr size_t WAITING = LIMIT * 2;

    // Gathered this long before anything is told. The poll below is twice this:
    // asking oftener than the gathering only wakes the loop for nothing.
    static constexpr std::chrono::milliseconds BATCH{60};
    static constexpr std::chrono::milliseconds POLL{120};

    // What the reader waits on an empty pipe.
    static constexpr std::chrono::milliseconds QUIET{10};

    Process::Stream _output{Process::NOTHING};

    std::thread _reader;
    std::atomic<bool> _quit{false};

    // The only thing either thread locks for.
    std::mutex _guard;
    std::vector<Line> _arrived;
    bool _closed{true};
    bool _lost{false};

    std::vector<Line> _lines;

    std::vector<Line> _pending;

    // Lines arrived while nothing was looking, so the whole of it is stale.
    bool _missed{false};

    bool _active{false};
    int _generation{0};

    slint::Timer _poll;
    slint::Timer _batch;
};
