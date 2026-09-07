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
#pragma once

#include <chrono>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "core/Process.h"
#include "main.h"
#include "gui/RunLog.h"

// What ZDL has started, filed under a name the caller chooses. A Doom port cannot
// be asked whether it loaded, so time stands in: alive a few seconds after
// starting means running. Logs outlive the run, to be read after a crash.
class Runs {
public:
    explicit Runs(const ui::Zdl *window);

    // `output` is the game's own stream where it was asked for.
    void began(const std::string &key, const std::string &title, const std::string &commandLine,
               Process::Id id, Process::Stream output);

    void refused(const std::string &key, const std::string &title, const std::string &reason);

    [[nodiscard]] bool alive(const std::string &key) const;

private:
    struct Run {
        Process::Id id{0};
        std::string state;
        std::string reason;
        std::string title;

        // ZDL asked it to quit, so its end is not read as a crash.
        bool asked{false};

            std::chrono::steady_clock::time_point since;
    };

    void sweep();

    static void set(Run &run, const std::string &state, const std::string &reason = {});

    RunLog *open(const std::string &key);

    [[nodiscard]] RunLog *log(const std::string &key) const;

    // Drops the oldest logs past KEPT, leaving anything running or on screen.
    void forget();

    void dock(const std::string &key);

    void show(const std::string &key);
    void hide();
    void toggle(const std::string &key);
    void close(const std::string &key);

    // Only the open one is told about its lines; the rest are not told at all.
    void listen();

    void push();
    void pushDock();
    void pushLines();

    // How long a game is given to fall over before it counts as running.
    static constexpr std::chrono::milliseconds SETTLE{4000};

    // How long an ended run is left on the card.
    static constexpr std::chrono::milliseconds CLOSED{5000};
    static constexpr std::chrono::milliseconds FAILED{15000};

    // Logs held on to after their run has ended.
    static constexpr size_t KEPT = 8;

    const ui::Zdl *_window;

    std::map<std::string, Run> _runs;

    // Outlive the run, so a log opened later still knows what it belongs to.
    std::map<std::string, std::unique_ptr<RunLog>> _logs;
    std::map<std::string, std::string> _titles;

    // The names in _logs, least recently launched first.
    std::vector<std::string> _order;

    std::vector<std::string> _docked;
    std::string _showing;

    // Up with no card watching -- the same thing launched twice. Polled only
    // so they are reaped when they end.
    std::vector<Process::Id> _orphans;

    // Which log the interface is told about, and how much of it it has.
    RunLog *_watching{nullptr};
    int _shownGeneration{-1};
    size_t _shownCount{0};

    std::shared_ptr<slint::VectorModel<ui::LogRow>> _lines;
    int _rev{0};

    slint::Timer _clock;
};
