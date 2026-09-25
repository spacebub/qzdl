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

#include <chrono>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "ttk/system/Process.h"
#include "ttk/util/Clock.h"

#include "gui/services/RunLog.h"
#include "gui/state/State.h"

// A port cannot say whether it loaded, so alive SETTLE after starting counts as running.
class Runs {
public:
    explicit Runs(ttk::Clock *clock);

    void began(const std::string &key, const std::string &title, const std::string &commandLine,
               ttk::Process::Id id, ttk::Process::Stream output);

    void refused(const std::string &key, const std::string &title, const std::string &reason);

    [[nodiscard]] bool alive(const std::string &key) const;

    [[nodiscard]] State::RunState stateOf(const std::string &key) const;
    [[nodiscard]] std::string reasonOf(const std::string &key) const;
    [[nodiscard]] std::string titleOf(const std::string &key) const;

    void show(const std::string &key);
    void hide();
    void toggle(const std::string &key);
    void close(const std::string &key);

    // For the clipboard.
    [[nodiscard]] std::string text() const;

private:
    struct Run {
        ttk::Process::Id id{0};
        State::RunState state = State::RunState::None;
        std::string reason;
        std::string title;

        // Stopped by ZDL, so its end is not a crash.
        bool asked{false};

        std::chrono::steady_clock::time_point since;
    };

    void sweep();

    [[nodiscard]] static bool up(const Run &run);

    static void set(Run &run, State::RunState state, const std::string &reason = {});

    RunLog *open(const std::string &key);

    [[nodiscard]] RunLog *log(const std::string &key) const;

    // Drops the oldest logs past KEPT, except running or shown ones.
    void forget();

    void dock(const std::string &key);

    // Only the shown log is active.
    void listen();

    void push();
    void pushDock();
    void pushLines();

    static constexpr std::chrono::milliseconds SETTLE{4000};

    // How long an ended run stays on the card.
    static constexpr std::chrono::milliseconds CLOSED{5000};
    static constexpr std::chrono::milliseconds FAILED{15000};

    static constexpr size_t KEPT = 8;

    static constexpr double TICK = 0.5;

    ttk::Clock *_clock;

    std::map<std::string, Run> _runs;

    // Outlive the run, for reading after a crash.
    std::map<std::string, std::unique_ptr<RunLog>> _logs;
    std::map<std::string, std::string> _titles;

    // Keys of _logs, least recently launched first.
    std::vector<std::string> _order;

    std::vector<std::string> _docked;
    std::string _showing;

    // Running with no card, after the same thing was launched twice. Reaped on the sweep,
    // and still counted by alive().
    struct Orphan {
        std::string key;
        ttk::Process::Id id{0};
        bool asked{false};
    };

    std::vector<Orphan> _orphans;

    RunLog *_watching{nullptr};
    int _shownGeneration{-1};
    size_t _shownCount{0};
    size_t _shownDropped{0};

    int _rev{0};

    int _ticker{0};
};
