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

#include <algorithm>

#include "gui/toolkit/layout/Wrap.h"

namespace toolkit {

Wrap *Wrap::spacing(const double across, const double down) {
    _across = across;
    _down = down;

    return this;
}

double Wrap::lay(Typeface &type, const double width, const bool place) {
    double x = 0.0;
    double y = 0.0;
    double line = 0.0;

    for (const Ptr &child : children()) {
        if (!child->visible()) {
            continue;
        }

        const double wanted = std::min(width, child->wantedWidth(type));
        const double tall = child->wantedHeight(type, wanted);

        if (x > 0.0 && x + wanted > width) {
            x = 0.0;
            y += line + _down;
            line = 0.0;
        }

        if (place) {
            child->place(BLRect{_box.x + x, _box.y + y, wanted, tall}, type);
        }

        x += wanted + _across;
        line = std::max(line, tall);
    }

    return y + line;
}

double Wrap::naturalWidth(Typeface &type) {
    if (fixedWidth >= 0.0) {
        return fixedWidth;
    }

    // The widest child, not the sum: the layout fits whatever it is given.
    double widest = 0.0;

    for (const Ptr &child : children()) {
        if (child->visible()) {
            widest = std::max(widest, child->wantedWidth(type));
        }
    }

    return widest;
}

double Wrap::naturalHeight(Typeface &type, const double width) {
    return fixedHeight >= 0.0 ? fixedHeight : lay(type, width, false);
}

void Wrap::arrange(Typeface &type) {
    lay(type, _box.w, true);
}

}
