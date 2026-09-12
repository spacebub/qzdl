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

#include "gui/services/Notifier.h"

void Notifier::info(const std::string &text, const std::string &title) {
    post(Info, text, title);
}

void Notifier::success(const std::string &text, const std::string &title) {
    post(Success, text, title);
}

void Notifier::warning(const std::string &text, const std::string &title) {
    post(Warning, text, title);
}

void Notifier::error(const std::string &text, const std::string &title) {
    post(Error, text, title);
}

void Notifier::post(const Severity severity, const std::string &text, const std::string &title) {
    while (_messages.size() >= LIMIT) {
        _messages.erase(_messages.begin());
    }

    _messages.push_back(State::Buzz{
        .id = _next++,
        .severity = severity,
        .title = title,
        .body = text,
        .duration = severity == Error
            ? 0
            : 3200 + (static_cast<int>(std::min<size_t>(text.length(), 160)) * 18),
    });

    State::get().touch();
}

void Notifier::dismiss(const int id) {
    const auto found = std::ranges::find_if(_messages, [id](const State::Buzz &each) {
        return each.id == id;
    });

    if (found != _messages.end()) {
        _messages.erase(found);

        State::get().touch();
    }
}
