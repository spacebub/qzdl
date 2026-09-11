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

#include <functional>
#include <string>

#include "gui/toolkit/controls/Label.h"
#include "gui/toolkit/layout/Box.h"

namespace toolkit {

// A small caption over a value, which may be a path that opens.
class Fact : public Box {
public:
    Fact(std::string label, std::string value);

    void setValue(std::string value);

    Fact *path(bool value = true);
    Fact *onClick(std::string tip, std::function<void()> clicked);

private:
    Label *_caption = nullptr;
    Label *_value = nullptr;
};

}
