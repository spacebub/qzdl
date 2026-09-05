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

#include <algorithm>

#include "gui/Runs.h"

namespace {

constexpr QLatin1StringView LAUNCHING("launching");
constexpr QLatin1StringView RUNNING("running");
constexpr QLatin1StringView CLOSED("closed");
constexpr QLatin1StringView FAILED("failed");

QString explain(const int code) {
    if (code < 0) {
        return QStringLiteral("It was killed (signal %1).").arg(-code);
    }

    return QStringLiteral("It stopped with an error (code %1).").arg(code);
}

}

Runs::Runs(QObject *parent) : QObject(parent) {
    // The clock only runs while there is something on it.
    _clock.setInterval(500);

    connect(&_clock, &QTimer::timeout, this, &Runs::sweep);
}

RunLog *Runs::open(const QString &key) {
    RunLog *&log = _logs[key];


    if (log == nullptr) {
        log = new RunLog(this);
    }

    return log;
}

void Runs::began(const QString &key, const QString &title, const QString &commandLine,
                 const Process::Id id, const Process::Stream output) {
    Run &run = _runs[key];

    // A second launch takes the card over; the first is followed without one.
    if (run.id != 0 && (run.state == LAUNCHING || run.state == RUNNING)) {
        _orphans.append(run.id);
    }

    run.id = id;
    run.title = title;
    run.asked = false;
    _titles[key] = title;
    run.state = LAUNCHING;
    run.reason.clear();
    run.since.start();

    RunLog *log = open(key);

    log->clear();
    log->note(QStringLiteral("Launching %1.").arg(title));
    log->note(QStringLiteral("$ %1").arg(commandLine));

    if (output != Process::NOTHING) {
        log->watch(output);
        dock(key);
    } else {
        log->note(QStringLiteral("This profile is not recording the game's output, so only "
                                 "ZDL's own side of it is here."));
    }

    _clock.start();

    emit changed();
}

void Runs::refused(const QString &key, const QString &title, const QString &reason) {
    Run &run = _runs[key];

    run.id = 0;
    run.title = title;
    run.state = FAILED;
    _titles[key] = title;
    run.reason = reason;
    run.since.start();

    RunLog *log = open(key);

    log->note(QStringLiteral("It would not start: %1").arg(reason));

    _clock.start();

    emit changed();
}

QVariantMap Runs::states() const {
    QVariantMap out;

    for (auto each = _runs.constBegin(); each != _runs.constEnd(); ++each) {
        out.insert(each.key(), each.value().state);
    }

    return out;
}

bool Runs::busy() const {
    return std::ranges::any_of(_runs, [](const Run &run) {
        return run.state == LAUNCHING || run.state == RUNNING;
    });
}

QString Runs::reason(const QString &key) const {
    const auto found = _runs.constFind(key);

    return found == _runs.constEnd() ? QString() : found->reason;
}

QString Runs::title(const QString &key) const {
    return _titles.value(key);
}

bool Runs::alive(const QString &key) const {
    const auto found = _runs.constFind(key);

    return found != _runs.constEnd() && (found->state == LAUNCHING || found->state == RUNNING);
}

RunLog *Runs::log(const QString &key) const {
    const auto found = _logs.constFind(key);

    return found == _logs.constEnd() ? nullptr : found.value();
}

void Runs::dock(const QString &key) {
    if (!_docked.contains(key)) {
        _docked.append(key);

        emit dockChanged();
    }
}

void Runs::show(const QString &key) {
    dock(key);

    if (_showing != key) {
        _showing = key;

        emit dockChanged();
    }
}

void Runs::hide() {
    if (!_showing.isEmpty()) {
        _showing.clear();

        emit dockChanged();
    }
}

void Runs::toggle(const QString &key) {
    if (_showing == key) {
        hide();
    } else {
        show(key);
    }
}

void Runs::close(const QString &key) {
    if (const auto found = _runs.find(key); found != _runs.end() && alive(key) && found->id != 0) {
        found->asked = true;

        Process::stop(found->id);

        if (RunLog *log = this->log(key); log != nullptr) {
            log->note(QStringLiteral("ZDL asked it to quit."));
        }
    }

    if (_showing == key) {
        _showing.clear();
    }

    // The log stays: it is still worth reading, and the profile page can ask
    // for it again long after the tab has gone.
    _docked.removeAll(key);

    emit dockChanged();
}

void Runs::set(Run &run, const QString &state, const QString &reason) {
    run.state = state;
    run.reason = reason;
    run.since.start();
}

void Runs::sweep() {
    bool moved = false;

    _orphans.removeIf([](const Process::Id id) {
        return Process::poll(id) != Process::State::Running;
    });

    for (auto each = _runs.begin(); each != _runs.end();) {
        Run &run = each.value();

        if (run.state == LAUNCHING || run.state == RUNNING) {
            int code = 0;
            RunLog *log = this->log(each.key());

            switch (Process::poll(run.id, &code)) {
                case Process::State::Running:
                    if (run.state == LAUNCHING && run.since.elapsed() >= SETTLE_MS) {
                        set(run, RUNNING);
                        moved = true;
                    }

                    break;

                case Process::State::Finished:
                    set(run, CLOSED);

                    if (log != nullptr) {
                        log->note(QStringLiteral("It closed."));
                    }

                    moved = true;
                    break;

                case Process::State::Failed: {
                    // A game that ZDL asked to quit did what it was told; the
                    // signal that carried the request is not a fault of its own.
                    const bool fault = !run.asked;
                    const QString said = fault ? explain(code) : QStringLiteral("It was stopped.");

                    set(run, fault ? FAILED : CLOSED, fault ? said : QString());

                    if (log != nullptr) {
                        log->note(said);
                    }

                    moved = true;
                    break;
                }

                case Process::State::Unknown:
                    // The hold is gone without its end having been seen.
                    each = _runs.erase(each);
                    moved = true;
                    continue;
            }

            ++each;

            continue;
        }

        const int keep = run.state == FAILED ? FAILED_MS : CLOSED_MS;

        if (run.since.elapsed() >= keep) {
            each = _runs.erase(each);
            moved = true;

            continue;
        }

        ++each;
    }

    if (_runs.isEmpty() && _orphans.isEmpty()) {
        _clock.stop();
    }

    if (moved) {
        emit changed();
    }
}
