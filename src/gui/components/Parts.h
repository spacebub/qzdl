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

#include <string>

#include "gui/draw/Theme.h"
#include "gui/toolkit/controls/Label.h"
#include "gui/toolkit/layout/Box.h"

// The two page pieces more than one page builds.
namespace components {

inline toolkit::Label *panelTitle(toolkit::Box *into, const std::string &text) {
    toolkit::Label *made = into->append(std::make_unique<toolkit::Label>(text));

    made->font(Theme::of().headingWeight, Theme::fontMedium)->tone(Theme::of().text);

    return made;
}

// "1 profile", "4 profiles".
inline std::string say(const size_t number, const std::string &thing) {
    return std::to_string(number) + " " + thing + (number == 1 ? "" : "s");
}

}
