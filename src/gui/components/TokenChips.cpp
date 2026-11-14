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

#include <memory>
#include <string>

#include "ttk/toolkit/controls/Chip.h"

#include "core/launch/Command.h"
#include "gui/components/TokenChips.h"

namespace components {

TokenChips::TokenChips() {
    spacing(2.0, 2.0);

    for (const Command::Token &token : Command::tokens()) {
        append(std::make_unique<ttk::Chip>(std::string(token.text), std::string(token.about)));
    }
}

}
