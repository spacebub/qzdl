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

#include "gui/dialogs/ConfirmDialog.h"
#include "gui/toolkit/controls/Button.h"
#include "gui/toolkit/layout/Spacer.h"

namespace dialogs {

using namespace toolkit;

ConfirmDialog::ConfirmDialog(const std::string &title, const std::string &said,
                           const std::string &accept, const bool danger,
                           std::function<void()> accepted)
    : _accepted(std::move(accepted)) {
    wanted = 460.0;

    Box *column = card()->append(Box::column());

    column->pad(22.0)->spacing(10.0);

    heading(column, title);
    body(column, said);

    column->append(std::make_unique<Spacer>(0.0))->fixedHeight = 8.0;

    Box *row = column->append(Box::row());

    row->spacing(8.0)->align(Box::Place::End);
    row->fixedHeight = Theme::control;

    row->append(std::make_unique<Button>("Cancel", [this] {
        if (dismissed) {
            dismissed();
        }
    }));

    // Copied out: answering takes the dialog down, and the callable with it.
    row->append(std::make_unique<Button>(accept, [this] {
        const std::function<void()> fire = _accepted;
        const std::function<void()> shut = dismissed;

        if (shut) {
            shut();
        }

        if (fire) {
            fire();
        }
    }))->kind(danger ? Button::Kind::Danger : Button::Kind::Primary);
}

}
