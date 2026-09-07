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

#include "gui/Convert.h"
#include "gui/Notifier.h"

Notifier::Notifier(const ui::Zdl *window)
    : _window(window),
      _messages(std::make_shared<slint::VectorModel<ui::Buzz>>()) {
    _window->global<ui::Notify>().set_messages(_messages);

    _window->global<ui::Notify>().on_dismiss([this](const int id) { dismiss(id); });

    _window->global<ui::Notify>().on_info([this](const slint::SharedString &text) {
        info(Convert::plain(text));
    });

    _window->global<ui::Notify>().on_success([this](const slint::SharedString &text) {
        success(Convert::plain(text));
    });

    _window->global<ui::Notify>().on_warning([this](const slint::SharedString &text) {
        warning(Convert::plain(text));
    });

    _window->global<ui::Notify>().on_error([this](const slint::SharedString &text) {
        error(Convert::plain(text));
    });
}

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
    while (_messages->row_count() >= LIMIT) {
        _messages->erase(0);
    }

    _messages->push_back(ui::Buzz{
        .id = _next++,
        .severity = severity,
        .title = Convert::text(title),
        .body = Convert::text(text),
        .duration = severity == Error
            ? 0
            : 3200 + (static_cast<int>(std::min<size_t>(text.length(), 160)) * 18),
    });
}

void Notifier::dismiss(const int id) {
    for (size_t row = 0; row < _messages->row_count(); row++) {
        if (const auto each = _messages->row_data(row); each && each->id == id) {
            _messages->erase(row);

            return;
        }
    }
}
