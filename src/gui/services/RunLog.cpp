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

#include <cstddef>
#include <utility>

#include "gui/app/Shell.h"
#include "gui/services/RunLog.h"

RunLog::RunLog(Shell *shell) : _shell(shell) {}

RunLog::~RunLog() {
    release();
}

void RunLog::watch(const Process::Stream output) {
    release();

    if (output == Process::NOTHING) {
        return;
    }

    _output = output;
    _quit = false;

    {
        const std::scoped_lock held(_guard);

        _arrived.clear();
        _closed = false;
        _lost = false;
    }

    _reader = std::thread([this] { read(); });

    _poll = _shell->every(POLL, [this] { harvest(); });

    if (published) {
        published();
    }
}

void RunLog::release() {
    _shell->cancel(_poll);

    _poll = 0;
    _quit = true;

    if (_reader.joinable()) {
        _reader.join();
    }

    if (_output != Process::NOTHING) {
        Process::closeStream(_output);
        _output = Process::NOTHING;
    }
}

void RunLog::note(const std::string &text) {
    _pending.push_back(Line{.text = text, .own = true});

    if (!_active) {
        publish();

        return;
    }

    if (_batch == 0) {
        _batch = _shell->after(BATCH, [this] {
            _batch = 0;

            publish();
        });
    }
}

void RunLog::setActive(const bool value) {
    if (value == _active) {
        return;
    }

    _active = value;

    if (_active && _missed) {
        _missed = false;
        _generation++;

        if (published) {
            published();
        }
    }
}

std::string RunLog::text() const {
    std::string out;

    for (const Line &line : _lines) {
        if (!out.empty()) {
            out += '\n';
        }

        out += line.text;
    }

    return out;
}

void RunLog::clear() {
    _lines.clear();
    _pending.clear();

    {
        const std::scoped_lock held(_guard);

        _arrived.clear();
        _lost = false;
    }

    _missed = false;
    _generation++;

    if (published) {
        published();
    }
}

void RunLog::read() {
    std::string partial;
    std::string chunk;
    std::vector<Line> gathered;
    bool ended = false;

    const auto hand = [this, &gathered] {
        if (gathered.empty()) {
            return;
        }

        const std::scoped_lock held(_guard);

        _arrived.insert(_arrived.end(), std::make_move_iterator(gathered.begin()),
                        std::make_move_iterator(gathered.end()));
        gathered.clear();

        if (_arrived.size() > WAITING) {
            _arrived.erase(_arrived.begin(),
                           _arrived.begin() + static_cast<std::ptrdiff_t>(_arrived.size() - WAITING));
            _lost = true;
        }
    };

    const auto take = [&gathered](std::string line) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        gathered.push_back(Line{.text = std::move(line), .own = false});
    };

    // Forced cuts back off to a UTF-8 boundary.
    const auto cut = [&partial, &take] {
        size_t from = 0;

        while (true) {
            const size_t end = partial.find('\n', from);

            if (end != std::string::npos && end - from <= WIDEST) {
                take(partial.substr(from, end - from));
                from = end + 1;

                continue;
            }

            if (partial.size() - from <= WIDEST) {
                break;
            }

            size_t at = WIDEST;

            while (at > WIDEST - 3
                   && (static_cast<unsigned char>(partial[from + at]) & 0xC0) == 0x80) {
                at--;
            }

            take(partial.substr(from, at));
            from += at;
        }

        partial.erase(0, from);
    };

    while (!_quit && !ended) {
        bool idle = true;

        while (!_quit) {
            if (!Process::read(_output, chunk)) {
                ended = true;

                break;
            }

            if (chunk.empty()) {
                break;
            }

            idle = false;
            partial += chunk;
            cut();
        }

        hand();

        if (idle && !ended) {
            std::this_thread::sleep_for(QUIET);
        }
    }

    if (ended && !partial.empty()) {
        cut();

        if (!partial.empty()) {
            take(std::move(partial));
        }

        hand();
    }

    const std::scoped_lock held(_guard);

    _closed = true;
}

void RunLog::harvest() {
    std::vector<Line> taken;
    bool lost = false;
    bool closed = false;

    {
        const std::scoped_lock held(_guard);

        taken.swap(_arrived);
        lost = std::exchange(_lost, false);
        closed = _closed;
    }

    if (lost) {
        _generation++;
    }

    _pending.insert(_pending.end(), std::make_move_iterator(taken.begin()),
                    std::make_move_iterator(taken.end()));

    if (closed) {
        release();
        publish();

        if (published) {
            published();
        }

        return;
    }

    if (_pending.empty()) {
        return;
    }

    if (!_active) {
        publish();

        return;
    }

    if (_batch == 0) {
        _batch = _shell->after(BATCH, [this] {
            _batch = 0;

            publish();
        });
    }
}

void RunLog::publish() {
    if (_pending.empty()) {
        return;
    }

    _lines.insert(_lines.end(), std::make_move_iterator(_pending.begin()),
                  std::make_move_iterator(_pending.end()));
    _pending.clear();

    if (_lines.size() > LIMIT) {
        _lines.erase(_lines.begin(),
                     _lines.begin() + static_cast<std::ptrdiff_t>(_lines.size() - LIMIT));
        _generation++;
    }

    if (!_active) {
        _missed = true;

        return;
    }

    if (published) {
        published();
    }
}
