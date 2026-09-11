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

#include "gui/draw/Theme.h"
#include "gui/toolkit/layout/Panel.h"

namespace toolkit {

void Panel::paint(const Painter &painter) {
    const Theme::Palette &palette = Theme::of();

    painter.round(_box, rounding, inset ? palette.sunken : palette.surface);

    if (hoverable && lit) {
        painter.round(_box, rounding, palette.hover);
    }

    if (bordered) {
        painter.outline(_box, rounding, 1.0, lit ? palette.borderStrong : palette.border);
    }

    Widget::paint(painter);
}

}
