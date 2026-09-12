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

#include <utility>

#include "core/util/Text.h"
#include "gui/dialogs/PromptDialog.h"
#include "gui/toolkit/controls/Button.h"

namespace dialogs {

using namespace toolkit;

PromptDialog::PromptDialog(const std::string &title, const std::string &label,
                         std::string value, const std::string &accept,
                         std::function<void(const std::string &)> accepted)
    : _accepted(std::move(accepted)), _value(std::move(value)) {
    wanted = 460.0;

    Box *column = card()->append(Box::column());

    column->pad(22.0)->spacing(16.0);

    heading(column, title);

    _field = column->append(std::make_unique<Field>(label, [this](const std::string &typed) {
        _value = typed;

        _accept->setEnabled(!Text::trim(_value).empty());
    }));

    _field->accepted = [this] { commit(); };
    _field->setText(_value);

    Box *row = column->append(Box::row());

    row->spacing(8.0)->align(Box::Place::End);
    row->fixedHeight = Theme::control;

    row->append(std::make_unique<Button>("Cancel", [this] {
        if (dismissed) {
            dismissed();
        }
    }));

    _accept = row->append(std::make_unique<Button>(accept, [this] { commit(); }));
    _accept->kind(Button::Kind::Primary);
    _accept->setEnabled(!Text::trim(_value).empty());
}

void PromptDialog::opened() {
    _field->takeFocus();
}

void PromptDialog::commit() {
    const std::string tidy = Text::trim(_value);

    if (tidy.empty()) {
        return;
    }

    // Copied out: accepting takes the dialog down, and the callable with it.
    const std::function<void(const std::string &)> fire = _accepted;
    const std::function<void()> shut = dismissed;

    if (shut) {
        shut();
    }

    if (fire) {
        fire(tidy);
    }
}

}
