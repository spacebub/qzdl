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

namespace toolkit {

// Takes whatever width or height is going and draws nothing.
class Spacer : public Widget {
public:
    explicit Spacer(const double weight = 1.0) { stretch = weight; }

    double naturalWidth(Typeface & /*type*/) override { return fixedWidth >= 0.0 ? fixedWidth : 0.0; }

    double naturalHeight(Typeface & /*type*/, double /*width*/) override {
        return fixedHeight >= 0.0 ? fixedHeight : 0.0;
    }
};

}
