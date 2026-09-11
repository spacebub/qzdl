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

#include "gui/toolkit/layout/Box.h"
#include "gui/toolkit/layout/Pair.h"

namespace toolkit {

double Pair::naturalHeight(Typeface &type, const double width) {
    setFlow(width >= _widest ? Flow::Row : Flow::Column);

    return Box::naturalHeight(type, width);
}

void Pair::arrange(Typeface &type) {
    const bool across = _box.w >= _widest;

    setFlow(across ? Flow::Row : Flow::Column);

    // Side by side the two share the row exactly; a third of a pixel either way
    // would leave the fields and the switches under them out of line.
    if (across) {
        size_t shown = 0;

        for (const Ptr &child : children()) {
            if (child->visible()) {
                ++shown;
            }
        }

        if (shown > 1) {
            const double half = (_box.w - gapTotal()) / static_cast<double>(shown);

            for (const Ptr &child : children()) {
                if (child->visible()) {
                    child->fixedWidth = half;
                }
            }
        }
    } else {
        for (const Ptr &child : children()) {
            child->fixedWidth = -1.0;
        }
    }

    Box::arrange(type);
}

}
