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

#include "gui/Convert.h"
#include "gui/components/Runs.h"

namespace {

constexpr auto LAUNCHING = "launching";
constexpr auto RUNNING = "running";
constexpr auto STOPPING = "stopping";
constexpr auto CLOSED_STATE = "closed";
constexpr auto FAILED_STATE = "failed";

constexpr std::chrono::milliseconds TICK{500};

std::string explain(const int code) {
    return code < 0
        ? "It was killed (signal " + std::to_string(-code) + ")."
        : "It stopped with an error (code " + std::to_string(code) + ").";
}

std::chrono::milliseconds since(const std::chrono::steady_clock::time_point &when) {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - when);
}

}

Runs::Runs(const ui::Zdl *window)
    : _window(window),
      _lines(std::make_shared<slint::VectorModel<ui::LogRow>>()) {
    const auto &state = _window->global<ui::Runs>();

    state.set_lines(_lines);

    state.on_state_of([this](int, const slint::SharedString &key) {
        const auto found = _runs.find(Convert::plain(key));

        return found == _runs.end() ? slint::SharedString() : Convert::text(found->second.state);
    });

    state.on_reason_of([this](int, const slint::SharedString &key) {
        const auto found = _runs.find(Convert::plain(key));

        return found == _runs.end() ? slint::SharedString() : Convert::text(found->second.reason);
    });

    state.on_title_of([this](int, const slint::SharedString &key) {
        const auto found = _titles.find(Convert::plain(key));

        return found == _titles.end() ? slint::SharedString() : Convert::text(found->second);
    });

    state.on_alive([this](int, const slint::SharedString &key) {
        return alive(Convert::plain(key));
    });

    state.on_show([this](const slint::SharedString &key) { show(Convert::plain(key)); });
    state.on_hide([this] { hide(); });
    state.on_toggle([this](const slint::SharedString &key) { toggle(Convert::plain(key)); });
    state.on_close([this](const slint::SharedString &key) { close(Convert::plain(key)); });

    state.on_text([this] {
        const RunLog *open = log(_showing);

        return open == nullptr ? slint::SharedString() : Convert::text(open->text());
    });

    push();
    pushDock();
}

RunLog *Runs::open(const std::string &key) {
    std::unique_ptr<RunLog> &held = _logs[key];

    if (!held) {
        held = std::make_unique<RunLog>();
        held->published = [this] { pushLines(); };
    }

    std::erase(_order, key);
    _order.push_back(key);
    forget();

    return _logs[key].get();
}

RunLog *Runs::log(const std::string &key) const {
    const auto found = _logs.find(key);

    return found == _logs.end() ? nullptr : found->second.get();
}

void Runs::forget() {
    for (auto each = _order.begin(); _logs.size() > KEPT && each != _order.end();) {
        const std::string &key = *each;

        if (key == _showing || std::ranges::find(_docked, key) != _docked.end() || alive(key)) {
            ++each;

            continue;
        }

        _logs.erase(key);
        each = _order.erase(each);
    }
}

void Runs::began(const std::string &key, const std::string &title,
                 const std::string &commandLine, const Process::Id id,
                 const Process::Stream output) {
    Run &run = _runs[key];

    // A second launch takes the card; the first becomes an orphan.
    if (run.id != 0
        && (run.state == LAUNCHING || run.state == RUNNING || run.state == STOPPING)) {
        _orphans.push_back(run.id);
    }

    run.id = id;
    run.title = title;
    run.asked = false;
    _titles[key] = title;
    run.state = LAUNCHING;
    run.reason.clear();
    run.since = std::chrono::steady_clock::now();

    RunLog *held = open(key);

    held->clear();
    held->note("Launching " + title + ".");
    held->note("$ " + commandLine);

    if (output != Process::NOTHING) {
        held->watch(output);
        dock(key);
    } else {
        held->note("This profile is not recording the game's output, so only ZDL4's own side of "
                   "it is here.");
    }

    _clock.start(slint::TimerMode::Repeated, TICK, [this] { sweep(); });

    push();
}

void Runs::refused(const std::string &key, const std::string &title, const std::string &reason) {
    Run &run = _runs[key];

    run.id = 0;
    run.title = title;
    run.state = FAILED_STATE;
    _titles[key] = title;
    run.reason = reason;
    run.since = std::chrono::steady_clock::now();

    open(key)->note("It would not start: " + reason);

    _clock.start(slint::TimerMode::Repeated, TICK, [this] { sweep(); });

    push();
}

bool Runs::alive(const std::string &key) const {
    const auto found = _runs.find(key);

    return found != _runs.end()
        && (found->second.state == LAUNCHING || found->second.state == RUNNING
            || found->second.state == STOPPING);
}

void Runs::dock(const std::string &key) {
    if (std::ranges::find(_docked, key) == _docked.end()) {
        _docked.push_back(key);

        pushDock();
    }
}

void Runs::show(const std::string &key) {
    dock(key);

    if (_showing != key) {
        _showing = key;

        pushDock();
    }
}

void Runs::hide() {
    if (!_showing.empty()) {
        _showing.clear();

        pushDock();
    }
}

void Runs::toggle(const std::string &key) {
    if (_showing == key) {
        hide();
    } else {
        show(key);
    }
}

void Runs::close(const std::string &key) {
    const auto found = _runs.find(key);
    const bool up = found != _runs.end() && alive(key) && found->second.id != 0;

    if (up && found->second.state != STOPPING) {
        found->second.asked = true;
        set(found->second, STOPPING);
        Process::stop(found->second.id);

        if (RunLog *held = log(key); held != nullptr) {
            held->note("ZDL4 asked it to quit.");
        }

        if (_showing == key) {
            _showing.clear();
        }

        pushDock();

        return;
    }

    // A second close forces.
    if (up) {
        Process::force(found->second.id);

        if (RunLog *held = log(key); held != nullptr) {
            held->note("ZDL4 took it down.");
        }

        return;
    }

    if (_showing == key) {
        _showing.clear();
    }

    std::erase(_docked, key);

    pushDock();
}

void Runs::listen() {
    RunLog *wanted = _showing.empty() ? nullptr : log(_showing);

    if (_watching == wanted) {
        return;
    }

    if (_watching != nullptr) {
        _watching->setActive(false);
    }

    _watching = wanted;
    _shownGeneration = -1;
    _shownCount = 0;

    if (_watching != nullptr) {
        _watching->setActive(true);
    }

    pushLines();
}

void Runs::set(Run &run, const std::string &state, const std::string &reason) {
    run.state = state;
    run.reason = reason;
    run.since = std::chrono::steady_clock::now();
}

void Runs::sweep() {
    bool moved = false;

    // Runs that ended after a stop request lose their tab.
    std::vector<std::string> ended;

    std::erase_if(_orphans, [](const Process::Id id) {
        return Process::poll(id) != Process::State::Running;
    });

    for (auto each = _runs.begin(); each != _runs.end();) {
        Run &run = each->second;

        if (run.state == LAUNCHING || run.state == RUNNING || run.state == STOPPING) {
            int code = 0;
            RunLog *held = log(each->first);
            const bool going = run.state == STOPPING;

            switch (Process::poll(run.id, &code)) {
                case Process::State::Running:
                    if (run.state == LAUNCHING && since(run.since) >= SETTLE) {
                        set(run, RUNNING);
                        moved = true;
                    }

                    break;

                case Process::State::Finished:
                    set(run, CLOSED_STATE);

                    if (held != nullptr) {
                        held->note("It closed.");
                    }

                    moved = true;
                    break;

                case Process::State::Failed: {
                    const bool fault = !run.asked;
                    const std::string said = fault ? explain(code) : "It was stopped.";

                    set(run, fault ? FAILED_STATE : CLOSED_STATE, fault ? said : std::string());

                    if (held != nullptr) {
                        held->note(said);
                    }

                    moved = true;
                    break;
                }

                case Process::State::Unknown:
                    if (going) {
                        ended.push_back(each->first);
                    }

                    each = _runs.erase(each);
                    moved = true;

                    continue;
            }

            if (going && run.state != STOPPING) {
                ended.push_back(each->first);
            }

            ++each;

            continue;
        }

        if (since(run.since) >= (run.state == FAILED_STATE ? FAILED : CLOSED)) {
            each = _runs.erase(each);
            moved = true;

            continue;
        }

        ++each;
    }

    for (const std::string &key : ended) {
        std::erase(_docked, key);

        if (_showing == key) {
            _showing.clear();
        }
    }

    if (!ended.empty()) {
        pushDock();
    }

    if (_runs.empty() && _orphans.empty()) {
        _clock.stop();
    }

    if (moved) {
        push();
    }
}

void Runs::push() {
    const auto &state = _window->global<ui::Runs>();
    std::vector<std::string> logged;

    logged.reserve(_logs.size());

    for (const auto &[key, held] : _logs) {
        logged.push_back(key);
    }

    state.set_logged(Convert::strings(logged));

    state.set_busy(std::ranges::any_of(_runs, [](const auto &entry) {
        return entry.second.state == LAUNCHING || entry.second.state == RUNNING
            || entry.second.state == STOPPING;
    }));

    state.set_rev(++_rev);
}

void Runs::pushDock() {
    const auto &state = _window->global<ui::Runs>();

    state.set_docked(Convert::strings(_docked));
    state.set_showing(Convert::text(_showing));

    listen();
    push();
}

void Runs::pushLines() {
    if (_watching == nullptr) {
        if (_lines->row_count() > 0) {
            _lines->set_vector({});
        }

        _window->global<ui::Runs>().set_live(false);

        return;
    }

    const std::vector<RunLog::Line> &lines = _watching->lines();

    if (_watching->generation() != _shownGeneration || _shownCount > lines.size()) {
        std::vector<ui::LogRow> rows;

        rows.reserve(lines.size());

        for (const RunLog::Line &line : lines) {
            rows.push_back(ui::LogRow{.line = Convert::text(line.text), .own = line.own});
        }

        _lines->set_vector(std::move(rows));
        _shownGeneration = _watching->generation();
    } else {
        for (size_t row = _shownCount; row < lines.size(); row++) {
            _lines->push_back(ui::LogRow{
                .line = Convert::text(lines[row].text),
                .own = lines[row].own,
            });
        }
    }

    _shownCount = lines.size();

    _window->global<ui::Runs>().set_live(_watching->live());
}
