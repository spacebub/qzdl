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

Box *Pair::cross(const Place where) {
    _rowCross = where;

    return this;
}

// Side by side the two share the row exactly; a third of a pixel either way would
// leave the fields and the switches under them out of line.
void Pair::reflow(const double width) {
    const bool across = width >= _widest;

    setFlow(across ? Flow::Row : Flow::Column);
    Box::cross(across ? _rowCross : Place::Fill);

    size_t shown = 0;

    for (const Ptr &child : children()) {
        if (child->visible()) {
            ++shown;
        }
    }

    const double half = shown > 1
        ? (width - gapTotal()) / static_cast<double>(shown)
        : -1.0;

    for (const Ptr &child : children()) {
        child->fixedWidth = across ? half : -1.0;
    }
}

double Pair::naturalHeight(Typeface &type, const double width) {
    reflow(width);

    return Box::naturalHeight(type, width);
}

void Pair::arrange(Typeface &type) {
    reflow(_box.w);

    Box::arrange(type);
}

}
