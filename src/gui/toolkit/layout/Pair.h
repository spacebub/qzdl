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

#include "gui/toolkit/Widget.h"
#include "gui/toolkit/layout/Box.h"

namespace toolkit {

// Two things side by side, stacked instead when the page is too narrow for them.
class Pair : public Box {
public:
    explicit Pair(const double widest) : Box(Flow::Row), _widest(widest) {}

    double naturalHeight(Typeface &type, double width) override;

    void arrange(Typeface &type) override;

private:
    double _widest;
};

}
