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

#include <cstdint>
#include <string>
#include <vector>

#include "gui/model/State.h"

// The toasts, oldest first.
class Notifier {
public:
    enum Severity : std::uint8_t {
        Info,
        Success,
        Warning,
        Error,
    };

    void info(const std::string &text, const std::string &title = {});
    void success(const std::string &text, const std::string &title = {});
    void warning(const std::string &text, const std::string &title = {});
    void error(const std::string &text, const std::string &title = {});

    // Errors stay until dismissed.
    void post(Severity severity, const std::string &text, const std::string &title = {});

    void dismiss(int id);

    [[nodiscard]] const std::vector<State::Buzz> &messages() const { return _messages; }

private:
    static constexpr size_t LIMIT = 4;

    std::vector<State::Buzz> _messages;
    int _next{0};
};
